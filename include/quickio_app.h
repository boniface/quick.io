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

#ifdef APP_ENABLE_DEBUG
	/**
	 * Disable when building for prod
	 */
	#define APP_DEBUG(format, ...) \
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
					"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
	/**
	 * Output debug info to the console, only when not built for prod.
	 */
	#define APP_DEBUG(format, ...)
#endif

/**
 * General useful information
 */
#define APP_INFO(format, ...) \
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, \
				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Output warning
 */
#define APP_WARN(format, ...) \
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, \
				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Like perror()
 */
#define APP_PERROR(format, ...) \
			g_log(QEV_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, \
				"%s:%d : " format ": %s", __FILE__, __LINE__, ##__VA_ARGS__, \
				strerror(errno))

/**
 * Output critical, non-fatal error
 */
#define APP_CRITICAL(format, ...) \
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, \
				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * A fatal error, after which nothing can continue.
 */
#define APP_FATAL(format, ...) \
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, \
				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Clients are to be treated as a blob that cannot be modified. All the server
 * functions know what to do with them.
 */
typedef struct client client_t;

/**
 * Events are to be treated as a blob. These gusy are what allow you to
 * broadcast to clients.
 */
typedef struct event event_t;

/**
 * Subscriptions are to be treated as a blob.
 */
typedef struct subscription subscription_t;

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
	EVS_STATUS_ERR,

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
	 * The server understood the request but needs to client to calm down.
	 * Aka. you've been rate-limited.
	 */
	CODE_ENHANCE_CALM = 420,

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
typedef enum evs_status (*evs_on_fn)(
	client_t *client,
	subscription_t *sub,
	const gchar *ev_extra,
	const evs_cb_t client_cb);

/**
 * The handler for when a client unsubscribes.
 *
 * @param client
 *     The client that unsubscribed.
 */
typedef void (*evs_off_fn)(
	client_t *client,
	const gchar *ev_extra);

/**
 * Function called when the client sends a callback to the server
 */
typedef enum evs_status (*evs_cb_fn)(
	client_t *client,
	const evs_cb_t client_cb,
	gchar *json);

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
 * Creates a handler for an event
 *
 * @param
 *     Your __qio_app
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
QIO_EXPORT event_t* qio_export_add_handler(
	void *app,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children);

#define qio_evs_add_handler(...) qio_export_add_handler(__qio_app, ##__VA_ARGS__)

/**
 * Doesnt allow anyone to subscribe to the event.
 */
QIO_EXPORT enum evs_status qio_evs_no_on(
	client_t *client,
	subscription_t *sub,
	const gchar *ev_extra,
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
QIO_EXPORT void qio_evs_send(
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
 *     Data to pass into the function
 * @param free_fn
 *     Frees the cb_data
 */
QIO_EXPORT void qio_evs_send_full(
	client_t *client,
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn);

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
QIO_EXPORT void qio_evs_on_cb(
	const gboolean success,
	struct client *client,
	subscription_t *sub,
	const evs_cb_t client_cb);

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
QIO_EXPORT void qio_evs_cb(
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
 *     callback.
 * @param cb_data
 *     Data to be given back to the callback when the client responds
 *     @args{transfer-full}
 * @param free_fn
 *     Function that frees cb_data
 */
QIO_EXPORT void qio_evs_cb_with_cb(
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
QIO_EXPORT void qio_evs_err_cb(
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
QIO_EXPORT void qio_evs_cb_full(
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
QIO_EXPORT void qio_evs_broadcast(
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json);

/**
 * Unpack a string into some JSON
 */
QIO_EXPORT gboolean qio_export_json_unpack(
	gchar *json,
	const gchar *spec,
	...);

#define qio_json_unpack qio_export_json_unpack

/**
 * Pack some values into a JSON string.
 */
QIO_EXPORT gboolean qio_export_json_pack(
	GString *buff,
	const gchar *spec,
	...);

#define qio_json_pack qio_export_json_pack

/**
 * Gets a buffer for you to work with
 */
QIO_EXPORT GString* qio_export_buffer_get();

#define qio_buffer_get qio_export_buffer_get

/**
 * Returns a buffer once you're done with it
 */
QIO_EXPORT void qio_export_buffer_put(GString *buff);

#define qio_buffer_put qio_export_buffer_put
