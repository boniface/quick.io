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
 * The value that indicates there is no callback
 */
#define EVS_NO_CALLBACK (0l)

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
	evs_on_fn on_fn;

	/**
	 * Called when client unsubscribes from event
	 */
	evs_off_fn off_fn;

	/**
	 * If this event wants to handle all children paths
	 */
	gboolean handle_children;

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
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children);

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
 * @param handler_fn
 *     The function that handles the event
 * @param on_fn
 *     Before a subscription is created, this will
 *     be called to check if the subscription should be allowed.
 * @param off_fn
 *     Called when a client unsubscribes
 * @param handle_children
 *     If this event handler also handles sub-events. For example, if
 */
struct event* evs_add_handler(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
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
 * Subscribes the client to the event, checking with the event's subscriber
 * callback that such things are allowed.
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
void evs_on(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb);

/**
 * Send an event to a client, damn the consequences.
 *
 * @note
 *     This is a brute-force function that's really only useful when
 *     sending internal QIO events that don't go by the general rules
 *     of other events.
 *
 * @param client
 *     The client to send the event to
 * @param ev_path
 *     The event path to brute force
 * @param ev_extra
 *     Any extra segments
 * @param json
 *     Data to include
 * @param cb_fn
 *     Function to execute on callback
 * @param cb_data
 *     Data to pass into the function @args{transfer-full}
 * @param free_fn
 *     Frees the cb_data
 */
void evs_send_bruteforce(
	struct client *client,
	const gchar *ev_path,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn);

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
void evs_off(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra);

/**
 * Cleans up after the client when it closes
 */
void evs_client_close(struct client *client);

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
