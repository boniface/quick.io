/**
 * Handles the routing of events triggered on the client side.
 * @file events_server.h
 */
#ifndef QIO_EVENTS_SERVER_H
#define QIO_EVENTS_SERVER_H

#include "qio.h"

/**
 * What is used to separate parameters in the events.
 *
 * @note The ":" character is not allowed in event names.
 */
#define EVENT_DELIMITER ":"

/**
 * What is used to separate the event parameters from the data.
 *
 * @note The "=" character is not allowed in event names.
 */
#define EVENT_DATA_DELIMITER "="

/**
 * The maximum number of callbacks that are supported on a client.
 * This MUST NEVER be greater than 255.
 */
#define MAX_CALLBACKS 4

/**
 * Generates a server callback
 */
#define SERVER_CALLBACK(slot, id) ((slot << 8) | id)

/**
 * Decomposes a server callback into slot and id
 */
#define SERVER_CALLBACK_PARTS(client, server_callback, slot, id) \
	slot = (server_callback >> 8) % G_N_ELEMENTS(client->callbacks); \
	id = server_callback & 0xFF;

/**
 * The list type used to hold extra path segments.
 * This is reference counted, so it MUST NEVER be g_ptr_array_free'd.
 *
 * @attention This may never be modified after it is created.
 * @note Each item is a gchar*.
 */
typedef GPtrArray path_extra_t;

/**
 * The data type for callbacks.
 */
typedef guint32 callback_t;

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
typedef struct event_s {
	/**
	 * The underlying buffer that the event is constructed from.
	 *
	 * @note Being able to access the full event name is a side-effect of
	 * processing the event string, and it's rather useful
	 *
	 * @attention This MUST be freed.
	 */
	gchar *path;

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
	callback_t server_callback;

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
} event_t;

/**
 * The handler function type.
 * This is the function signature that all event handlers MUST implement.
 *
 * @param client The client that triggered the event.
 * @param event The event that the client sent to the server.
 * @param response The buffer that the handler should write his response to. If there is a
 * callback, then this is the data that will be sent. If there is no callback, then the
 * data will be ignored.
 *
 * @return CLIENT_GOOD Everything went well within the handler, and any data / callbacks
 * should be sent.
 * @return CLIENT_ERROR Something went wrong, just answer with a standard QIO error,
 * or drop the request if there is no callback. With this return value, `response` will
 * be ignored.
 */
typedef status_t (*handler_fn)(client_t *client, event_handler_t *handler, event_t *event, GString *response);

/**
 * A callback for when a client subscribes to a specific event.
 * This is supplied to evs_server_on() and is called when a client subscribes
 * to the event.  Don't give the callbacks too much information about the
 * subscription: it's too much temptation to do unsafe stuff.
 *
 * @param client The client that subscribed to the event.
 * @param handler The event handler, so that references don't have to be stored in the app.
 * @param extra Any extra parameters that came in with the subscription.
 * @param client_callback The callback to be issued if going async
 *
 * @return CLIENT_GOOD Everything is good, send the callback as normal.
 * @return CLIENT_ASYNC Doing async verification, will send the callback internally.
 * @return CLIENT_INVALID_SUBSCRIPTION if the subscription should be rejected
 */
typedef status_t (*on_subscribe_handler_cb)(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback);

/**
 * A callback for when a client unsubscribes to a specific event.
 * This is supplied to evs_server_on() and is called when a client subscribes
 * to the event.  Don't give the callbacks too much information about the
 * subscription: it's too much temptation to do unsafe stuff.
 *
 * @param client The client that unsubscribed from the event.
 * @param handler The event handler, so that references don't have to be stored in the app.
 * @param extra Any extra parameters that came in with the subscription that should be killed.
 */
typedef void (*on_unsubscribe_handler_cb)(client_t *client, const event_handler_t *handler, path_extra_t *extra);

/**
 * The function type that is called when a client sends a callback to the server.
 *
 * @param data The data that was passed into evs_server_new_callback()
 * @param event The complete event object. If you want to keep anything in here for anything async,
 * make a copy.
 *
 * @return A status code for the callback.
 */
typedef status_t (*callback_fn)(client_t *client, void *data, event_t *event);

/**
 * The function to be called to free the passed into evs_server_new_callback().
 * @note Data may be freed at any time, even if the callback has not been called yet.
 *
 * @param data The data to be freed.
 */
typedef void (*callback_free_fn)(void *data);

/**
 * The information necessary for maintaining server callbacks.
 */
struct client_cb_s {
	/**
	 * The current ID of the callback.  Makes sure that callbacks don't overlap
	 * when one has been evicted and its still active on the client.
	 */
	guint8 id;

	/**
	 * The callback function.
	 * @attention When this is NULL, it indicates that the callback is not in use,
	 * so when a callback is used, this MUST be set to NULL.
	 */
	callback_fn fn;

