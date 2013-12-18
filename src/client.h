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
	 * The only timeout that will be set on a client from QIO.
	 *
	 * @attention This MAY ONLY ever be set on connect and in the protocols.
	 * It may never be altered anywhere else.
	 */
	qev_timeout_t *timeout;

	/**
	 * Used as a set.
	 *
	 * Maps: struct subscription -> gint32 (qev_list index)
	 */
	GHashTable *subs;
};

/**
 * Get an index pointer to use for the qev_list that manages the subscriptions.
 *
 * This function uses a very simple heuristic to determine if a client should
 * be allowed to subscribe: it's based on the number of active clients vs
 * the number of active subscriptions.
 *
 * @param client
 *     The client attempting to subscribe
 *
 * @return
 *     The pointer to use; NULL if memory pressure is stopping the subscription.
 */
gint32* client_sub_get(struct client *client);

/**
 * Removes a single subscription from a client.
 *
 * @param client
 *     The client that is releasing the subscription
 * @param idx
 *     The value returned from client_sub_get()
 */
void client_sub_put(struct client *client, gint32 *idx);
