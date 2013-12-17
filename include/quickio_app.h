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

#pragma once
#include <glib.h>

/**
 * Allow exported functions to be declared here without trouble
 */
#ifndef QIO_EXPORT
	#define QIO_EXPORT
#endif

/**
 * Used to verify that modules are QIO apps.
 */
#define QIO_MAGIC_NUM 0xfc63e18a

/**
 * For an app: sets up all the necessary callbacks
 */
#define QUICKIO_APP(init, run, exit, test) \
	guint __qio_is_app = QIO_MAGIC_NUM; \
	gboolean __qio_app_init(void *app) \
	{ \
		__qio_app = app; \
		return init(); } \
	gboolean __qio_app_run() \
	{ \
		return run(); } \
	gboolean __qio_app_exit() \
	{ \
		return exit(); } \
	gboolean __qio_app_test() \
	{ \
		return test(); }

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
	EVS_STATUS_OK,

	/**
	 * There was an error with the handler and a generic error should be
	 * sent back to the client.
	 *
	 * For subscriptions, this indicates that a subscription is invalid
	 * and should be rejected.
	 */
	EVS_STATUS_ERROR,

	/**
	 * The event was handled and any necessary callbacks (including error
	 * callbacks) have been dispatched by the handler / are going to be
	 * dispatched in the future when data becomes available.
	 *
	 * For subscriptions, this indicates that the subscription_handler
	 * has already / will dispatch the correct subscription callbacks
	 * when it can.
	 */
	EVS_STATUS_HANDLED,
};

/**
 * Codes used for callbacks.
 * We're not doing anything HTTP with QIO, but the HTTP error codes are pretty
 * well know, so reusing what already exists just makes more sense.
 */
enum evs_code {
	/**
	 * Everything went as expected
	 */
	CODE_OK = 200,

	/**
	 * Malformed data was sent. It's not fatal, just annoying.
	 */
	CODE_BAD = 400,

	/**
	 * You're not allowed to do that.
	 */
	CODE_UNAUTH = 401,

	/**
	 * Yeah, I'm going to need you to go ahead and not use something that
	 * doesn't exist.
	 */
	CODE_NOT_FOUND = 404,

	/**
	 * An unknown error occurred
	 */
	CODE_UNKNOWN = 500,
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

/**
 * The app's reference to what QIO knows about the app. The app must never
 * mess with this.
 */
void *__qio_app;

/**
 * If the app doesn't want to implement one of the functions, this may be
 * used as a shorcut
 */
static G_GNUC_UNUSED gboolean qio_app_noop() { return TRUE; }

/**
 * Add an event handler
 */
QIO_EXPORT void qio_export_add_handler(
	void *app,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children);

#define qio_add_handler(...) qio_export_add_handler(__qio_app, ##__VA_ARGS__)
