/**
 * Handles routing all events to their proper handlers.
 * @file evs.h
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

	/**
	 * All of the children subscriptions to this event, referenced by
	 * extra path segments.
	 *
	 * @note
	 *     "" as ev_extra is the root event
	 *
	 * Mapping: ev_extra -> struct subscription
	 */
	GHashTable *subs;

	/**
	 * Lock for subs
	 */
	GRWLock subs_lock;
};

/**
 * Intialize a new `struct event`
 */
void event_init(
	struct event *ev,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn);

/**
 * Clean up all memory inside a `struct event`
 */
void event_clear(struct event *ev);

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
void evs_add_handler(
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
 *
 * @return
 *     If the subscription succeeded.
 */
gboolean evs_subscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb);

/**
 * Remove a client from an event
 *
 * @param client
 *     The client to unsubscribe from the event
 * @param ev
 *     The event to remove the client from
 * @param ev_extra
 *     Any extra path segments
 */
void evs_unsubscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra);

/**
 * Cleans up after the client when it closes
 */
void evs_client_close(struct client *client);

/**
 * Sends a callback to a client
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The ID of the callback to send
 * @param code
 *     Any response code you want to send. There are the standard ones,
 *     but you're free to use whatever you want. Keep in mind that `err_msg`
 *     is _only_ sent with the callback when code != 200.
 * @param err_msg
 *     An error message to send with the callback if code != 200.
 * @param json
 *     Any data to include with the callback
 */
void evs_send_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json);

/**
 * Sends a callback to a client, with all possible data.
 *
 * @attention
 *     It is understood that, if a non CODE_OK callback is sent, the server
 *     MAY NOT expect a callback from the client.
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The ID of the callback to send
 * @param code
 *     Any response code you want to send. There are the standard ones,
 *     but you're free to use whatever you want (including negative ones).
 *     Keep in mind that `err_msg` is _only_ sent with the callback when
 *     code != 200.
 * @param err_msg
 *     An error message to send with the callback if code != 200.
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
void evs_send_cb_full(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn);

/**
 * Broadcast a message to all clients listening on the event
 *
 * @param ev
 *     The event to broadcast to
 * @param ev_extra
 *     Any extra path segments
 * @param json
 *     The json to send to everyone
 */
void evs_broadcast(
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json);

/**
 * Broadcast a message to all clients listening on the event
 *
 * @param ev_path
 *     The path to broadcast to
 * @param json
 *     The json to send to everyone
 */
void evs_broadcast_path(
	const gchar *ev_path,
	const gchar *json);

/**
 * Do the actual event broadcasting.
 *
 * @note
 *     This may only be called from a QEV thread.
 */
void evs_broadcast_tick();

/**
 * Initialize events
 */
void evs_init();
