#pragma once
#include "client.h"
#include "debug.h"

// What is used to offset a command from arguments
#define COMMAND_DELIMITER ":"

// Protocol commands that need to be interpreted as regular commands
#define COMMAND_PING "ping" COMMAND_DELIMITER
#define COMMAND_SUB "sub" COMMAND_DELIMITER

// Responses to requests
#define COMMAND_RESPONSE_INVALID_SUBSCRIPTION "invalid" COMMAND_DELIMITER
#define COMMAND_RESPONSE_MAX_SUBSCRIPTIONS "too_many_subscriptions" COMMAND_DELIMITER
#define COMMAND_RESPONSE_ALREADY_SUBSCRIBED "already_subscribed" COMMAND_DELIMITER
#define COMMAND_RESPONSE_CANNOT_UNSUBSRIBE "cannot_unsubscribe" COMMAND_DELIMITER

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
status_t command_handle(client_t*);

/**
 * The "sub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t command_subscribe(client_t*, message_t*);

/**
 * The "unsub:1477" command.
 *
 * This is made public so that the command can be wrapped by other commands.
 */
status_t command_unsubscribe(client_t*, message_t*);

/**
 * Add a command handler.
 */
void command_add(gchar*, commandfn_t);

/**
 * Init the command interface
 */
gboolean commands_init();