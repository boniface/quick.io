#pragma once

#include <glib.h>

#include "client.h"
#include "debug.h"

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
 * The command function type.
 */
typedef status_t(*commandfn_t)(client_t*, message_t*);

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
	gchar *buffer;
	
	// The event segments: /test/event => ['test', 'event']
	// The items are all pointers into *buffer
	GPtrArray *segments;
	
	// The callback number the client sent
	guint callback;
	
	// The type of data sitting in the data buffer
	enum data_t type;
	
	// The data string sent from the client (this is part of *buffer)
	gchar *data;
} event_t;

/**
 * Handle an event from a client.
 */
status_t events_handle(client_t*);

/**
 * The "sub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t events_subscribe(client_t*, message_t*);

/**
 * The "unsub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t events_unsubscribe(client_t*, message_t*);

/**
 * Listen for an event from clients.
 *
 * The ":" character is not allowed in event names.
 */
void events_on(gchar*, commandfn_t);

/**
 * Init the command interface
 */
gboolean events_init();