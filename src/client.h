/**
 * Provides all utility client functions.
 * @file client.h
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
	 * The state of the client at the protocol level. Protocols may only
	 * touch this once they have signaled that they handle the client.
	 *
	 * @note Protocols may only use bits 0-14, bit 15 (identified by 0x8000) is
	 * reserved for the protocol router.
	 */
	guint16 protocol_flags;

	/**
	 * Once a protocol has accepted the client, this will point to the protocol-
	 * level functions that can be used.
	 */
	struct protocol *protocol;

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
	 * @attention This MAY ONLY ever be set on connect and in the protocols.
	 * It may never be altered anywhere else.
	 */
	qev_timeout_t *timeout;

	/**
	 * Maps: struct subscription -> gint32 (qev_list index)
	 */
	GHashTable *subs;

	/**
	 * The callbacks for the client.
	 */
	struct client_cb *cbs[8];

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
 *     The function to call when the client triggers the callback. @args{not-null}
 * @param cb_data
 *     Data to come with the callback @args{transfer-full}
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
 * Checks if a client is subscribed to the given susbcription
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @args{transfer-none}
 *
 * @return
 *     If the client has the subscription
 */
gboolean client_sub_has(struct client *client, struct subscription *sub);

/**
 * Adds the subscription to the client.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @args{transfer-full}
 *
 * @return
 *     If the subscription was successful.
 */
gboolean client_sub_add(struct client *client, struct subscription *sub);

/**
 * Removes the subscription from the client.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question @args{transfer-none}
 *
 * @return
 *     If the subscription was removed.
 */
gboolean client_sub_remove(struct client *client, struct subscription *sub);

/**
 * Removes all subscriptions from the client.
 *
 * @param client
 *     The client in question
 */
void client_sub_remove_all(struct client *client);

/**
 * A client has closed and should be completely cleaned up
 */
void client_close(struct client *client);

/**
 * Initialize clients data
 */
void client_init();
