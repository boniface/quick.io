/**
 * The base definitions of functions and datatypes accessible from an app.
 * @file qio_app.h
 */

#ifndef QIO_APP_H
#define QIO_APP_H

#include <glib.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef APP_DEBUG
	#define DEBUG(format, ...) g_log(_app_name, G_LOG_LEVEL_DEBUG, \
				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
	/**
	 * Output debug info to the console, only when not built for prod.
	 */
	#define DEBUG(format, ...)
#endif

/**
 * General useful information
 */
#define INFO(format, ...) g_log(_app_name, G_LOG_LEVEL_INFO, \
 				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Output warning
 */
#define WARN(format, ...) g_log(_app_name, G_LOG_LEVEL_WARNING, \
 				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Output critical, non-fatal error
 */
#define CRITICAL(format, ...) g_log(_app_name, G_LOG_LEVEL_CRITICAL, \
 				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * A fatal error, after which nothing can continue.
 */
#define FATAL(format, ...) g_log(_app_name, G_LOG_LEVEL_ERROR, \
 				"%s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Possible status for clients, in response to events
 */
enum status {
	/**
	 * Everything went as expected. Send any callbacks, include any data in buffers, and move on.
	 */
	CLIENT_GOOD,

	/**
	 * That bastard of a client aborted / should be aborted, or did something incredibly stupid, so
	 * he should just be killed.
 	 */
	CLIENT_FATAL,

	/**
	 * A non-fatal error occurred while processing. This response trips the event handler
	 * to respond with an error event to callbacks.
	 */
	CLIENT_ERROR,

	/**
	 * An async operation is being performed. This should be treated like CLIENT_GOOD in
	 * most cases, but it MUST NOT send back any information to the client. The function
	 * that returned this status becomes responsible for that.
	 */
	CLIENT_ASYNC,
};

/**
 * Gets a string for the event type.
 */
#define DATA_TYPE(t) (t == d_plain ? (gchar*)"plain" : (gchar*)"json")

#ifdef APP_TESTING
#define TEST_APP_CONFIG app_test_config();

/**
 * When running tests, you sometimes need to set config options: this function will
 * be called during config time, if APP_PARSE_CONFIG() is called.
 */
void app_test_config();

#else

/**
 * Only while running tests does this macro auto-expands to app_test_config()
 */
#define TEST_APP_CONFIG

#endif

/**
 * Parses the configuration for this application and sets up test configuration if
 * necessary.
 */
#define APP_PARSE_CONFIG(config) \
 	{ \
		option_setup_config(config, G_N_ELEMENTS(config)); \
		if (!option_parse_config_file(_app_name, config, G_N_ELEMENTS(config))) { \
			CRITICAL("Could not load config for app \"%s\"", _app_name); \
			return FALSE; \
		} \
		TEST_APP_CONFIG; \
	}

/**
 * Nothing useful in this struct.  Just pass it as needed.
 */
typedef void event_handler_t;

/**
 * The list type used to hold extra path segments.
 *
 * @attention Each item MUST contain a gchar*.
 */
typedef GPtrArray path_extra_t;

/**
 * The data type for callbacks.
 */
typedef guint32 callback_t;

/**
 * The client type.  This is void as it MAY NEVER be modified from anything but internal QIO code.
 * If you ever keep a reference to this around, you MUST call client_ref(client) to increment
 * its reference count.  When you are done with the reference, you MUST call client_unref(client).
 * Doing anything else is a memory leak.
 */
typedef void client_t;

/**
 * The types of entries that are in the configuration file.
 */
enum opt_entry {
	/**
	 * The entry is a 32bit integer.
	 */
	e_int,

	/**
	 * The entry is a string.
	 */
	e_string,

	/**
	 * The entry is a vector of strings.
	 */
	e_stringv,

	/**
	 * The entry is an unsigned 64bit integer.
	 */
	e_uint64,

	/**
	 * The entry is a boolean
	 */
	e_boolean,
};

/**
 * The struct for the config file directive definitions.
 */
struct config_file_entry {
	/**
	 * The name of the option in the configuration file.
	 */
	gchar *name;

	/**
	 * The type of the option in the configuration file.
	 */
	enum opt_entry arg;

