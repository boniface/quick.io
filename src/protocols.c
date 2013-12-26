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
 * Standard heartbeat interval for QIO
 */
#define HEARTBEAT_INTERVAL QEV_SEC_TO_USEC(60)

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
		.exit = NULL,
	},
	{	.id = 1,
		.handles = protocol_stomp_handles,
		.handshake = protocol_stomp_handshake,
		.route = protocol_stomp_route,
		.heartbeat = protocol_stomp_heartbeat,
		.frame = protocol_stomp_frame,
		.close = NULL,
		.exit = NULL,
	},
	{	.id = 2,
		.handles = protocol_flash_handles,
		.handshake = protocol_flash_handshake,
		.route = protocol_flash_route,
		.heartbeat = NULL,
		.frame = NULL,
		.close = NULL,
		.exit = NULL,
	},
	{	.id = 3,
		.handles = protocol_raw_handles,
		.handshake = protocol_raw_handshake,
		.route = protocol_raw_route,
		.heartbeat = protocol_raw_heartbeat,
		.frame = protocol_raw_frame,
		.close = NULL,
		.exit = NULL,
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
	enum protocol_status status = client->protocol->route(client);

	switch (status) {
		case PROT_OK:
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
}

static void _heartbeat_cb(struct client *client, void *hb_)
{
	struct heartbeat *hb = hb_;

	if (client->protocol != NULL && client->protocol->heartbeat != NULL) {
		client->protocol->heartbeat(client, hb);
	}
}

static void _cleanup()
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		if (_protocols[i].exit != NULL) {
			_protocols[i].exit();
		}
	}
}

void protocols_route(struct client *client)
{
	void *data = NULL;

	client->last_recv = qev_monotonic;

	if (client->protocol == NULL && !_find_handler(client, &data)) {
		return;
	}

	if (!(client->protocol_flags & HANDSHAKED)) {
		_handshake(client, data);
		return;
	}

	_route(client);
}

void protocols_write(struct client *client, const gchar *data, const guint len)
{
	if (client->protocol_flags & HANDSHAKED) {
		client->last_send = qev_monotonic;

		GString *frame = client->protocol->frame(data, len);
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

GString** protocols_bcast(const gchar *e, const guint len)
{
	guint i;
	GString **frames = g_slice_alloc0(sizeof(*frames) * G_N_ELEMENTS(_protocols));

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		struct protocol *p = _protocols + i;
		if (p->frame != NULL) {
			*(frames + i) = p->frame(e, len);
		}
	}

	return frames;
}

void protocols_bcast_write(struct client *client, GString **frames)
{
	GString *frame = *(frames + client->protocol->id);
	if (frame != NULL && (client->protocol_flags & HANDSHAKED)) {
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

void protocols_init()
{
	ASSERT(qev_timer(protocols_heartbeat, cfg_heartbeat_interval, 0),
			"Could not setup timer for heartbeats");
	qev_cleanup_fn(_cleanup);
}
