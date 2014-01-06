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
 *     your event is /test/event, then it would receive events of
 *     /test/event/path, /test/event/other, /test/event/more.
 */
struct event* evs_add_handler(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children);

/**
 * Doesn't allow anyone to subscribe to the event.
 * Use this for qio.add_handler for the on_fn to disable subscriptions
 * to the event.
 */
enum evs_status evs_no_on(const struct evs_on_info *info);

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
	gchar *ev_extra,
	const evs_cb_t client_cb);

/**
 * Sends an event to a specific client.
 *
 * @param client
 *     The client to send the callback to
 * @param ev
 *     The event to send
 * @param ev_extra
 *     Any extra path segments for the event
 * @param json
 *     Any data to include with the event
 */
void evs_send(
	client_t *client,
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json);

/**
 * Sends an event to a specific client, requesting a callback from the client.
 *
 * @param client
 *     The client to send the callback to
 * @param ev
 *     The event to send
 * @param ev_extra
 *     Any extra path segments for the event
 * @param json
 *     Any data to include with the event
 * @param cb_fn
 *     The function to execute on callback
 * @param cb_data
 *     Data to pass into the function @args{transfer-full}
 * @param free_fn
 *     Frees the cb_data
 */
void evs_send_full(
	client_t *client,
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn);

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
 * Subscribes a client to an event without any callback checks. This should
 * only be used in the callback case.
 *
 * @param success
 *     If the checks were successful and the client should be added to the
 *     subscription. An error callback is sent if false.
 * @param client
 *     The client to subscribe
 * @param sub
 *     The subscription to put the client in
 * @param client_cb
 *     The callback to send to the client
 */
void evs_on_cb(
	const gboolean success,
	const struct evs_on_info *info);

/**
 * Make a copy of the evs_on_info(), returning a reference that is safe
 * to use from another thread.
 *
 * @param info
 *     The object to make a copy of. Also ensures that all references necessary
 *     are made before returning.
 * @param with_ev_extra
 *     If you want ev_extra in the copy. By default, info->ev_extra is left
 *     NULL;
 */
struct evs_on_info* evs_on_info_copy(
	const struct evs_on_info *info,
	const gboolean with_ev_extra);

/**
 * Clean up the copy of struct evs_on_info, releasing any references made.
 *
 * @param info
 *     The data to free up. Makes sure that all things inside are unreferenced
 *     properly.
 */
void evs_on_info_free(struct evs_on_info *info);

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
 * Sends a CODE_OK callback to a client
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The ID of the callback to send
 * @param json
 *     Any data to include with the callback
 */
void evs_cb(
	client_t *client,
	const evs_cb_t client_cb,
	const gchar *json);

/**
 * Sends a CODE_OK callback to a client while requesting a callback from the
 * client.
 *
 * @param client
 *     The client to send the callback to
 * @param client_cb
 *     The ID of the callback to send
 * @param json
 *     Any data to include with the callback
 * @param cb_fn
 *     The function to be called when the client responds to the server
 *     callback. @args{allow-null}
 * @param cb_data
 *     Data to be given back to the callback when the client responds
 *     @args{transfer-full}
 * @param free_fn
 *     Function that frees cb_data @args{allow-null}
 */
void evs_cb_with_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn);

/**
 * Sends a callback to a client with an error code and message.
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
void evs_err_cb(
	client_t *client,
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
 * @param cb_fn
 *     The function to be called when the client responds to the server
 *     callback.
 * @param cb_data
 *     Data to be given back to the callback when the client responds
 *     @args{transfer-full}
 * @param free_fn
 *     Function that frees cb_data
 */
void evs_cb_full(
	client_t *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn);

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
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json);

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
