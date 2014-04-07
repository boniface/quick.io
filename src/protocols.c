/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * For protocols that poll, the maximum amount of time a socket may
 * be left idle
 */
#define HEARTBEAT_POLL QEV_SEC_TO_USEC(55)

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
	{	.global = &protocol_http,
		.init = protocol_http_init,
		.handles = protocol_http_handles,
		.handshake = NULL,
		.route = protocol_http_route,
		.heartbeat = protocol_http_heartbeat,
		.frame = protocol_http_frame,
		.send = protocol_http_send,
		.close = protocol_http_close,
	},
	{	.global = &protocol_flash,
		.init = protocol_flash_init,
		.handles = protocol_flash_handles,
		.handshake = protocol_flash_handshake,
		.route = NULL,
		.heartbeat = NULL,
		.frame = NULL,
		.send = NULL,
		.close = NULL,
	},
	{	.global = &protocol_raw,
		.init = protocol_raw_init,
		.handles = protocol_raw_handles,
		.handshake = protocol_raw_handshake,
		.route = protocol_raw_route,
		.heartbeat = protocol_raw_heartbeat,
		.frame = protocol_raw_frame,
		.send = NULL,
		.close = NULL,
	},
	{	.global = &protocol_rfc6455,
		.init = protocol_rfc6455_init,
		.handles = protocol_rfc6455_handles,
		.handshake = protocol_rfc6455_handshake,
		.route = protocol_rfc6455_route,
		.heartbeat = protocol_rfc6455_heartbeat,
		.frame = protocol_rfc6455_frame,
		.send = NULL,
		.close = protocol_rfc6455_close,
	},
};

static gboolean _find_handler(struct client *client)
{
	guint i;
	gboolean supported = FALSE;

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		enum protocol_handles handles = _protocols[i].handles(client);

		switch (handles) {
			case PROT_YES:
				client->protocol.prot = _protocols + i;
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

static void _send(struct client *client, const struct protocol_frames *pframes)
{
	client->last_send = qev_monotonic;

	if (client->protocol.prot->send) {
		client->protocol.prot->send(client, pframes);
	} else if (pframes->def != NULL) {
		qev_write(client, pframes->def->str, pframes->def->len);
	}
}

static void _set_handshaked(struct client *client)
{
	client->last_send = qev_monotonic;
	client->protocol.handshaked = TRUE;
	qev_timeout_clear(&client->timeout);
}

static void _handshake(struct client *client)
{
	enum protocol_status status = client->protocol.prot->handshake(client);

	switch (status) {
		case PROT_OK:
			_set_handshaked(client);
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
	gsize used = 0;
	enum protocol_status status = PROT_FATAL;
	GString *rbuff = client->qev_client.rbuff;

	do {
		if (client->protocol.prot->route != NULL) {
			status = client->protocol.prot->route(client, &used);
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
				goto out;

			case PROT_FATAL:
				qev_close(client, QIO_CLOSE_UNKNOWN_ERROR);
				goto out;
		}
	} while (used < rbuff->len);

out:
	g_string_erase(rbuff, 0, used);
}

struct client* protocols_new_surrogate(struct protocol *prot)
{
	struct client *client = qev_surrogate_new();

	if (client != NULL) {
		client->protocol.prot = prot;
		_set_handshaked(client);
	}

	return client;
}

void protocols_route(struct client *client)
{
	client->last_recv = qev_monotonic;

	if (client->protocol.prot == NULL && !_find_handler(client)) {
		return;
	}

	if (!client->protocol.handshaked) {
		if (client->protocol.prot->handshake == NULL) {
			_set_handshaked(client);
		} else {
			_handshake(client);
			return;
		}
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
	if (client->protocol.handshaked) {
		ev_extra = ev_extra ? : "";

		struct protocol_frames pframes = client->protocol.prot->frame(
										ev_path, ev_extra, server_cb, json);
		_send(client, &pframes);

		qev_buffer_put(pframes.def);
		qev_buffer_put(pframes.raw);
	}
}

void protocols_closed(struct client *client, guint reason)
{
	if (client->protocol.prot != NULL && client->protocol.prot->close != NULL) {
		client->protocol.prot->close(client, reason);
	}

	client->protocol_flags = 0;
}

struct protocol_frames* protocols_bcast(const gchar *ev_path, const gchar *json)
{
	guint i;
	struct protocol_frames *frames = g_slice_alloc0(sizeof(*frames) * G_N_ELEMENTS(_protocols));

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		struct protocol *p = _protocols + i;
		if (p->frame != NULL) {
			*(frames + i) = p->frame(ev_path, "", EVS_NO_CALLBACK, json);
		}
	}

	return frames;
}

void protocols_bcast_write(
	struct client *client,
	const struct protocol_frames *frames)
{
	const struct protocol_frames *pframes = frames + client->protocol.prot->id;
	if (client->protocol.handshaked) {
		_send(client, pframes);
	}
}

void protocols_bcast_free(struct protocol_frames *frames)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		struct protocol_frames *frame = frames + i;
		qev_buffer_put(frame->def);
		qev_buffer_put(frame->raw);
	}

	g_slice_free1(sizeof(*frames) * G_N_ELEMENTS(_protocols), frames);
}

void protocols_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb)
{
	if (client->protocol.handshaked &&
		client->protocol.prot->heartbeat != NULL) {

		client->protocol.prot->heartbeat(client, hb);
	}
}

struct protocol_heartbeat protocols_heartbeat_get_intervals()
{
	gint64 now = qev_monotonic;

	return (struct protocol_heartbeat){
		.timeout = now - QEV_MS_TO_USEC(qev_cfg_get_timeout()),
		.poll = now - HEARTBEAT_POLL + QEV_SEC_TO_USEC(cfg_periodic_interval),
		.heartbeat = now - HEARTBEAT_INTERVAL +
						QEV_SEC_TO_USEC(cfg_periodic_interval),
		.challenge = now - HEARTBEAT_CHALLENGE_INTERVAL,
		.dead = now - HEARTBEAT_DEAD,
	};
}

void protocols_switch(struct client *client, struct protocol *prot)
{
	client->protocol.prot = prot;
	client->protocol.handshaked = FALSE;
	memset(&client->protocol.flags, 0, sizeof(client->protocol.flags));
}

void protocols_init()
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(_protocols); i++) {
		struct protocol *p = _protocols + i;
		p->id = i;
		*p->global = p;
		if (p->init != NULL) {
			p->init(p);
		}
	}
}
