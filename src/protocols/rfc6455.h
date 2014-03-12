/**
 * The QuickIO protocol over websockets (rfc6455).
 * @file rfc6455.h
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
enum rfc6455_close_reasons {
	/**
	 * Client didn't ask for upgrade
	 */
	RFC6455_MISSING_UPGRADE = 2000,

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
enum protocol_status protocol_rfc6455_route(struct client *client);

/**
 * Sends a heartbeat to a client
 */
void protocol_rfc6455_heartbeat(struct client *client, struct heartbeat *hb);

/**
 * Frames data to send out to a client.
 *
 * @return
 *     A buffer containg the frame. @args{transfer-full}
 */
GString* protocol_rfc6455_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Terminates all communications with the client
 */
void protocol_rfc6455_close(struct client *client, guint reason);
