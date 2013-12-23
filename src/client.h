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
 * Checks if a client is subscribed to the given susbcription
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question
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
 *     The subscription in question
 *
 * @return
 *     If the subscription was successful. When TRUE, sub now belongs to
 *     client. When FALSE, sub does not transfer ownership.
 */
gboolean client_sub_add(struct client *client, struct subscription *sub);

/**
 * Removes the subscription from the client.
 *
 * @param client
 *     The client in question
 * @param sub
 *     The subscription in question
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
