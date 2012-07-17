#include <string.h>
#include <stdlib.h>

#include "apps.h"
#include "client.h"
#include "events.h"
#include "pubsub.h"

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
	gsize delim = strcspn(buff->str, EVENT_DELIMITER);
	
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
static status_t _events_ping(client_t *client, message_t *message) {
	// This command just needs to send back whatever text we recieved
	return CLIENT_WRITE;
}

/**
 * Send a message to the channel a user is subscribed to.
 *
 * Syntax: "send:1477:some random message string"
 */
static status_t _events_send(client_t *client, message_t *message) {
	gchar *room = _slice(message);
	
	if (room == NULL) {
		return CLIENT_BAD_COMMAND;
	}
	
	DEBUGF("event_send: %s; message: %s", room, message->buffer->str);
	
	status_t status = pub_message(room, message);
	free(room);
	
	// We don't send anything back for a send...that's handled transparently for us
	g_string_truncate(message->buffer, 0);
	
	return status;
}

status_t events_subscribe(client_t *client, message_t *message) {
	#warning create test case for empty responses when they should be empty
	DEBUGF("event_subscribe: %s", message->buffer->str);
	
	// External clients aren't allowed to know about UNSUBSCRIBED
	if (strcmp(message->buffer->str, UNSUBSCRIBED) == 0) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	status_t status = sub_client(message->buffer->str, client);
	// Attempt to subscribe the client to the event
	if (status == CLIENT_INVALID_SUBSCRIPTION) {
		// The event was invalid, inform the client
		g_string_prepend(message->buffer, EVENT_RESPONSE_INVALID_SUBSCRIPTION);
		return CLIENT_WRITE;
	} else if (status == CLIENT_TOO_MANY_SUBSCRIPTIONS) {
		// The client is subscribed to the maximum number of rooms already, may not add any more
		g_string_prepend(message->buffer, EVENT_RESPONSE_MAX_SUBSCRIPTIONS);
		return CLIENT_WRITE;
	} else if (status == CLIENT_ALREADY_SUBSCRIBED) {
		// Why is he subscribing again?
		g_string_prepend(message->buffer, EVENT_RESPONSE_ALREADY_SUBSCRIBED);
		return CLIENT_WRITE;
	}
	
	// The event was valid, so there's nothing more to do
	return CLIENT_GOOD;
}

status_t events_unsubscribe(client_t *client, message_t *message) {
	#warning create test case for empty responses when they should be empty
	DEBUGF("event_unsubscribe: %s", message->buffer->str);
	
	// External clients aren't allowed to know about UNSUBSCRIBED
	if (strcmp(message->buffer->str, UNSUBSCRIBED) == 0) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	status_t status = sub_unsub_client(message->buffer->str, client);
	
	if (status == CLIENT_CANNOT_UNSUBSCRIBE) {
		g_string_prepend(message->buffer, EVENT_RESPONSE_CANNOT_UNSUBSRIBE);
		return CLIENT_WRITE;
	}
	
	return CLIENT_GOOD;
}

status_t events_handle(client_t *client) {
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

void events_add(gchar *event_name, commandfn_t fn) {
	g_hash_table_insert(commands, event_name, fn);
}

gboolean events_init() {
	commands = g_hash_table_new(g_str_hash, g_str_equal);
	
	events_add("ping", _events_ping);
	events_add("sub", events_subscribe);
	events_add("send", _events_send);
	events_add("unsub", events_unsubscribe);
	
	// Internal commands are ready, let the app register its commands.
	apps_register_commands();
	
	return TRUE;
}