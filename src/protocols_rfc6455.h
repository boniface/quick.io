/**
 * The QuickIO protocol over websockets (rfc6455).
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
 * Extends the reasons for closing a client
 */
enum rfc6455_close_reasons {
	/**
	 * Client didn't ask for upgrade
	 */
	RFC6455_MISSING_UPGRADE = 3000,

	/**
	 * Client didn't mask his message
	 */
	RFC6455_NO_MASK,

	/**
	 * Client sent an unsupported opcode
	 */
	RFC6455_UNSUPPORTED_OPCODE,

	/**
	 * Client sent data that was not UTF8
	 */
	RFC6455_NOT_UTF8,
};

/**
 * This protocol's functions.
 */
struct protocol *protocol_rfc6455;

/**
 * Sets up everything to run
 */
void protocol_rfc6455_init();

/**
 * If rfc6455 can handle this client
 */
enum protocol_handles protocol_rfc6455_handles(struct client *client);

/**
 * To do the rfc6455 handshake
 */
enum protocol_status protocol_rfc6455_handshake(struct client *client);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_rfc6455_route(struct client *client, gsize *used);

/**
 * Sends a heartbeat to a client
 */
void protocol_rfc6455_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb);

/**
 * Frames data to send out to a client.
 */
struct protocol_frames protocol_rfc6455_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Terminates all communications with the client
 */
void protocol_rfc6455_close(struct client *client, guint reason);

/**
 * Upgrade a client to RFC6455, sending out all necessary headers.
 *
 * @param client
 *     The client to upgrade
 * @param key
 *     Sec-WebSocket-Key from the HTTP Headers
 */
void protocol_rfc6455_upgrade(struct client *client, const gchar *key);
