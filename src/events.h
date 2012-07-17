#pragma once
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
 * Handle a command.
 *
 * Returns CLIENT_* status messages.  command->buffer will be set to what should
 * be sent back to the client, or will be empty (command->buffer->len == 0).
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
 * Add a command handler.
 */
void events_add(gchar*, commandfn_t);

/**
 * Init the command interface
 */
gboolean events_init();