	/**
	 * The data to be passed to the callback
	 */
	void *data;

	/**
	 * The free function
	 */
	callback_free_fn free_fn;
};

/**
 * All of the handlers and callbacks for an event.
 */
struct event_handler_s {
	/**
	 * The handler function for the event.
	 * @note If this is NULL, then there will be no handler.
	 */
	handler_fn fn;

	/**
	 * The alert function for when a client subscribes to the event.
	 * @note If this is NULL, then no notifications will be sent.
	 */
	on_subscribe_handler_cb on_subscribe;

	/**
	 * The alert function for when a client unsubscribes from the event.
	 * @note If this is NULL, then no notifications will be sent.
	 */
	on_unsubscribe_handler_cb on_unsubscribe;

	/**
	 * If this event handler handles child events without handlers.
	 *
	 * For example, if the event is: /some/event/with/children,
	 * and the handler is /some/event, if this is TRUE, and there is
	 * no more specific specifier (ie. /some/event/with), then this
	 * will be the handler.  Otherwise, it will not.
	 */
	gboolean handle_children;
};

/**
 * Get all of the event info for the event.
 *
 * @param event_path The event path.
 * @param[out] extra Where any extra path segments are put. Will not be updated if NULL. Use
 * g_ptr_array_unref() when done.
 *
 * @return The event handler. If null, extra will also be NULL.
 */
event_handler_t* evs_server_get_handler(const gchar *event_path, path_extra_t **extra);

/**
 * Handle an event from a client.
 *
 * @param client The client that sent the event to handle.
 *
 * @return CLIENT_GOOD Everything went well with processing.
 * @return CLIENT_WRITE Data should be sent back to the client.
 * @return CLIENT_ARBORTED The client should be closed.
 */
status_t evs_server_handle(client_t *client);

/**
 * Listen for an event from clients.
 *
 * @param event_path The name of the event to be notified for.
 * @param fn The function that should be called when the event comes in. If this parameter is
 * NULL, then any events sent from the clients to this event will be dropped.
 * @param on_subscribe The function to be called when a client subscribes to this event.
 * @param on_unsubscribe The function to be called when a client unsubscribes from this event.
 * @param handle_children If this handler handles children events with handlers.
 *
 * @see event_info_s
 */
event_handler_t* evs_server_on(const gchar *event_path, handler_fn fn, on_subscribe_handler_cb on_subscribe, on_unsubscribe_handler_cb on_unsubscribe, gboolean handle_children);

/**
 * Gets the name of an event from the handler.
 * Rationale: in order to hide event path prefixing from apps, they're only given a reference
 * to their handlers so that they can't touch anything (they already know everything about what is
 * in their handlers).  With the handler, we need a way to resolve the path of the event, so this
 * function is provided for that purpose.
 *
 * @param handler The handler to get the path for.
 *
 * @return The path of the event, NULL if not found. THIS MUST NOT BE free()'d
 */
gchar* evs_server_path_from_handler(const event_handler_t *handler);

/**
 * Cleans up and formats the path, with any extra path elements added on.
 *
 * Any invalid characters will be stripped from the path until it contains
 * only the following characters: -_a-zA-Z0-9/
 *
 * @param event_path The event path
 * @param extra Any extra paramters (may be NULL).
 *
 * @return The complete, formatted path, with any extra parameters. If the path is
 * completely invalid, this will return NULL. This MUST be g_free()'d
 */
gchar* evs_server_format_path(const gchar *event_path, path_extra_t *extra);

/**
 * A shortcut function for not allowing clients to subscribe to an event.
 *
 * @ingroup AppFunctions
 *
 * @return CLIENT_INVALID_SUBSCRIPTION The client is not allowed to subscribe.
 */
APP_EXPORT status_t evs_no_subscribe(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback);

/**
 * Create a new callback on the client that the client can call back to.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to add a callback to.
 * @param fn The function to be called when the callback comes in.
 * @param data The data to be passed to the callback.
 * @param free_fn The function to be called to free the data passed in.  May be NULL.
 *
 * @return The callback ID.
 */
APP_EXPORT callback_t evs_server_callback_new(client_t *client, callback_fn fn, void *data, callback_free_fn free_fn);

/**
 * Free a previously-created server callback.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to add a callback to.
 * @param server_callback The id of the callback to remove.
 */
APP_EXPORT void evs_server_callback_free(client_t *client, callback_t server_callback);

/**
 * Alert that a client has closed so that it can free all its stuffs.
 *
 * @param client The client that closed.
 */
void evs_server_client_close(client_t *client);

/**
 * Init the event listening interface.
 */
gboolean evs_server_init();

#ifdef TESTING
#include "../test/test_events_server.h"
#endif

#endif
