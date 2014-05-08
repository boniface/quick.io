/**
 * Manages all of the protocols that clients may speak.
 * @file protocols.h
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
 * Full heartbeat event that is fired to clients
 */
#define PROTOCOLS_HEARTBEAT "/qio/heartbeat:0=null"

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
 * Responses from the route() functions
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
struct protocol_heartbeat {
	/**
	 * If client->last_send is less than this, then the client can be
	 * considered as timed out of the protocol chose to implement timeouts
	 * using heartbeats instead of qev_timeout().
	 */
	gint64 timeout;

	/**
	 * If client->last_send is less than this, the poll should be completed
	 * so that a new iteration can be started.
	 */
	gint64 poll;

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
 * Some protocols (HTTP, looking at you) need a few frames types depending
 * on the type of client they're immediately sending to, so those possiblities
 * are stored here and exposed to all protocols.
 */
struct protocol_frames {
	/**
	 * The default frame. If no special handlers are implemented, this is used
	 */
	GString *def;

	/**
	 * A raw frame. Contains whatever raw means to the protocol.
	 */
	GString *raw;
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
	 * Reference to the global variable that references this protocol.
	 */
	struct protocol **global;

	/**
	 * Sets up the protocol to run.
	 */
	void (*init)();

	/**
	 * Function that determines if the protocol can handle the client
	 *
	 * @param client
	 *     The client that we're checking
	 */
	enum protocol_handles (*handles)(struct client *client);

	/**
	 * Completes the handshake with the client. Once a protocol has accepted
	 * a client, this function will be called instead of route until
	 * PROT_OK is returned or the client is killed.
	 *
	 * @param client
	 *     The client to handshake with
	 */
	enum protocol_status (*handshake)(struct client *client);

	/**
	 * Reads and routes data available on the client
	 */
	enum protocol_status (*route)(struct client *client, gsize *used);

	/**
	 * Send a heartbeat to a client, if necessary
	 */
	void (*heartbeat)(struct client *client, const struct protocol_heartbeat *hb);

	/**
	 * Frames the data in whatever the protocol dictates such that it
	 * can be directly written via qev_write().
	 */
	struct protocol_frames (*frame)(
		const gchar *ev_path,
		const gchar *ev_extra,
		const evs_cb_t server_cb,
		const gchar *json);

	/**
	 * Send a frame to a client.
	 */
	void (*send)(struct client *client, const struct protocol_frames *frames);

	/**
	 * Sends a final farewell message to clients before they close.
	 */
	void (*close)(struct client *client, guint reason);
};

/**
 * Create a new surrogate client bound to the given protocol.
 *
 * @param prot
 *     The protocol to bind the new client to.
 *
 * @return
 *     A quick-event surrogate client bound to the protocol.
 */
struct client* protocols_new_surrogate(struct protocol *prot);

/**
 * Route a message from a client to the correct protocol handler
 *
 * @param client
 *     The client ready for routing
 */
void protocols_route(struct client *client);

/**
 * Write data out to a client, properly framing it as the protocol
 * requires. No data validation is done; anything that is passed is
 * assumed to be valid. Use evs_* functions for friendlier, validated,
 * and more idomatic ways of sending messages.
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
 *     Data to send with the event. MUST be a properly-formatted JSON string.
 *     @arg{not-null}
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
struct protocol_frames* protocols_bcast(const gchar *ev_path, const gchar *json);

/**
 * Writes the broadcast to the given client
 */
void protocols_bcast_write(
	struct client *client,
	const struct protocol_frames *frames);

/**
 * Frees up the frames allocated by protocols_bcast().
 */
void protocols_bcast_free(struct protocol_frames *frames);

/**
 * Run any necessary heartbeating on the given client.
 */
void protocols_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb);

/**
 * Get the heartbeat intervals that should be used in this round of periodic
 * tasks.
 */
struct protocol_heartbeat protocols_heartbeat_get_intervals();

/**
 * Switch a client to a different protocol.
 */
void protocols_switch(struct client *client, struct protocol *prot);

/**
 * Setup all protocols and get ready to run.
 */
void protocols_init();
