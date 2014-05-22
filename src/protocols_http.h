/**
 * Provides support for HTTP long polling.
 * @file
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#pragma once
#include "quickio.h"

/**
 * Completely disables caching for HTTP requests
 */
#define HTTP_NOCACHE \
	"Cache-Control: private, max-age=0\r\n" \
	"Expires: -1\r\n"

/**
 * Headers that are common to each response
 */
#define HTTP_COMMON \
	HTTP_NOCACHE \
	"Connection: Keep-Alive\r\n" \
	"Keep-Alive: timeout=60\r\n"

/**
 * Extends the reasons for closing a client
 */
enum http_close_reasons {
	/**
	 * Client closed according to the HTTP standard.
	 */
	HTTP_DONE = 2000,

	/**
	 * No surrogate was found for the client. That's a 403.
	 */
	HTTP_NO_SURROGATE,

	/**
	 * Sometimes was woefully wrong with the request
	 */
	HTTP_BAD_REQUEST,

	/**
	 * Client didn't send length
	 */
	HTTP_LENGTH_REQUIRED,
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
void protocol_http_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb);

/**
 * Frames data to send out to a client.
 *
 * @return
 *     A buffer containing the frame. @arg{transfer-full}
 */
struct protocol_frames protocol_http_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * HTTP uses surrogates, so it has its own sending mechanisms to make
 * sure messages are routed to the right clients
 */
void protocol_http_send(
	struct client *client,
	const struct protocol_frames *pframes);

/**
 * Terminates all communications with the client
 */
void protocol_http_close(struct client *client, guint reason);
