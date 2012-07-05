#include <string.h>
#include <stdlib.h>

#include "app.h"
#include "client.h"
#include "commands.h"
#include "pubsub.h"

/**
 * The command function type.
 */
typedef status_t(*commandfn_t)(client_t*, message_t*);

/**
 * A table of all of the commands that a client can send.
 */
static GHashTable* commands;

/**
 * Given a message, removes everything before the ":" and returns it
 * while removing it from the message body.  Returns NULL if no ":" is found,
 * and the message is not touched.
 *
 * The result, if not NULL, MUST be free()'d.
 */
static gchar* _slice(message_t *message) {
	GString *buff = message->buffer;
	gsize delim = strcspn(buff->str, COMMAND_DELIMITER);
	
	// If no command was found (ie. no delimiter, nothing before the delimeter)
	if (delim == buff->len) {
		return NULL;
	}
	
	// Allocate +1 for the null-terminator
	gchar *cmd = malloc((delim + 1) * sizeof(*cmd));
	
	// Well, we cant allocate, so just fall through.
	if (cmd == NULL) {
		return NULL;
	}
	
	cmd[delim] = 0; // Null-terminate it
	
	// Copy the command to the return string
	strncpy(cmd, buff->str, delim);
	
	// Remove the command from the message buffer so the handlers can process more easily
	g_string_erase(message->buffer, 0, delim + 1);
	
	return cmd;
}

/**
 * The "ping:" command
 */
static status_t command_ping(client_t *client, message_t *message) {
	// This command just needs to send back whatever text we recieved
	return CLIENT_WRITE;
}

/**
 * The "sub:1477" command
 */
static status_t command_subscribe(client_t *client, message_t *message) {
	#warning create test case for empty responses when they should be empty
	DEBUGF("command_subscribe: %s", message->buffer->str);
	
	status_t status = sub_client(message->buffer->str, client);
	// Attempt to subscribe the client to the room
	if (status == CLIENT_INVALID_SUBSCRIPTION) {
		// The room was invalid, inform the client
		g_string_prepend(message->buffer, COMMAND_RESPONSE_INVALID_SUBSCRIPTION);
	} else if (status == CLIENT_TOO_MANY_SUBSCRIPTIONS) {
		// The client is subscribed to the maximum number of rooms already, may not add any more
		g_string_prepend(message->buffer, COMMAND_RESPONSE_MAX_SUBSCRIPTIONS);
	} else {
		// The room was valid, so the client doesn't get anything back
		g_string_truncate(message->buffer, 0);
	}
	
	return CLIENT_GOOD;
}

/**
 * Send a message to the channel a user is subscribed to.
 *
 * Syntax: "send:1477:some random message string"
 */
static status_t command_send(client_t *client, message_t *message) {
	gchar *room = _slice(message);
	
	if (room == NULL) {
		return CLIENT_BAD_COMMAND;
	}
	
	DEBUGF("command_send: %s; message: %s", room, message->buffer->str);
	
	status_t status = pub_message(room, message);
	free(room);
	
	// We don't send anything back for a send...that's handled transparently for us
	g_string_truncate(message->buffer, 0);
	
	return status;
}

status_t command_handle(client_t *client) {
	message_t *message = client->message;
	gchar *cmd = _slice(message);
	
	// Was there a command in the string?
	if (cmd == NULL) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	// Get the command from the hash table of commands
	commandfn_t fn = g_hash_table_lookup(commands, cmd);
	if (fn == NULL) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	// Clean up the command
	free(cmd);
	
	return (*fn)(client, message);
}

gboolean commands_init() {
	commands = g_hash_table_new(g_str_hash, g_str_equal);
	
	g_hash_table_insert(commands, "ping", command_ping);
	g_hash_table_insert(commands, "sub", command_subscribe);
	g_hash_table_insert(commands, "send", command_send);
	
	// Internal commands are ready, let the app register its commands.
	app_register_commands();
	
	return TRUE;
}