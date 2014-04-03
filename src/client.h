/**
 * Provides all utility client functions.
 * @file client.h
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
 * Client subscriptions go through multiple states, explained here.
 */
enum client_sub_state {
	/**
	 * The subscription already exists and is in the pending state (probably
	 * went async with verification).
	 */
	CLIENT_SUB_PENDING,

	/**
	 * The subscription exists and is going to be removed, but evs_on_cb()
	 * hasn't been called by the owner, so removal is being delayed until then.
	 */
	CLIENT_SUB_TOMBSTONED,

	/**
	 * The subscription was created and is in the pending state.
	 */
	CLIENT_SUB_CREATED,

	/**
	 * The subscription is active and valid.
	 */
	CLIENT_SUB_ACTIVE,

	/**
	 * The client has no such subscription.
	 */
	CLIENT_SUB_NULL,
};

/**
 * Information about the client's subscription to an event.
 */
struct client_sub {
	/**
	 * Where the client lives in the sub list
	 */
	gint32 idx;

	/**
	 * If the subscription is pending: the on() callback went async
	 */
	gboolean pending;

	/**
	 * If an unsubscribe came in before evs_on_cb() came through
	 */
	gboolean tombstone;
};

/**
 * Information necessary to fire a callback
 */
struct client_cb {
	/**
	 * To make sure that callbacks don't overlap.
	 */
	guint16 id;

	/**
	 * The function to be called
	 */
	evs_cb_fn cb_fn;

	/**
	 * Data to pass to the function
	 */
	void *cb_data;

	/**
	 * Function used to free the cb_data
	 */
	qev_free_fn free_fn;

	/**
	 * When the callback was created (monotonic time)
	 */
	gint64 created;
};

/**
 * Everything that a client needs to function
 */
struct client {
	/**
	 * All for quick-event
	 */
	struct qev_client qev_client;

	/**
	 * What's needed to handle protocol support in the client
	 */
	struct {
		/**
		 * If the client has handshaked with its protocol.
		 */
		gboolean handshaked:1;

		/**
		 * Once a protocol has accepted the client, this will point to the
		 * protocol-level functions that can be used.
		 */
		struct protocol *prot;

		/**
		 * Any protocol-level flags should be created here as a union of
		 * bit-field structs.
		 */
		union {

		} flags;
	} protocol;

	/**
	 * The state of the client at the protocol level. Protocols may only
	 * touch this once they have signaled that they handle the client.
	 *
	 * @note
	 *     Protocols may only use bits 0-14, bit 15 (identified by 0x8000) is
	 *     reserved for the protocol router.
	 */
	guint16 protocol_flags;

	/**
	 * When anything was last sent to the client
	 */
	gint64 last_send;

	/**
	 * When anything was last received from the client.
	 */
	gint64 last_recv;

	/**
	 * The only timeout that will be set on a client from QIO.
	 *
	 * @attention
	 *     This MAY ONLY ever be set on connect and in the protocols.
	 *     It may never be altered anywhere else.
	 */
	qev_timeout_t *timeout;

	/**
	 * Maps: struct subscription -> gint32 (qev_list index)
	 */
	GHashTable *subs;

	/**
	 * For managing the HTTP session for the client
	 */
	struct {
		/**
		 * The key in http's client table.
		 */
		__uint128_t sid;

		/**
		 * Which table the client lives in
		 */
		guint tbl;

		/**
		 * How much data is left to read from the socket
		 */
		guint64 body_len;

		/**
		 * If you can't figure out what this is, you should put the magic
		 * box down.
		 */
		struct {
			/**
			 * If the client sent a POST
			 */
			gboolean is_post:1;

			/**
			 * If the client requested the iframe
			 */
			gboolean iframe_requested:1;

			/**
			 * If the client is keep-alive
			 */
			gboolean keep_alive:1;

			/**
			 * If headers have been received and the client currently processing
			 * a request.
			 */
			gboolean in_request:1;

			/**
			 * A new request just came in and is being processed; don't send
			 * out any updates until the new request's entire body has been
			 * processed.
			 *
			 * This is for surrogates.
			 */
			gboolean incoming:1;
		} flags;

