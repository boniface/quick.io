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

#pragma once
#include "quickio.h"

/**
 * Events are stored in a prefix tree for fast, nice lookups.
 *
 * @attention
 *     All fields are READ ONLY and set by events_query.
 */
struct event {
	/**
	 * The event path for this node
	 */
	gchar *ev_path;

	/**
	 * Function called when client sends event to the server
	 */
	evs_handler_fn handler_fn;

	/**
	 * Called when client attempts to subscribe to event
	 */
	evs_subscribe_fn subscribe_fn;

	/**
	 * Called when client unsubscribes from event
	 */
	evs_unsubscribe_fn unsubscribe_fn;
};

/**
 * Creates a handler for an event
 *
 * @param prefix
 *     A prefix to add to the event path
 * @param ev_path
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
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children);

/**
 * Routes an event from a client.
 *
 * @param client
 *     The client that sent the event
 * @param ev_path
 *     The path of the event
 * @param client_cb
 *     If not 0, the client wants a response to this event sent to the
 *     callback of that id.
 * @param json
 *     Any JSON data included with the event. May NOT be null.
 */
void evs_route(
	struct client *client,
	gchar *ev_path,
	const evs_cb_t client_cb,
	gchar *json);

/**
 * Subscribes the client to the event.
 *
 * @param client
 *     The client to subscribe to the event
 * @param ev
 *     The event to add the client to
 * @param ev_extra
 *     Any extra path segments
 * @param client_cb
 *     The id of the callback to send to the client
 */
void evs_subscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb);

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
	const evs_cb_fn cb_fn,
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
