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
 * Used to verify that modules are QIO apps.
 */
#define QIO_MAGIC_NUM 0xfc63e18a

/**
 * For an app: sets up all the necessary callbacks
 */
#define QUICKIO_APP(init, run, exit, test) \
	guint __qio_is_app = QIO_MAGIC_NUM; \
	gboolean __qio_app_init(void *app, struct qio_exports exports) \
	{ \
		qio = exports; \
		__qio_app = app; \
		return init(); } \
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
		g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
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
	g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_INFO, \
		"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Output warning
 */
#define APP_WARN(format, ...) \
	g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_WARNING, \
		"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Like perror()
 */
#define APP_PERROR(format, ...) \
	g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, \
		"%s:%d : " format ": %s", __FILE__, __LINE__, ##__VA_ARGS__, \
		strerror(errno))

/**
 * Output critical, non-fatal error
 */
#define APP_CRITICAL(format, ...) \
	g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, \
		"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * A fatal error, after which nothing can continue.
 */
#define APP_FATAL(format, ...) \
	g_log(APP_LOG_DOMAIN, G_LOG_LEVEL_ERROR, \
		"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * If the condition is not true, it brings down the app with it,
 * logging the given message as fatal.
 */
#define APP_ASSERT(format, ...) \
	if (!(cond)) { \
		APP_FATAL(msg, ##__VA_ARGS__); }

/**
 * Increments a counter
 *
 * @param counter
 *     A pointer to the counter struct
 *
 * @return
 *     The value after increment
 */
#define qio_stats_counter_inc(counter) qio.stats_counter_add(counter, 1);

/**
 * Decrements a counter
 *
 * @param counter
 *     A pointer to the counter struct
 *
 * @return
 *     The value after decrement
 */
#define qio_stats_counter_dec(counter) qio.stats_counter_add(counter, -1);

/**
 * Times the execution of the given block of code.
 *
 * @param timer
 *     A pointer to the timer
 * @param block
 *     The block to time
 */
#define qio_stats_time(timer, block) { \
	gint64 qio_stats_timer_start = g_get_monotonic_time(); \
	block; \
	qio.stats_timer_record(timer, g_get_monotonic_time() - qio_stats_timer_start); }

/**
 * For determining if something should be alerted on
 */
struct qio_stats_threshold {
	/**
	 * If the threshold has been configured and is active.
	 */
	gboolean active;

	/**
	 * If there is a warn value for this threshold
	 */
	gboolean has_warn;

	/**
	 * The compare for warn is greater
	 */
	gboolean warn_greater;

	/**
	 * If warn_greater, warn when val > warn, otherwise when val < warn.
	 */
	gint64 warn;

	/**
	 * If there is a warn value for this threshold
	 */
	gboolean has_crit;

	/**
	 * The compare for critical is greater
	 */
	gboolean crit_greater;

	/**
	 * If crit_greater, warn when val > crit, otherwise when val < crit.
	 */
	gint64 crit;
};

/**
 * A custom monitoring function. Returns a value (typically 0 - 3) indicating
 * how everything is going, typically based off of values in thresh.
 *
 * @param val
 *     The value of the stat
 * @param thresh
 *     The array of thresholds configured
 * @param thrs
 *     The number of thresholds
 * @param msg[out]
 *     Message to be included with the return value; print your status message
 *     into the given GString.
 *
 * @return
 *     By convention, returns take one of the following values, but it's
 *     acceptable to return different values if your sink knows what to do
 *     with them.
 * @return
 *     0 when OK
 * @return
 *     1 when WARNING
 * @return
 *     2 when CRITICAL
 * @return
 *     3 when UNKNOWN
 */
typedef gint (*qio_stats_monitor_fn)(
	const gint64 val,
	const struct qio_stats_threshold thresh,
	GString *msg);

/**
 * A gauge stat
 */
typedef struct qio_stats_gauge qio_stats_gauge_t;

/**
 * A counter stat
 */
typedef struct qio_stats_counter qio_stats_counter_t;

/**
 * A timer stat
 */
typedef struct qio_stats_timer qio_stats_timer_t;

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
 * Contains all the fields necessary to send a callback to a client about
 * a subscription.
 *
 * To make a copy of this struct, use: qio.evs_on_info_copy(). To free the
 * memory allocated from there, use qio.evs_on_info_free().
 */
struct evs_on_info {
	/**
	 * Any extra parameters sent with the event
	 */
	gchar *ev_extra;

	/**
	 * Reference to the susbcription. Apps should consider this completely
	 * opaque and MAY NEVER touch it.
	 */
	subscription_t *sub;

 	/**
 	 * The client that sent the subscription
 	 */
	client_t *client;

	/**
	 * The callback id to be sent. Don't mess with this.
	 */
	evs_cb_t client_cb;
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
 * @param info
 *     A self-contained struct of info that can be hurled around; contains
 *     all information needed to send a subscribe callback to a client. If
 *     you intend on returning EVS_STATUS_HANDLED and are passing this
 *     struct to another thread or using it anywhere else, you MUST use
 *     qio.evs_on_info_copy() to make sure all references this struct contains
 *     are usable (qio.evs_on_info_free() cleans it all up).
 * @param ev_extra
 *     Any extra paramters that came with the subscription
 *
 * @return
 *     What should be done with the subscription.
 */
typedef enum evs_status (*evs_on_fn)(const struct evs_on_info *info);

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

struct qio_exports {
	/**
	 * Add an event handler.
	 *
	 * @param app
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
	 *     your event is /test/event, then it would receive events of
	 *     /test/event/path, /test/event/other, /test/event/more.
	 */
	event_t* (*evs_add_handler)(
		void *app,
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
	 enum evs_status (*evs_no_on)(const struct evs_on_info *info);

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
	void (*evs_send)(
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
	void (*evs_send_full)(
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
	void (*evs_on_cb)(
		const gboolean success,
		const struct evs_on_info *info);

	/**
	 * Make a copy of the evs_on_info(), returning a reference that is safe
	 * to use from another thread.
	 *
	 * @param with_ev_extra
	 *     By default, ev_extra is not included in the copy of evs_on_info,
	 *     if you're going to need it, be sure to pass in TRUE.
	 */
	struct evs_on_info* (*evs_on_info_copy)(
		const struct evs_on_info *info,
		const gboolean with_ev_extra);

	/**
	 * Clean up the copy of struct evs_on_info, releasing any references made.
	 */
	void (*evs_on_info_free)(struct evs_on_info *info);

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
	void (*evs_cb)(
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
	void (*evs_cb_with_cb)(
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
	void (*evs_err_cb)(
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
	void (*evs_cb_full)(
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
	void (*evs_broadcast)(
		event_t *ev,
		const gchar *ev_extra,
		const gchar *json);

	/**
	 * Adds a gauge to the stats lineup.
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 *
	 * @return
	 *     The stat, useful for qio.qio_stats_gauge_set/get()
	 */
	qio_stats_gauge_t* (*stats_gauge)(void *app, const gchar *name);

	/**
	 * Adds a gauge to the stats lineup.
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 * @param monitor
	 *     A custom monitor function.
	 *
	 * @return
	 *     The stat, useful for qio.qio_stats_gauge_set/get()
	 */
	qio_stats_gauge_t* (*stats_gauge_monitor)(
		void *app,
		const gchar *name,
		const qio_stats_monitor_fn monitor);

	/**
	 * Set the gauge to the given value.
	 *
	 * @param gauge
	 *     The gauge to set
	 * @param val
	 *     The value to set to
	 */
	void (*stats_gauge_set)(qio_stats_gauge_t *gauge, gint64 val);

	/**
	 * Get the value of the gauge right now.
	 *
	 * @param gauge
	 *     The gauge to set
	 *
	 * @return
	 *     The value of the gauge
	 */
	gint64 (*stats_gauge_get)(qio_stats_gauge_t *gauge);

	/**
	 * Adds a counter to the stats lineup
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 * @param reset_on_flush
	 *     If the counter should be reset between stats flushes
	 *
	 * @return
	 *     The stat, useful for qio.stats_counter_inc/dec() and
	 *     qio.stats_counter_get()
	 */
	qio_stats_counter_t* (*stats_counter)(
		void *app,
		const gchar *name,
		const gboolean reset_on_flush);

	/**
	 * Adds a counter to the stats lineup
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 * @param reset_on_flush
	 *     If the counter should be reset between stats flushes
	 * @param monitor
	 *     A custom monitor function.
	 *
	 * @return
	 *     The stat, useful for qio.stats_counter_inc/dec() and
	 *     qio.stats_counter_get()
	 */
	qio_stats_counter_t* (*stats_counter_monitor)(
		void *app,
		const gchar *name,
		const gboolean reset_on_flush,
		const qio_stats_monitor_fn monitor);

	/**
	 * Add the given value to the counter
	 *
	 * @param counter
	 *     The counter to add the value to
	 * @param val
	 *     The value to add, be it positive or negative.
	 *
	 * @return
	 *     The value of the counter after the operation
	 */
	gint64 (*stats_counter_add)(qio_stats_counter_t *counter, const gint64 val);

	/**
	 * Get the given value to the counter at this instant
	 *
	 * @param counter
	 *     The counter to add the value to
	 *
	 * @return
	 *     The value of the counter right now
	 */
	gint64 (*stats_counter_get)(qio_stats_counter_t *counter);

	/**
	 * Adds a timer to the stats lineup.
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 *
	 * @return
	 *     The timer, used for qio.stats_record_time()
	 */
	qio_stats_timer_t* (*stats_timer)(void *app, const gchar *name);

	/**
	 * Adds a timer to the stats lineup.
	 *
	 * @param name
	 *     The name of the stat (may be `A-Za-z0-9._-`, `.` being
	 *     the path separator)
	 * @param monitor_sum
	 *     Custom monitor for the timer's sum value
	 * @param monitor_stddev
	 *     Custom monitor for the timer's standard deviation value
	 * @param monitor_mean
	 *     Custom monitor for the timer's mean value
	 * @param monitor_max
	 *     Custom monitor for the timer's max value
	 * @param monitor_min
	 *     Custom monitor for the timer's min value
	 * @param monitor_count
	 *     Custom monitor for the timer's count
	 *
	 * @return
	 *     The timer, used for qio.stats_record_time()
	 */
	qio_stats_timer_t* (*stats_timer_monitor)(
		void *app,
		const gchar *name,
		const qio_stats_monitor_fn monitor_sum,
		const qio_stats_monitor_fn monitor_stddev,
		const qio_stats_monitor_fn monitor_mean,
		const qio_stats_monitor_fn monitor_max,
		const qio_stats_monitor_fn monitor_min,
		const qio_stats_monitor_fn monitor_count);

	/**
	 * Records a time to the timer.
	 *
	 * @param timer
	 *     The timer struct
	 * @param us
	 *     Microseconds to record
	 */
	void (*stats_timer_record)(
		qio_stats_timer_t *timer,
		const gint64 us);

	/**
	 * Unpack a string into some JSON
	 */
	gboolean (*json_unpack)(
		gchar *json,
		const gchar *spec,
		...);

	/**
	 * Pack some values into a JSON string.
	 */
	gboolean (*json_pack)(
		GString *buff,
		const gchar *spec,
		...);

	/**
	 * Gets a buffer for you to work with
	 */
	GString* (*buffer_get)();

	/**
	 * Returns a buffer to the pool once you're done with it
	 */
	void (*buffer_put)(GString *buff);

	/**
	 * Close a client
	 */
	void (*close)(client_t *client, guint reason);

	/**
	 * Take a reference to a client
	 */
	client_t* (*ref)(client_t *client);

	/**
	 * Release a reference to a client
	 */
	void (*unref)(client_t *client);
};

/**
 * The app's reference to what QIO knows about the app. The app must never
 * mess with this.
 */
void *__qio_app;

/**
 * The functions that each app has access to.
 */
struct qio_exports qio;

/**
 * If the app doesn't want to implement one of the functions, this may be
 * used as a shorcut
 */
static G_GNUC_UNUSED gboolean qio_app_noop() { return TRUE; }
