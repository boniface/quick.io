/**
 * For doing fast lookups of ev_path -> handler.
 * @file
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
 * Create a new event
 *
 * @param ev_path
 *     The event path
 * @param handler_fn
 *     The function to be called when an event of this type comes in
 * @param on_fn
 *     Called when a client attempts to subscribe to the event
 * @param off_fn
 *     Called when a client unsubscribes from the event
 * @param handle_children
 *     If the event would like to handle all unhandled children events
 *
 * @return
 *     If the event was inserted. FALSE indicates that the event already exists.
 */
struct event* evs_query_insert(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children);

/**
 * Finds an event by its path
 *
 * @param ev_path
 *     The event path to find
 * @param ev_extra
 *     Where to place any extra path segments that were found. For example,
 *     if ev_path="/event/path/with/some/extras", and the registered
 *     event was "/event/path", this will contain "/with/some/extras". If
 *     there are no extra segments, this will point to a string of len==0.
 */
struct event* evs_query(
	const gchar *ev_path,
	gchar **ev_extra);

/**
 * Intializes the ptrie for usage
 */
void evs_query_init();
