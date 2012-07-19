#include <string.h>
#include <stdlib.h>

#include "apps.h"
#include "client.h"
#include "events.h"
#include "pubsub.h"

/**
 * A table of all of the commands that a client can send.
 */
static GHashTable* _events;

/**
 * Given a message, it parses out all the different aspects of the event
 * and populates the *event
 */
static gboolean _event_new(message_t *message, event_t *event) {
	// Clear out the event we're setting
	memset(event, 0, sizeof(event));
	
	// A pointer to the part of the event we are currently processing, and some end segment
	gchar *curr, *end;
	
	// Steal the string from the buffer and replace it with a new, empty buffer
	event->buffer = message->buffer->str;
	g_string_free(message->buffer, FALSE);
	message->buffer = g_string_sized_new(0);
	
	// Make sure the event was given in the correct format:
	// event:msgId:type=data
	
	// Begin by chopping off the data, the processing of the data is left
	// to the individual handlers
	curr = g_strstr_len(event->buffer, -1, "=");
	if (curr == NULL) {
		DEBUG("Bad data");
		return FALSE;
	}
	
	// Set the pointer to the beginning of the data
	// The worst that can happen is that data is NULL
	event->data = curr + 1;
	
	// Null-terminate the end of the event spec string
	*curr = '\0';
	
	// Get the event path -- events start at the beginning of the string and go to the first ":"
	curr = g_strstr_len(event->buffer, -1, ":");
	if (curr == NULL) {
		DEBUG("Bad event path");
		return FALSE;
	}
	
	// Null-terminate the event path before splitting it
	*curr = '\0';
	
	// Break up the events into their paths
	event->segments = g_ptr_array_new();
	gchar *seg;
	
	// Prime the tokenizer
	seg = strtok(event->buffer, "/");
	while (seg != NULL) {
		g_ptr_array_add(event->segments, seg);
		
		if (event->segments->len > option_max_event_depth()) {
			DEBUGF("Too many segments: %d > %d", event->segments->len, option_max_event_depth());
			return FALSE;
		}
		
		// Any subsequent calls to strtok should take a NULL to finish the previous string
		seg = strtok(NULL, "/");
	}
	
	// Parse out the messageId for callbacks -- the second parameter
	curr++; // Advance past the NULL terminator
	end = g_strstr_len(curr, -1, ":");
	
	// NULL terminate the msgId string before converting to a number
	*end = '\0';
	
	// Advance past the NULL terminator after the msgId
	end++;
	
	event->callback = g_ascii_strtoull(curr, NULL, 10);
	
	// Find the type of the data, this substring will be NULL terminated (from
	// cutting off the data)
	
	if (g_strcmp0(end, "plain") == 0) {
		event->type = d_plain;
	} else if (g_strcmp0(end, "json") == 0) {
		event->type = d_json;
	} else {
		return FALSE;
	}
	
	return TRUE;
}

/**
 * Free up all the allocated memory for an event.
 */
static void _event_free(event_t *event) {
	free(event->buffer);
	
	if (event->segments != NULL) {
		g_ptr_array_free(event->segments, TRUE);
	}
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
	// gchar *room = _slice(message);
	
	// if (room == NULL) {
	// 	return CLIENT_BAD_COMMAND;
	// }
	
	// DEBUGF("event_send: %s; message: %s", room, message->buffer->str);
	
	// status_t status = pub_message(room, message);
	// free(room);
	
	// // We don't send anything back for a send...that's handled transparently for us
	// g_string_truncate(message->buffer, 0);
	
	// return status;
	
	return CLIENT_GOOD;
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
	
	// #error Namespace events: /ihr/thumbs/1477, allow forcing apps to namespaces
	
	event_t event;
	if (!_event_new(message, &event)) {
		_event_free(&event);
		return CLIENT_BAD_MESSAGE;
	}
	
	DEBUGF("Event: %s", event.buffer);
	
	return CLIENT_GOOD;
	
	// // Get the command from the hash table of commands
	// commandfn_t fn = g_hash_table_lookup(commands, cmd);
	// if (fn == NULL) {
	// 	return CLIENT_UNKNOWN_COMMAND;
	// }
	
	// return (*fn)(client, message);
}

void events_on(gchar *event_name, commandfn_t fn) {
	// Don't allow events with ":" in the name
	if (g_strstr_len(event_name, -1, ":") != NULL) {
		ERRORF("Could not add event \"%s\", \":\" not allowed in event names.", event_name);
		return;
	}
	
	if (*event_name != '/') {
		ERRORF("Could not add event \"%s\", event must start with \"/\"", event_name);
		return;
	}
	
	g_hash_table_insert(_events, event_name, fn);
}

gboolean events_init() {
	_events = g_hash_table_new(g_str_hash, g_str_equal);
	
	events_on("/ping", _events_ping);
	events_on("/sub", events_subscribe);
	events_on("/ssend", _events_send);
	events_on("/unsub", events_unsubscribe);
	
	// Internal commands are ready, let the app register its commands.
	apps_register_events();
	
	return TRUE;
}