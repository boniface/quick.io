/**
 * Controls the memory around subscriptions.
 * @file sub.h
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
 * Events are what are located at the paths, whereas subscriptions are what
 * are located at ev_path + ev_extra (ev_extra may == ""), and these are
 * what you can actually broadcast and send events to.
 */
struct subscription {
	/**
	 * The event that this sub belongs to
	 */
	struct event *ev;

	/**
	 * To allow a way to remove sub from ev->subs
	 */
	gchar *ev_extra;

	/**
	 * All of the clients currently listening for this event
	 */
	qev_list_t *subscribers;

	/**
	 * Reference count to allow unlocked operations when dealing with
	 * subscriptions. This number technically represents the total number
	 * of clients subscribed and the number of outstanding references
	 * to the subscription. This allows a quick way to determine simultaneously
	 * if the list is empty and no one has a reference anymore.
	 */
	guint refs;
};

/**
 * Get a subscription. If the subscription does not exist, it is created.
 *
 * @param ev
 *     The event to get the subscription from
 * @param ev_extra
 *     Any extra path segments for the subscription
 *
 * @return
 *     A reference to the subscription. When done, call sub_unref().
 */
struct subscription* sub_get(struct event *ev, const gchar *ev_extra);

/**
 * Increase the reference count on the subscription.
 *
 * @param sub
 *     The subscription you're done with.
 */
struct subscription* sub_ref(struct subscription *sub);

/**
 * Release a reference to the subscription, cleaning it up if there
 * are no more references.
 *
 * @param sub
 *     The subscription you're done with.
 */
void sub_unref(struct subscription *sub);

/**
 * Get subs ready to run
 */
void sub_init();
