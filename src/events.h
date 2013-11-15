/**
 * Handles routing all events to their proper handlers.
 * @file events.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Creates a handler for an event
 *
 * @param event_path
 *     The path for the event
 * @param handler
 *     The function that handles the event
 * @param subscribe_handler
 *     Before a subscription is created, this will
 *     be called to check if the subscription should be allowed.
 * @param unsubscribe_handler
 *     Called when a client unsubscribes
 * @param handle_children
 *     If this event handler also handles sub-events. For example, if
 */

/**
 * Routes an event from a client.
 *
 * @param client
 *     The client that sent the event
 * @param event_path
 *     The path of the event
 * @param client_callback
 *     If not 0, the client wants a response to this event sent to the
 *     callback of that id.
 * @param json
 *     Any JSON data included with the event. May NOT be null.
 */
void events_route(
	struct client *client,
	const gchar *event_path,
	const guint64 client_callback,
	const gchar *json);

/**
 * Sends an event to a client.
 */
void events_send();

/**
 * Initialize events
 */
void events_init();
