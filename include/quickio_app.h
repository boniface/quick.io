/**
 * Everything an application needs to run inside a QuickIO server.
 * @file quickio_app.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * Clients are to be treated as a blob that cannot be modified. All the server
 * functions know what to do with them.
 */
typedef struct client client_t;

/**
 * Callbacks are just gigantic integers, 0 meaning "no callback".
 */
typedef guint64 evs_cb_t;

/**
 * From handlers, these values instruct the server how to handle everything
 */
enum evs_status {
	/**
	 * The event was handled and a default callback should be sent, if
	 * the client requested.
	 *
	 * For subscriptions, this indicates that a subscription is valid
	 * and should be accepted.
	 */
	EVS_OK,

	/**
	 * The event was handled and any necessary callbacks (including error
	 * callbacks) have been dispatched by the handler / are going to be
	 * dispatched in the future when data becomes available.
	 *
	 * For subscriptions, this indicates that the subscription_handler
	 * has already / will dispatch the correct subscription callbacks
	 * when it can.
	 */
	EVS_HANDLED,

	/**
	 * There was an error with the handler and a generic error should be
	 * sent back to the client.
	 *
	 * For subscriptions, this indicates that a subscription is invalid
	 * and should be rejected.
	 */
	EVS_ERROR,
};

/**
 * The handler function type.
 *
 * @param client
 *     The client that triggered the event.
 */
typedef enum evs_status (*evs_handler_fn)(
	client_t *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb,
	gchar *json);

/**
 * The handler for when a client subscribes.
 *
 * @todo Indicate if there are already clients subscribed to the event
 *
 * @param client
 *     The client that subscribed.
 */
typedef enum evs_status (*evs_subscribe_fn)(
	client_t *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb);

/**
 * The handler for when a client unsubscribes.
 *
 * @param client
 *     The client that unsubscribed.
 */
typedef void (*evs_unsubscribe_fn)(
	client_t *client);

/**
 * Function called when the client sends a callback to the server
 */
typedef void (*evs_cb_fn)();
