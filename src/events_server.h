#pragma once
#include "qio.h"

// What is used to offset a command from arguments
#define EVENT_DELIMITER ":"

// Protocol commands that need to be interpreted as regular commands
#define EVENT_PING "ping" EVENT_DELIMITER
#define EVENT_SUB "sub" EVENT_DELIMITER

// Responses to requests
#define EVENT_RESPONSE_INVALID_SUBSCRIPTION "invalid" EVENT_DELIMITER
#define EVENT_RESPONSE_MAX_SUBSCRIPTIONS "too_many_subscriptions" EVENT_DELIMITER
#define EVENT_RESPONSE_ALREADY_SUBSCRIBED "already_subscribed" EVENT_DELIMITER
#define EVENT_RESPONSE_CANNOT_UNSUBSRIBE "cannot_unsubscribe" EVENT_DELIMITER

/**
 * The different types of data that an event can contain.
 */
enum data_t {
	d_plain,
	d_json,
};

/**
 * All the information an event needs.
 */
typedef struct event_s {
	// The underlying buffer that the event is constructed from
	// This MUST be freed
	// Being able to access the full event name is a side-effect of
	// processing the event string, and it's rather useful
	gchar *name;
	
	// The extra event segments to be passed to the event handler,
	// not including the event itself.
	//
	// So, if the event is: /some/test/event, and the handler is /some,
	// then this will be ["test", "event"]
	GList *extra_segments;
	
	// The number of extra segments, so that clients don't have to iterate
	// to find the length
	guint extra_segments_len;
	
	// The callback number the client sent
	guint callback;
	
	// The type of data sitting in the data buffer
	enum data_t type;
	
	// The data string sent from the client (this is part of *buffer)
	gchar *data;
} event_t;

/**
 * The command function type.
 */
typedef status_t(*handler_fn)(client_t*, message_t*, event_t*);

/**
 * Handle an event from a client.
 */
status_t evs_server_handle(client_t*);

/**
 * The "sub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t evs_server_subscribe(client_t*, message_t*, event_t*);

/**
 * The "unsub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t evs_server_unsubscribe(client_t*, message_t*, event_t*);

/**
 * Listen for an event from clients.
 *
 * The ":" character is not allowed in event names.
 */
MODULE_EXPORT void evs_server_on(gchar*, handler_fn);

/**
 * Init the command interface
 */
gboolean evs_server_init();