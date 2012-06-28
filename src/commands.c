#include <string.h>
#include <stdlib.h>

#include "client.h"
#include "commands.h"

/**
 * The command function type.
 */
typedef status_t(*commandfn_t)(client_t*, message_t*);

/**
 * A table of all of the commands that a client can send.
 */
static GHashTable* commands;

/**
 * The "ping:" command
 */
static status_t command_ping(client_t *client, message_t *message) {
	// This command just needs to send back whatever text we recieved
	return CLIENT_GOOD;
}

/**
 * The "sub:1477" command
 */
static status_t command_subscribe(client_t *client, message_t *message) {
	// Verify that the argument to us is actually an integer
	int station = (int)g_ascii_strtoll(message->buffer->str, NULL, 10);
	
	if (station == 0) {
		return CLIENT_BAD_COMMAND;
	}
	
	// We don't send anything back to the client
	g_string_truncate(message->buffer, 0);
	
	#warning create test case for empty responses when they should be empty
	
	DEBUGF("%d", station);
	
	return CLIENT_GOOD;
}

status_t command_handle(client_t *client) {
	message_t *message = client->message;
	
	GString *buff = message->buffer;
	int delim = strcspn(buff->str, COMMAND_DELIMITER);
	
	// If no command was found (ie. no delimiter, nothing before the delimeter)
	if (delim == buff->len) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	char cmd[delim+1];
	cmd[delim] = 0; // Null-terminate it
	strncpy(cmd, buff->str, delim);
	
	// Get the command from the hash table of commands
	commandfn_t fn = g_hash_table_lookup(commands, cmd);
	if (fn == NULL) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	// Remove the command from the message buffer so the handlers can process more easily
	g_string_erase(message->buffer, 0, sizeof(cmd));
	
	return (*fn)(client, message);
}

gboolean commands_init() {
	commands = g_hash_table_new(g_str_hash, g_str_equal);
	
	g_hash_table_insert(commands, "ping", command_ping);
	g_hash_table_insert(commands, "sub", command_subscribe);
	
	return TRUE;
}