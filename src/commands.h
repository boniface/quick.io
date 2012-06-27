#pragma once
#include "client.h"
#include "commands.h"
#include "debug.h"

// What is used to offset a command from arguments
#define COMMAND_DELIMITER ":"

// Protocol commands that need to be interpreted as regular commands
#define COMMAND_PING "ping" COMMAND_DELIMITER

/**
 * Handle a a command.
 *
 * Returns CLIENT_* status messages.  command->buffer will be set to what should
 * be sent back to the client, or will be empty (command->buffer->len == 0).
 */
short command_handle(command_t*);

/**
 * Init the command interface
 */
gboolean commands_init();