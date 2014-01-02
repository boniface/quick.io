/**
 * Manages all of the protocols that clients may speak.
 * @file protocols.h
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
 * If a protocol can handle a client
 */
enum protocol_handles {
	/**
	 * Can definitely handle the client
	 */
	PROT_YES,

	/**
	 * Might be able to handle the client, need more data to see
	 */
	PROT_MAYBE,

	/**
	 * Definitely cannot handle the client
	 */
	PROT_NO,
};

/**
 * Responses from the routers
 */
enum protocol_status {
	/**
	 * Message was handled and routed correctly
	 */
	PROT_OK,

	/**
	 * Message has not been completely delivered yet, try again with
	 * more data
	 */
	PROT_AGAIN,

	/**
	 * The client was killed by the protocol and will not be handled
	 */
	PROT_FATAL,
};

/**
 * Useful information about heartbeat timings.
 */
struct heartbeat {
	/**
	 * If client->last_send is less than this, needs a simple heartbeat sent.
	 */
	gint64 heartbeat;

	/**
	 * If client->last_recv is less than this, needs a hearbeat challenge.
	 */
	gint64 challenge;

	/**
	 * If client->last_recv is less than this, the client is just dead.
	 */
	gint64 dead;
};

/**
 * What a protocol needs to route messages around
 */
struct protocol {
	/**
	 * Useful for broadcasts
	 */
	guint id;

	/**
	 * Function that determines if the protocol can handle the client
	 *
	 * @param client
	 *     The client that we're checking
	 * @param data
	 *     If this protocol can handle the client, this data
	 *     will only be passed back into handshake immediately after this call;
	 *     all future calls to handshake will receive NULL.
	 */
	enum protocol_handles (*handles)(struct client *client, void **data);

	/**
	 * Completes the handshake with the client. Once a protocol has accepted
	 * a client, this function will be called instead of route until
	 * PROT_OK is returned or the client is killed.
	 *
	 * @param client
	 *     The client to handshake with
	 * @param data
	 *     Whatever data came from the previous handles() call; this
	 *     will _only_ be set on the first call to handshake() after handles(),
	 *     not on any subsequent calls.
	 */
	enum protocol_status (*handshake)(struct client *client, void *data);

	/**
	 * Reads and routes data available on the client
	 */
	enum protocol_status (*route)(struct client *client);

	/**
	 * Send a heartbeat to a client, if necessary
	 */
	void (*heartbeat)(struct client *client, struct heartbeat *hb);

	/**
	 * Frames the data in whatever the protocol dictates such that it
	 * can be directly written via qev_write().
	 */
	GString* (*frame)(
		const gchar *ev_path,
		const gchar *ev_extra,
		const evs_cb_t server_cb,
		const gchar *json);

	/**
	 * Sends a final farewell message to clients before they close.
	 */
	void (*close)(struct client *client, guint reason);

	/**
	 * Tears down the protocol and everything around it.
	 */
	void (*exit)();
};

/**
 * Route a message from a client to the correct protocol handler
 *
 * @param client
 *     The client ready for routing
 */
void protocols_route(struct client *client);

/**
 * Write data out to a client, properly framing it as the protocol
 * requires.
 *
 * @param client
 *     The client to write to
 * @param ev_path
 *     The path of the event
 * @param ev_extra
 *     Any extra path segments
 * @param server_cb
 *     The callback expected on the server
 * @param json
 *     Data to send with the event
 */
void protocols_send(
	struct client *client,
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json);

/**
 * Notification that a client was closed.
 *
 * @param client
 *     The client that closed
 * @param reason
 *     What happened to the client
 */
void protocols_closed(struct client *client, guint reason);

/**
 * Get an array of framed events, one for each protocol.
 *
 * @param ev_path
 *     The path of the event
 * @param json
 *     Data to send
 *
 * @return
 *     The frames that can be passed to protocols_bcast_write() to send
 *     to a client. Must be freed with protocols_bcast_free() when done.
 */
GString** protocols_bcast(const gchar *ev_path, const gchar *json);

/**
 * Writes the broadcast to the given client
 */
void protocols_bcast_write(struct client *client, GString **frames);

/**
 * Frees up the frames allocated by protocols_bcast().
 */
void protocols_bcast_free(GString **frames);

/**
 * Sends out heartbeats to everyone who needs them right now.
 */
void protocols_heartbeat();

/**
 * Determine if a client has completely finished his handshake.
 */
gboolean protocols_client_handshaked(struct client *client);

/**
 * Setup all protocols and get ready to run.
 */
void protocols_init();
