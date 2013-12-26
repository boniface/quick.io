/**
 * Allows communication with QIO without any framing. In other words,
 * plain text using the QIO protocol.
 * @file qio.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Extends the reasons for closing a client
 */
enum raw_close_reasons {
	/**
	 * The event was improperly formatted
	 */
	RAW_INVALID_EVENT_FORMAT = 1000,

	/**
	 * Heartbeating the client failed, and he's now dead. (In other words,
	 * client is probably closed because there was no response from him
	 * when a heartbeat challenge was sent.)
	 */
	RAW_HEARTATTACK,
};

/**
 * The type of heartbeat to send
 */
enum protocol_raw_hb {
	protocol_raw_hb_none,
	protocol_raw_hb_heartbeat,
	protocol_raw_hb_challenge,
};

/**
 * If flash can handle this client
 */
enum protocol_handles protocol_raw_handles(struct client *client, void **data);

/**
 * To complete the single, simple flash handshake.
 */
enum protocol_status protocol_raw_handshake(struct client *client, void *data);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_raw_route(struct client *client);

/**
 * Sends a heartbeat to a client
 */
void protocol_raw_heartbeat(struct client *client, struct heartbeat *hb);

/**
 * Frames data to send out to a client.
 *
 * @return
 *     A buffer containg the frame. @args{transfer-full}
 */
GString* protocol_raw_frame(const gchar *data, const guint64 len);

/**
 * Checks that the data received is indeed a valid QIO handshake
 */
gboolean protocol_raw_check_handshake(struct client *client);

/**
 * Checks that the data received is indeed a valid QIO handshake
 */
void protocol_raw_do_heartbeat(
	struct client *client,
	struct heartbeat *hb,
	const gchar *heartbeat,
	const guint heartbeat_len);

/**
 * Handles a raw frame from a client. Assumes that client->qev_client.rbuff
 * contains data of length `len` that is an actual message. Once the message
 * has been processed, `frame_len` will be removed from rbuff.
 */
enum protocol_status protocol_raw_handle(
	struct client *client,
	const guint64 len,
	const guint64 frame_len);
