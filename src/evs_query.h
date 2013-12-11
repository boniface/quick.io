/**
 * For doing fast lookups of ev_path -> handler.
 * @file events_query.h
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
 * Create a new event
 *
 * @param ev_path
 *     The event path
 * @param handler
 *     The function to be called when an event of this type comes in
 * @param subscribe
 *     Called when a client attempts to subscribe to the event
 * @param unsubscribe
 *     Called when a client unsubscribes from the event
 * @param handle_children
 *     If the event would like to handle all unhandled children events
 *
 * @return
 *     If the event was inserted. FALSE indicates that the event already exists.
 */
gboolean evs_query_insert(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children);

/**
 * Finds an event by its path
 *
 * @param ev_path
 *     The event path to find
 * @param ev_extra
 *     Where to place any extra path segments that were found. For example,
 *     if ev_path="/event/path/with/some/extras", and the registered
 *     event was "/event/path", this will contains "/with/some/extras". If
 *     there are no extra paths, this will point to a string of len==0.
 */
struct event* evs_query(
	const gchar *ev_path,
	gchar **ev_extra);

/**
 * Intializes the ptrie for usage
 */
void evs_query_init();
