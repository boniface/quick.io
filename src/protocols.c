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

/*
 * All of the known protocols, in order of their preference (index 0 being
 * most preferred, N being least).
 */
static struct protocol _protocols[] ={
	{	.handles = protocol_rfc6455_handles,
		.handshake = protocol_rfc6455_handshake,
		.route = protocol_rfc6455_route,
		.close = protocol_rfc6455_close,
		.exit = NULL,
	},
	{	.handles = protocol_stomp_handles,
		.handshake = protocol_stomp_handshake,
		.route = protocol_stomp_route,
		.close = NULL,
		.exit = NULL,
	},
	{	.handles = protocol_flash_handles,
		.handshake = protocol_flash_handshake,
		.route = protocol_flash_route,
		.close = NULL,
		.exit = NULL,
	},
	{	.handles = protocol_raw_handles,
		.handshake = protocol_raw_handshake,
		.route = protocol_raw_route,
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
			qev_timeout_clear(client, &client->timeout);
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
				qev_timeout_clear(client, &client->timeout);
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

	if (client->protocol == NULL && !_find_handler(client, &data)) {
		return;
	}

	if (!(client->protocol_flags & HANDSHAKED)) {
		_handshake(client, data);
		return;
	}

	_route(client);
}

void protocols_closed(struct client *client, guint reason)
{
	if (client->protocol != NULL && client->protocol->close != NULL) {
		client->protocol->close(client, reason);
	}
}

void protocols_init()
{
	qev_cleanup_fn(_cleanup);
}
