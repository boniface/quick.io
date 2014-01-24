/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Determines if the client has completed his handshake with a supported
 * protocol.
 */
#define HANDSHAKED 0x8000

/**
 * Standard heartbeat interval for QIO + 1 seconds. There's a race condition
 * with client->last_send and the heartbeat timer: since it's measured in
 * microseconds, minor variations cause it to suddenly be greater than
 * client->last_send when it really shouldn't. Just give it a slight
 * passing to ensure that heartbeats are really only sent out every 60 seconds.
 */
#define HEARTBEAT_INTERVAL QEV_SEC_TO_USEC(61)

/**
 * How often to send a challenge heartbeat
 */
#define HEARTBEAT_CHALLENGE_INTERVAL QEV_SEC_TO_USEC(60 * 15)

/**
 * How much time must elapse before the client is considered dead.
 */
#define HEARTBEAT_DEAD (HEARTBEAT_CHALLENGE_INTERVAL + QEV_SEC_TO_USEC(60))

/*
 * All of the known protocols, in order of their preference (index 0 being
 * most preferred, N being least).
 */
static struct protocol _protocols[] = {
	{	.id = 0,
		.handles = protocol_rfc6455_handles,
		.handshake = protocol_rfc6455_handshake,
		.route = protocol_rfc6455_route,
		.heartbeat = protocol_rfc6455_heartbeat,
		.frame = protocol_rfc6455_frame,
		.close = protocol_rfc6455_close,
	},
	{	.id = 1,
		.handles = protocol_stomp_handles,
		.handshake = protocol_stomp_handshake,
		.route = protocol_stomp_route,
		.heartbeat = protocol_stomp_heartbeat,
		.frame = protocol_stomp_frame,
		.close = NULL,
	},
	{	.id = 2,
		.handles = protocol_flash_handles,
		.handshake = protocol_flash_handshake,
		.route = NULL,
		.heartbeat = NULL,
		.frame = NULL,
		.close = NULL,
	},
	{	.id = 3,
		.handles = protocol_raw_handles,
		.handshake = protocol_raw_handshake,
		.route = protocol_raw_route,
		.heartbeat = protocol_raw_heartbeat,
		.frame = protocol_raw_frame,
		.close = NULL,
	},
};

static gboolean _find_handler(struct client *client, void **data)
{
	guint i;
	gboolean supported = FALSE;

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		enum protocol_handles handles = _protocols[i].handles(client, data);

		switch (handles) {
			case PROT_YES:
				client->protocol = _protocols + i;
				return TRUE;

			case PROT_MAYBE:
				supported = TRUE;
				break;

			case PROT_NO:
				break;
		}
	}

	if (!supported) {
		qev_close(client, QIO_CLOSE_NOT_SUPPORTED);
	}

	return FALSE;
}

static void _handshake(struct client *client, void *data)
{
	enum protocol_status status = client->protocol->handshake(client, data);

	switch (status) {
		case PROT_OK:
			client->last_send = qev_monotonic;
			client->protocol_flags |= HANDSHAKED;
			qev_timeout_clear(&client->timeout);
			break;

		case PROT_AGAIN:
			break;

		case PROT_FATAL:
			qev_close(client, QIO_CLOSE_NOT_SUPPORTED);
			break;
	}
}

static void _route(struct client *client)
{
	enum protocol_status status = PROT_FATAL;

	do {
		if (client->protocol->route != NULL) {
			status = client->protocol->route(client);
		}

		switch (status) {
			case PROT_OK:
				client->last_recv = qev_monotonic;

				/*
				 * This isn't too elegant, but this timeout is only ever set
				 * on connect and in the read thread, so if it's not set,
				 * this little check will save some.
				 */
				if (client->timeout != NULL) {
					qev_timeout_clear(&client->timeout);
				}
				break;

			case PROT_AGAIN:
				qev_timeout(client, &client->timeout);
				break;

			case PROT_FATAL:
				qev_close(client, QIO_CLOSE_UNKNOWN_ERROR);
				break;
		}
	} while (status == PROT_OK && client->qev_client.rbuff->len > 0);
}

static void _heartbeat_cb(struct client *client, void *hb_)
{
	struct heartbeat *hb = hb_;

	if (client->protocol != NULL && client->protocol->heartbeat != NULL) {
		client->protocol->heartbeat(client, hb);
	}
}

void protocols_route(struct client *client)
{
	void *data = NULL;

	client->last_recv = qev_monotonic;

	if (client->protocol == NULL && !_find_handler(client, &data)) {
		return;
	}

	if (!protocols_client_handshaked(client)) {
		_handshake(client, data);
		return;
	}

	_route(client);
}

void protocols_send(
	struct client *client,
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json)
{
	if (protocols_client_handshaked(client)) {
		client->last_send = qev_monotonic;

		ev_extra = ev_extra ? : "";
		GString *frame = client->protocol->frame(ev_path, ev_extra,
												server_cb, json);
		qev_write(client, frame->str, frame->len);
		qev_buffer_put(frame);
	}
}

void protocols_closed(struct client *client, guint reason)
{
	if (client->protocol != NULL && client->protocol->close != NULL) {
		client->protocol->close(client, reason);
	}
}

GString** protocols_bcast(const gchar *ev_path, const gchar *json)
{
	guint i;
	GString **frames = g_slice_alloc0(sizeof(*frames) * G_N_ELEMENTS(_protocols));

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		struct protocol *p = _protocols + i;
		if (p->frame != NULL) {
			*(frames + i) = p->frame(ev_path, "", EVS_NO_CALLBACK, json);
		}
	}

	return frames;
}

void protocols_bcast_write(struct client *client, GString **frames)
{
	GString *frame = *(frames + client->protocol->id);
	if (frame != NULL && protocols_client_handshaked(client)) {
		client->last_send = qev_monotonic;
		qev_write(client, frame->str, frame->len);
	}
}

void protocols_bcast_free(GString **frames)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		GString *f = *(frames + i);
		if (f != NULL) {
			qev_buffer_put(f);
		}
	}

	g_slice_free1(sizeof(*frames) * G_N_ELEMENTS(_protocols), frames);
}

void protocols_heartbeat()
{
	gint64 now = qev_monotonic;

	struct heartbeat hb = {
		.heartbeat = now - HEARTBEAT_INTERVAL +
						QEV_SEC_TO_USEC(cfg_heartbeat_interval),
		.challenge = now - HEARTBEAT_CHALLENGE_INTERVAL,
		.dead = now - HEARTBEAT_DEAD,
	};

	qev_foreach(_heartbeat_cb, cfg_heartbeat_threads, &hb);
}

gboolean protocols_client_handshaked(struct client *client)
{
	return client->protocol_flags & HANDSHAKED;
}

void protocols_init()
{
	qev_timer(protocols_heartbeat, cfg_heartbeat_interval, 0);
}