	/**
	 * Where the value from the configration file should be stored.
	 */
	gpointer arg_data;

	/**
	 * If an array, the number of items read from the configuration file.
	 */
	gpointer len;
};

/**
 * The different types of data that an event can contain.
 */
enum data_t {
	/**
	 * The data contained in the event is plain text.
	 */
	d_plain,

	/**
	 * The data contained in the event is json-formatted.
	 */
	d_json,
};

/**
 * An event from the client. Contains everything the handler needs to process it.
 */
struct event {
	/**
	 * The underlying buffer that the event is constructed from.
	 *
	 * @note Being able to access the full event name is a side-effect of
	 * processing the event string, and it's rather useful
	 *
	 * @attention This MUST be freed.
	 */
	gchar *name;

	/**
	 * The extra event segments to be passed to the event handler,
	 * not including the event itself.
	 *
	 * @par Example
	 * If the event is: /some/test/event, and the handler is /some,
	 * then this will be ["test", "event"].
	 *
	 * @attention This is ref-counted, so it MUST be g_ptr_array_un/ref()'d
	 */
	path_extra_t *extra;

	/**
	 * The callback number the client sent.
	 */
	callback_t client_callback;

	/**
	 * The callback the server wants to recieve for this event.
	 */
	guint32 server_callback;

	/**
	 * The type of data sitting in the data buffer.
	 */
	enum data_t data_type;

	/**
	 * The data string sent from the client.
	 *
	 * @attention This is a pointer into the original buffer; as such, it MUST
	 * NOT be free'd
	 */
	gchar *data;
};

/**
 * An approximation of the current time, in seconds since Jan 1, 1970.
 * This is updated every so often to reflect a fairly-recent system time.
 */
gint64 qev_time;

/**
 * Increment the reference count on the client.
 */
void client_ref(client_t *client);

/**
 * Decrement the reference count on the client.
 *
 * @attention Once you call this function, you MUST consider your pointer to the client to be
 * invalid, and you MAY NEVER use it again.
 */
void client_unref(client_t *client);

/**
 * Sets a piece of information on a client. All strings will be duplicated before being
 * stored. To remove a key, simply call `client_set(client, "some_key", NULL)`. All values
 * will be available as long as the client exists (ie. as long as a reference to the client
 * exists).
 *
 * @note This information is visible to all apps.
 *
 * @param client The client to store the information on.
 * @param key The key to set.
 * @param value The value to set.
 *
 * @return A pointer to the new, duplicated value. This value MUST NOT be altered, free'd, or
 * anything: if you need to do any operations on it, make a copy.
 */
const gchar* client_set(client_t *client, const gchar *key, const gchar *value);

/**
 * Gets a piece of information on a client.
 *
 * @param client The client to get the information from.
 * @param key The key to get.
 *
 * @return The value, or NULL if not found. This value MUST NOT be altered, free'd, or anything:
 * if you need to do any operations on it, make a copy.
 */
const gchar* client_get(client_t *client, const gchar *key);

/**
 * Determines if a piece of information exists on a client.
 *
 * @param client The client to check the information on.
 * @param key The key to check.
 *
 * @return TRUE if the information if there, FALSE otherwise;
 */
gboolean client_has(client_t *client, const gchar *key);

/**
 * The handler function type.
 * This is the function signature that all event handlers MUST implement.
 *
 * @param client The client that triggered the event.
 * @param event The event that the client sent to the server.
 * @param response The buffer that the handler should write his response to. If the handler
 * is going to write something to this buffer, it MUST return CLIENT_WRITE.
 */
typedef enum status (*handler_fn)(
	client_t *client,
	event_handler_t *handler,
	struct event *event,
	GString *response);

/**
 * A callback for when a client subscribes to a specific event.
 * This is supplied to evs_server_on() and is called when a client subscribes
 * to the event.  Don't give the callbacks too much information about the
 * subscription: it's too much temptation to do unsafe stuff.
 *
 * @param client The client that subscribed to the event.
 * @param handler The event handler, so that references don't have to be stored in the app.
 * @param extra Any extra parameters that came in with the subscription
 * @param callback The callback to be issued if going async
 *
 * @return CLIENT_GOOD Everything is good, send the callback as normal.
 * @return CLIENT_ASYNC Doing async verification, will send the callback internally.
 * @return CLIENT_ERROR if the subscription should be rejected
 */
