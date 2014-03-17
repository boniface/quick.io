/**
 * Provides support for HTTP long polling.
 * @file http.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Extends the reasons for closing a client
 */
enum http_close_reasons {
	/**
	 * Client looked like it wanted to upgrade to WebSocket but was missing
	 * the proper upgrade headers.
	 */
	HTTP_FAILED_UPGRADE = 2000,

	/**
	 * Client had proper upgrade headers but isn't ready to speak our
	 * dialect/protocol of WebSocket.
	 */
	HTTP_FAILED_WEBSOCKET_UPGRADE,
};

/**
 * This protocol's functions.
 */
struct protocol *protocol_http;

/**
 * Sets up everything to run
 */
void protocol_http_init();

/**
 * If the client really wants to HTTP Poll
 */
enum protocol_handles protocol_http_handles(struct client *client);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_http_route(struct client *client, gsize *used);

/**
 * Sends a heartbeat to a client
 */
void protocol_http_heartbeat(struct client *client, struct heartbeat *hb);

/**
 * Frames data to send out to a client.
 *
 * @return
 *     A buffer containing the frame. @args{transfer-full}
 */
GString* protocol_http_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Terminates all communications with the client
 */
void protocol_http_close(struct client *client, guint reason);
