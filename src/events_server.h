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
	guint callback;
	
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
 * @param response The buffer that the handler should write his response to. If the handler is going to write something to this buffer, it MUST return CLIENT_WRITE.
 */
typedef status_t (*handler_fn)(client_t *client, event_t *event, GString *response);

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
 */
void evs_server_on(const gchar *event, handler_fn handler);

/**
 * Init the event listening interface.
 */
gboolean evs_server_init();