typedef enum status (*on_subscribe_handler_cb)(
	client_t *client,
	const event_handler_t *handler,
	path_extra_t *extra,
	const callback_t callback);

/**
 * A callback for when a client subscribes to a specific event.
 * This is supplied to evs_server_on() and is called when a client subscribes
 * to the event.  Don't give the callbacks too much information about the
 * subscription: it's too much temptation to do unsafe stuff.
 *
 * @param client The client that subscribed to the event.
 * @param handler The event handler, so that references don't have to be stored in the app.
 * @param extra Any extra parameters that came in with the subscription
 * subscription should be canceled.
 */
typedef void (*on_unsubscribe_handler_cb)(
	client_t *client,
	const event_handler_t *handler,
	path_extra_t *extra);

/**
 * A global on subscribe callback: the type of function called whena user subscribes
 * or unsubscribes from an event.
 *
 * @param client The client that subscribed to the event.
 * @param event_path The path of the event that was subscribed to.
 * @param extra Any extra parameters that came in with the subscription
 */
typedef void (*on_subscribe_cb)(
	const client_t *client,
	const char *event_path,
	const path_extra_t *extra);

/**
 * The function type that is called when a client sends a callback to the server.
 *
 * @param data The data that was passed into evs_server_new_callback()
 * @param event The complete event object. If you want to keep anything in here for anything async,
 * make a copy.
 *
 * @return A status code for the callback.
 */
typedef enum status (*callback_fn)(client_t *client, void *data, struct event *event);

/**
 * The function to be called to free the passed into evs_server_new_callback().
 * @note Data may be freed at any time, even if the callback has not been called yet.
 *
 * @param data The data to be freed.
 */
typedef void (*callback_free_fn)(void *data);

/**
 * For closured "on" functions.
 *
 * @param event The name of the event to be notified for.
 * @param handler The function that should be called when the event comes in.
 * @param on_subscribe The function that should be notified when a client subscribes to this event.
 * @param handle_children If this handler handles children events with handlers.
 *
 * @return The object representing the event, able to be called.
 *
 * @see event_info_s
 */
typedef event_handler_t* (*app_on)(
	const gchar *event_path,
	handler_fn handler,
	on_subscribe_handler_cb on_subscribe,
	on_unsubscribe_handler_cb on_unsubscribe,
	gboolean handle_children);

/**
 * The callback function for adding stats to be sent to graphite.
 *
 * @param key The graphite key to be added
 * @param val The number to be added
 */
typedef void (*apps_stats_cb)(gchar *key, double val);

/**
 * Put a new balance request into the queue.
 *
 * @param count The number of clients to move.
 * @param to The external address (with port) the clients should be sent to.
 */
void conns_balance(guint count, gchar *to);

/**
 * Gets the number of people currently subscribed to a specific event.
 *
 * @param handler The handler to check subscriptions for.
 * @param extra Any extra parameters to add to the path.
 *
 * @return The number of people subscribed; 0 if event does not exist.
 */
gsize evs_client_number_subscribed(
	const event_handler_t *handler,
	path_extra_t *extra);

/**
 * Send a single event to the given client.
 *
 * @param client The client to send the event to.
 * @param handler The event handler of the event being sent
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param server_callback The callback we're expecting on the server.
 * @param type The type of the data (json, plain, etc). d_plain if none.
 * @param data The actual data to be sent. NULL if none.
 *
 * @attention IS thread safe.
 */
enum status evs_client_send(
	client_t *client,
	const event_handler_t *handler,
	path_extra_t *extra,
	const callback_t server_callback,
	const enum data_t type,
	const gchar *data);

/**
 * Send a message to everyone subscribed to the event. This just adds to the list of
 * messages to send, and the message is actually sent in the next loop of the main
 * thread.
 *
 * @param handler The event handler to publish to.
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param type The type of the data (json, plain, etc). d_plain if none.
 * @param data The actual data to be sent. NULL if none.
 *
 * @attention IS thread safe.
 */
