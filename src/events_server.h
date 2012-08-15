/**
 * Handles the routing of events triggered on the client side.
 * @file events_server.h
 */

#pragma once
#include "qio.h"

/**
 * What is used to separate parameters in the events.
 *
 * @note The ":" character is not allowed in event names.
 */
#define EVENT_DELIMITER ":"

/**
 * Response to the client that the subscription is invalid.
 */
#define EVENT_RESPONSE_INVALID_SUBSCRIPTION "invalid"

/**
 * Response that the client already has the maximum number of subscriptions.
 */
#define EVENT_RESPONSE_MAX_SUBSCRIPTIONS "too_many_subscriptions"

/**
 * Response that the clietn is already subscribed to this event.
 */
#define EVENT_RESPONSE_ALREADY_SUBSCRIBED "already_subscribed"

/**
 * Response that the client cannot be unsubscribed from the event.  Either way,
 * the client will not recieve this event.
 */
#define EVENT_RESPONSE_CANNOT_UNSUBSCRIBE "cannot_unsubscribe"

/**
 * The list type used to hold extra path segments.
 *
 * @attention Each item MUST contain a gchar*.
 */
typedef GList* path_extra_t;

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
	gchar *name;
	
	/**
	 * The extra event segments to be passed to the event handler,
	 * not including the event itself.
	 *
	 * @par Example
	 * If the event is: /some/test/event, and the handler is /some,
	 * then this will be ["test", "event"].
	 *
	 * @attention This must be free'd with g_list_free_full(extra_segments, g_free)
	 */
	path_extra_t extra_segments;
	
	/**
	 * The number of extra segments, so that clients don't have to iterate
	 * to find the length.
	 */
	guint16 extra_segments_len;
	
	/**
	 * The callback number the client sent.
	 */
	guint32 callback;
	
	/** 
	 * The type of data sitting in the data buffer.
	 */
	enum data_t type;
	
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
 * @param response The buffer that the handler should write his response to. If the handler
 * is going to write something to this buffer, it MUST return CLIENT_WRITE.
 */
typedef status_t (*handler_fn)(client_t *client, event_t *event, GString *response);

/**
 * A callback for when a client subscribes to a specific event.
 * This is supplied to evs_server_on() and is called when a client subscribes
 * to the event.  Don't give the callbacks too much information about the 
 * subscription: it's too much temptation to do unsafe stuff.
 *
 * @param client The client that subscribed to the event.
 * @param extra Any extra parameters that came in with the subscription
 * @param extra_len The count of extra.
 */
typedef void (*on_subscribe_cb)(const client_t *client, const path_extra_t extra, const guint extra_len);

/**
 * All of the handlers and callbacks for an event.
 */
typedef struct event_handler_s {
	/**
	 * The handler function for the event.
	 * @note If this is NULL, then there will be no handler.
	 */
	handler_fn fn;
	
	/**
	 * The alert function for when a client subscribes to the event.
	 * @note If this is NULL, then no notifications will be sent.
	 */
	on_subscribe_cb on_subscribe;
	
	/**
	 * The alert function for when a client unsubscribes from the event.
	 * @note If this is NULL, then no notifications will be sent.
	 */
	on_subscribe_cb on_unsubscribe;
	
	/**
	 * If this event handler handles child events without handlers.
	 *
	 * For example, if the event is: /some/event/with/children,
	 * and the handler is /some/event, if this is TRUE, and there is
	 * no more specific specifier (ie. /some/event/with), then this
	 * will be the handler.  Otherwise, it will not.
	 */
	gboolean handle_children;
} event_handler_t;

/**
 * Get all of the event info for the event.
 *
 * @param event_path The event path.
 * @param[out] extra Where any extra path segments are put. Will not be updated if NULL. MUST be g_list_free_full(extra, g_free)'d.
 * @param[out] extra_len The number of extra path segments. Will not be updated if NULL.
 *
 * @return The event handler. If null, extra will also be NULL.
 */
event_handler_t* evs_server_get_handler(const gchar *event_path, path_extra_t *extra, guint16 *extra_len);

/**
 * Handle an event from a client.
 *
 * @param client The client that sent the event to handle.
 */
status_t evs_server_handle(client_t *client);

/**
 * The "/sub::plain=1477" command.
 */
status_t evs_server_subscribe(client_t*, event_t*, GString *response);

/**
 * The "/unsub::plain=1477" command.
 */
status_t evs_server_unsubscribe(client_t*, event_t*, GString *response);

/**
 * Listen for an event from clients.
 *
 * @param event The name of the event to be notified for.
 * @param handler The function that should be called when the event comes in.
 * @param on_subscribe The function to be called when a client subscribes to this event.
 * @param on_unsubscribe The function to be called when a client unsubscribes from this event.
 * @param handle_children If this handler handles children events with handlers.
 *
 * @see event_info_s
 */
event_handler_t* evs_server_on(const gchar *event, handler_fn fn, on_subscribe_cb on_subscribe, on_subscribe_cb on_unsubscribe, gboolean handle_children);

/**
 * Gets the name of an event from the handler.
 * Rationale: in order to hide event path prefixing from apps, they're only given a reference
 * to their handlers so that they can't touch anything (they already know everything about what is
 * in their handlers).  With the handler, we need a way to resolve the path of the event, so this
 * function is provided for that purpose.
 *
 * @param handler The handler to get the path for.
 *
 * @return The path of the event, NULL if not found.
 */
gchar* evs_server_path_from_handler(const event_handler_t *handler);

/**
 * Init the event listening interface.
 */
gboolean evs_server_init();

#ifdef TESTING
#include "../test/test_events_server.h"
#endif