		/**
		 * Can be 1 of 3 things, depending on client type:
		 *   1) If the client is a surrogate, then points to the client
		 *      with an open socket that is waiting on data for the surrogate
		 *   2) If the client has a socket, then points to the surrogate client
		 *   3) Just a NULL
		 *
		 * Must be free'd with qev_unref().
		 */
		struct client *client;
	} http;

	/**
	 * The callbacks for the client.
	 */
	struct client_cb *cbs[4];

	/**
	 * The ID of the most recent callback
	 */
	guint16 cbs_id;
};

/**
 * Creates a callback for the client.
 * If cb_fn is NULL and cb_data is not, it is cleaned up with free_fn, and
 * EVS_NO_CALLBACK is returned.
 *
 * @param client
 *     The client to create a callback for
 * @param cb_fn
 *     The function to call when the client triggers the callback. @arg{not-null}
 * @param cb_data
 *     Data to come with the callback @arg{transfer-full}
 * @param free_fn
 *     Frees cb_data
 *
 * @return
 *     The callback ID to give to the client. EVS_NO_CALLBACK if there's an
 *     error.
 */
evs_cb_t client_cb_new(
	struct client *client,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn);

/**
 * Fires a callback and frees up everything associated with it.
 *
 * @param client
 *     The client to create a callback for
 * @param server_cb
 *     The callback ID the client sent back for the server.
 * @param client_cb
 *     The id of the callback to send to the client
 * @param json
 *     Data sent with the callback
 */
enum evs_status client_cb_fire(
	struct client *client,
	const evs_cb_t server_cb,
	const evs_cb_t client_cb,
	gchar *json);

/**
 * Prune any client callbacks that have been hanging around too long.
 */
void client_cb_prune(struct client *client, const gint64 before);

/**
 * Get the time before which callbacks should be pruned.
 */
gint64 client_cb_prune_get_before();

/**
 * Checks if a client is subscribed to the given susbcription.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @arg{transfer-none}
 *
 * @return
 *     If the client has the subscription and it's active.
 */
gboolean client_sub_active(struct client *client, struct subscription *sub);

/**
 * Adds the subscription to the client.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @arg{transfer-full}
 *
 * @return CLIENT_SUB_PENDING
 *     When a previous subscription is still pending
 * @return CLIENT_SUB_NULL
 *     When the client has too many subscriptions and needs to enhance its calm.
 * @return CLIENT_SUB_ACTIVE
 *     The client is already subscribed to this event, and the event is active.
 * @return CLIENT_SUB_CREATED
 *     Sub has been accepted and is in the pending state, waiting for a call to
 *     client_sub_accept()/reject().
 *
 */
enum client_sub_state client_sub_add(
	struct client *client,
	struct subscription *sub);

/**
 * Marks the subscription as accepted.
 *
 * @param client
 *     The client to accept the subscription on
 * @param sub
 *     The subscription to accept
 *
 * @return CLIENT_SUB_TOMBSTONED
 *     Since it's possible for a client to unsubscribe from an event before
 *     a subscription succeeds, it's possible for approval to fail. If this is
 *     returned, only the off callback need be fired; the sub is automaticall
 *     removed from the client internally.
 * @return CLIENT_SUB_NULL
 *     There was an error setting up the subscription: the client should be
 *     told to enhance his calm
 * @return CLIENT_SUB_ACTIVE
 *     Everything went as planned
 */
enum client_sub_state client_sub_accept(
	struct client *client,
	struct subscription *sub);

/**
 * Removes a pending subscription
 *
 * @param client
 *     The client to reject the subscription on
 * @param sub
 *     The subscription to reject
 */
void client_sub_reject(
	struct client *client,
	struct subscription *sub);

/**
 * Removes the subscription from the client.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @arg{transfer-none}
 *
 * @return
 *     If the subscription was removed.
 */
gboolean client_sub_remove(struct client *client, struct subscription *sub);

/**
 * For configuration values: update the fair pool for subs
 *
 * @param total
 *     The new total to use
 * @param fairness
 *     The new fairness value to use
 */
void client_update_subs_config(const guint64 total, const guint64 fairness);

/**
 * A client is being freed and should be completely cleaned up
 */
void client_free_all(struct client *client);

/**
 * Initialize clients data
 */
void client_init();