enum status evs_client_pub_event(
	const event_handler_t *handler,
	path_extra_t *extra,
	const enum data_t type,
	const gchar *data);

/**
 * The client subscribed to an invalid event. The client should be sent an error message,
 * assuming a callback is specified, and unsubscribed from the event.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to be unsubscribed.
 * @param handler The handler for the event.
 * @param extra Any extra segments to add to the event path
 * @param client_callback The id of the callback the client sent with the event.
 * @param valid If the susbcription is valid; determines if the client will be subscribed
 * or rejected.
 */
void evs_client_app_sub_cb(
	client_t *client,
	const event_handler_t *handler,
	path_extra_t *extra,
	const callback_t client_callback,
	const gboolean valid);

/**
 * Send a callback to a user. This is mainly used for async events that need to send stuff back.
 *
 * @param client The client to send the callback to
 * @param client_callback The id of the callback the client is expecting
 * @param server_callback The callback to be sent back to the server
 * @param type The data type
 * @param data The data to send with the callback
 */
void evs_client_send_callback(
	client_t *client,
	const callback_t client_callback,
	const callback_t server_callback,
	const enum data_t type,
	const gchar *data);

/**
 * Sends an error callback to the user.
 *
 * @param client The client to send the callback to
 * @param client_callback The id of the callback the client is expecting
 */
void evs_client_send_error_callback(
	client_t *client,
	const callback_t client_callback);

/**
 * A shortcut function for not allowing clients to subscribe to an event.
 *
 * @return CLIENT_INVALID_SUBSCRIPTION The client is not allowed to subscribe.
 */
enum status evs_no_subscribe(
	client_t *client,
	const event_handler_t *handler,
	path_extra_t *extra,
	const callback_t callback);

/**
 * Create a new callback on the client that the client can call back to.
 *
 * @param client The client to add a callback to.
 * @param fn The function to be called when the callback comes in.
 * @param data The data to be passed to the callback.
 * @param free_fn The function to be called to free the data passed in.  May be NULL.
 *
 * @return The callback ID.
 */
callback_t evs_server_callback_new(
	client_t *client,
	callback_fn fn,
	void *data,
	callback_free_fn free_fn);

/**
 * Free a previously-created server callback.
 *
 * @param client The client to add a callback to.
 * @param server_callback The id of the callback to remove.
 */
void evs_server_callback_free(client_t *client, callback_t server_callback);

/**
 * If you are ever going to use any static strings in your default config, you MUST
 * call this function before calling option_parse_config_file(). Because options can
 * be overriden all over the place, it is necessary to move all options into the data
 * segment so that they can be free'd at will.
 *
 * @param opts The options to make safe to free.
 * @param opts_len The number of options being passed in.
 */
void option_setup_config(struct config_file_entry opts[], gsize opts_len);

/**
 * Loads the config file and populates all the options.
 *
 * @param group_name The name of the group that should be read and processed.
 * @param opts The options that should be populated.
 * @param opts_len The number of options being passed in.
 */
gboolean option_parse_config_file(
	gchar *group_name,
	struct config_file_entry opts[],
	size_t opts_len);

/**
 * Gets the hostname of the running instance.
 */
const gchar* option_hostname();

/**
 * Gets the number of clients currently connected.
 *
 * @return The number of clients connected.
 */
gsize stats_clients();

#ifdef TESTING
	#define TEST_STATS_INC(slot) g_atomic_pointer_add(&(utils_stats()->slot), 1)
#else
	/**
	 * Increments the stats counter ONLY during testing.
	 */
	#define TEST_STATS_INC(slot)
#endif

/**
 * Boiler-plate code for app initialization.
 */
static guint _app_id = 0;
static gchar *_app_name = NULL;
static gchar *_app_dir = NULL;

/**
 * You should never need this function. It is called from apps.c to give the
 * application information about itself.
 */
void qio_set_app_opts(const guint app_id, const gchar *name, const gchar *dir) {
	_app_id = app_id;
	_app_name = g_strdup(name);
	_app_dir = g_strdup(dir);
}

#ifdef __cplusplus
}
#endif

#endif
