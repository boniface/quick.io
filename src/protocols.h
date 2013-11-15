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

#ifndef QUICKIO_PROTOCOLS_H
#define QUICKIO_PROTOCOLS_H

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
 * What a protocol needs to route messages around
 */
struct protocol {
	/**
	 * Function that determines if the protocol can handle the client
	 *
	 * @param client The client that we're checking
	 * @param data If this protocol can handle the client, this data
	 * will only be passed back into handshake immediately after this call;
	 * all future calls to handshake will receive NULL.
	 */
	enum protocol_handles (*handles)(struct client *client, void **data);

	/**
	 * Completes the handshake with the client. Once a protocol has accepted
	 * a client, this function will be called instead of route until
	 * PROT_OK is returned or the client is killed.
	 *
	 * @param client The client to handshake with
	 * @param data Whatever data came from the previous handles() call; this
	 * will _only_ be set on the first call to handshake() after handles(),
	 * not on any subsequent calls.
	 */
	enum protocol_status (*handshake)(struct client *client, void *data);

	/**
	 * Reads and routes data available on the client
	 */
	enum protocol_status (*route)(struct client *client);

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
 * @param client The client ready for routing
 */
void protocols_route(struct client *client);

/**
 * Notification that a client was closed.
 *
 * @param client The client that closed
 * @param reason What happened to the client
 */
void protocols_closed(struct client *client, guint reason);

/**
 * Setup all protocols and get ready to run.
 */
void protocols_init();

#endif
