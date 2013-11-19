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
 * @param prefix
 *     A prefix to add to the event path
 * @param event_path
 *     The path for the event
 * @param handler
 *     The function that handles the event
 * @param subscribe
 *     Before a subscription is created, this will
 *     be called to check if the subscription should be allowed.
 * @param unsubscribe
 *     Called when a client unsubscribes
 * @param handle_children
 *     If this event handler also handles sub-events. For example, if
 */
void evs_on(
	const gchar *prefix,
	const gchar *event_path,
	const evs_handler handler,
	const evs_subscribe subscribe,
	const evs_unsubscribe unsubscribe,
	const gboolean handle_children);

/**
 * Routes an event from a client.
 *
 * @param client
 *     The client that sent the event
 * @param event_path
 *     The path of the event
 * @param client_cb
 *     If not 0, the client wants a response to this event sent to the
 *     callback of that id.
 * @param json
 *     Any JSON data included with the event. May NOT be null.
 */
void evs_route(
	struct client *client,
	gchar *event_path,
	const evs_cb_t client_cb,
	gchar *json);

/**
 * Sends an event to a client.
 */
void evs_send();

/**
 * Sends a callback to a client
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The ID of the callback to send
 * @param json
 *     Any data to include with the callback
 * @param cb
 *     The function to be called when the client responds to the server
 *     callback.
 * @param cb_data
 *     Data to be given back to the callback when the client responds
 *     @args{transfer-full}
 * @param free_fn
 *     Function that frees cb_data
 */
void evs_send_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *json,
	const evs_cb cb,
	void *cb_data,
	const qev_free_fn free_fn);

/**
 * Sends an error callback to a client
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The id of the callback expecting the response
 * @param msg
 *     The error message that should be included with the response
 */
void evs_send_cb_err(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *msg);

/**
 * Initialize events
 */
void evs_init();
