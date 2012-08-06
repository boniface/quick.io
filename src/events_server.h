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
 * Response that the client canno tbe unsubscribed from the event.  Either way,
 * the client will not recieve this event.
 */
#define EVENT_RESPONSE_CANNOT_UNSUBSRIBE "cannot_unsubscribe"

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
 * All the information an event needs.
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
	 * @note This must be free'd with g_list_free
	 */
	GList *extra_segments;
	
	/**
	 * The number of extra segments, so that clients don't have to iterate
	 * to find the length.
	 */
	guint extra_segments_len;
	
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
 * to the event
 *
 * @param client The client that subscribed to the event.
 * @param extra Any extra parameters that came in with the subscription
 * @param extra_len The count of extra.
 */
typedef void (*on_subscribe_cb)(client_t *client, GList *extra, guint extra_len);

/**
 * Necessary data for storing events in the hash table.
 */
typedef struct event_info_s {
	/**
	 * The handler function for the event.
	 * @note If this is NULL, then there will be no handler.
	 */
	handler_fn handler;
	
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
} event_info_t;

/**
 * Get all of the event info for the event.
 *
 * Here's an idea of how to call this function:<br />
 * `gchar *event = g_strdup("/some/event");`<br />
 * `GList *extra = NULL;`<br />
 * `guint extra_len = 0;`<br />
 * `evs_server_get_einfo(event, &extra, &extra_len);`<br />
 * `g_free(event)`
 *
 * @param[out] curr The event name. This WILL be modified to contain the 
 * path of the handler found.
 * @param[out] extra Where any extra path segments are put. This is REQUIRED.
 * @param[out] extra_len The number of extra path segments. This is REQUIRED.
 */
event_info_t* evs_server_get_einfo(gchar *curr, GList **extra, guint *extra_len);

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
 * @see event_handler_s
 */
void evs_server_on(const gchar *event, handler_fn handler, on_subscribe_cb on_subscribe, on_subscribe_cb on_unsubscribe, gboolean handle_children);

/**
 * Init the event listening interface.
 */
gboolean evs_server_init();