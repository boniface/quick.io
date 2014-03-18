 /**
 * Allows communication with QIO without any framing. In other words,
 * plain text using the QIO protocol.
 * @file qio.h
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
 * This protocol's functions.
 */
struct protocol *protocol_raw;

/**
 * Sets up everything to run
 */
void protocol_raw_init();

/**
 * If flash can handle this client
 */
enum protocol_handles protocol_raw_handles(struct client *client);

/**
 * To complete the single, simple flash handshake.
 */
enum protocol_status protocol_raw_handshake(struct client *client);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_raw_route(struct client *client, gsize *used);

/**
 * Sends a heartbeat to a client
 */
void protocol_raw_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb);

/**
 * Frames data to send out to a client.
 *
 * @return
 *     A buffer containing the frame. @args{transfer-full}
 */
struct protocol_frames protocol_raw_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Formats an event into the QIO protocol
 *
 * @param ev_path
 *     Obviously the path
 * @param ev_extra
 *     Extra path segments
 * @param server_cb
 *     The callback the server expects
 * @param json
 *     Data to send
 *
 * @return
 *     A buffer containing the formatted event. @args{transfer-full}
 */
GString* protocol_raw_format(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Checks that the data received is indeed a valid QIO handshake
 */
gboolean protocol_raw_check_handshake(struct client *client);

/**
 * Checks that the data received is indeed a valid QIO handshake
 */
void protocol_raw_do_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb,
	const gchar *heartbeat,
	const gsize heartbeat_len);

/**
 * Handles a raw frame from a client.
 *
 * @param client
 *     The client that sent the event
 * @param event
 *     The raw event from the client. MUST be NULL-terminated.
 */
enum protocol_status protocol_raw_handle(struct client *client, gchar *event);
