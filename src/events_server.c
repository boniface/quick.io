#include "qio.h"

/**
 * A table of all of the commands that a client can send.
 */
static GHashTable* _events;

/**
 * Given a message, it parses out all the different aspects of the event
 * and populates the event
 */
static status_t _event_new(message_t *message, event_info_t **handler, event_t *event) {
	// Clear out the event we're setting
	memset(event, 0, sizeof(*event));
	
	// Pointers to:
	// 1) The place we are currently in the buffer
	// 2) The end of the current buffer segment
	// 3) An end pointer that can be trashed and moved without affecting other parsing
	gchar *curr, *end, *tmp_end;
	
	// Steal the string from the buffer and replace it with a new, empty buffer
	event->name = message->buffer->str;
	
	// Pointer to the end of the buffer, for error checking
	gchar *buffer_end = event->name + message->buffer->len - 1;
	
	// Replace what we just took in the message, for responses
	g_string_free(message->buffer, FALSE);
	message->buffer = g_string_sized_new(0);
	
	// Make sure the event was given in the correct format:
	// event:msgId:type=data
	
	// Start by reading out the event specifier
	curr = event->name;
	end = tmp_end = g_strstr_len(event->name, -1, EVENT_DELIMITER);
	
	// If the first : in the string was the first character....
	if (end == NULL || curr == end) {
		DEBUG("Bad event specifier.");
		return CLIENT_BAD_MESSAGE;
	}
	
	// Null terminate our event
	*end = '\0';
	
	// Attempt to find what handles this event
	*handler = evs_server_get_einfo(curr, &(event->extra_segments), &(event->extra_segments_len));
	
	// If there isn't handler, that's wrong
	if (*handler == NULL) {
		return CLIENT_UNKNOWN_EVENT;
	}
	
	// Time to parse out the message Id
	curr = end + 1;
	end = g_strstr_len(curr, -1, EVENT_DELIMITER);
	
	if (end == NULL) {
		DEBUG("Bad messageId");
		return CLIENT_BAD_MESSAGE;
	}
	
	// Make the sub-string NULL terminated so this works
	*end = '\0';
	event->callback = g_ascii_strtoull(curr, NULL, 10);
	
	// And the data type
	curr = end + 1;
	end = g_strstr_len(curr, -1, "=");
	
	// There has to be something found for this to work
	if (end == NULL) {
		DEBUG("Bad message format: data type");
		return CLIENT_BAD_MESSAGE;
	}
	
	// To allow the string compares to work
	*end = '\0';
	
	if (g_strcmp0(curr, "plain") == 0) {
		event->type = d_plain;
	} else if (g_strcmp0(curr, "json") == 0) {
		event->type = d_json;
	} else {
		DEBUG("No matching data type found");
		return CLIENT_BAD_MESSAGE;
	}
	
	// Don't allow memory to go rampant
	if (++end > buffer_end) {
		DEBUG("Memory overrun");
		return CLIENT_BAD_MESSAGE;
	}
	
	// Finally, set the pointer to the data, everything following the "="
	event->data = end;
	
	return CLIENT_GOOD;
}

/**
 * Free up all the allocated memory for an event.
 */
static void _event_free(event_t *event) {
	// Since all the pointers in the event struct are just into the original buffer,
	// we don't need to free anything but this one string, and everything else is done
	free(event->name);
	
	if (event->extra_segments != NULL) {
		g_list_free(event->extra_segments);
	}
}

/**
 * The "ping:" command
 */
static status_t _evs_server_ping(client_t *client, event_t *event, GString *response) {
	// This command just needs to send back whatever text we recieved
	return CLIENT_WRITE;
}

/**
 * Send a message to the channel a user is subscribed to.
 *
 * Syntax: "send:1477:some random message string"
 */
static status_t _evs_server_send(client_t *client, event_t *event, GString *response) {
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

event_info_t* evs_server_get_einfo(gchar *curr, GList **extra, guint *extra_len) {
	event_info_t *handler = NULL;
	
	// Get a pointer to the end of the string
	gchar *end = curr + strlen(curr);
	
	// Move from the end of the string to the front, looking for less-specific
	// event specifiers as we move on
	while (end > curr) {
		// Make the end pointer NULL terminate the string so that we can use
		// the buffer as a series of strings
		*end = '\0';
		
		// Attempt to find the handler
		handler = (event_info_t*)g_hash_table_lookup(_events, curr);
		
		// If we found the most-specific command
		if (handler != NULL && handler->handle_children) {
			break;
		}
		
		// Rewind the end pointer to the next '/'
		while (*(--end) != '/' && end > curr);
		
		// If the event is in the form "/some/event/" (trailing slash), this
		// could be null, so ignore
		if (*(end + 1) == '\0') {
			continue;
		}
		
		// If it falls through here, then no command was found, so plop the current
		// segment onto the extra segments list
		// +1 -> the iterator moves onto the '/', and we don't want that in the segment
		*extra = g_list_prepend(*extra, end + 1);
		(*extra_len)++;
	}
	
	// If we made it to the beginning of the string, we couldn't find the event
	// (also implies that *handler == NULL)
	return handler;
}

status_t evs_server_subscribe(client_t *client, event_t *event, GString *response) {
	#warning create test case for empty responses when they should be empty
	DEBUGF("event_subscribe: %s", event->data);
	
	// External clients aren't allowed to know about UNSUBSCRIBED
	if (strcmp(event->data, UNSUBSCRIBED) == 0) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	status_t status = evs_client_sub_client(event->data, client);
	// Attempt to subscribe the client to the event
	if (status == CLIENT_INVALID_SUBSCRIPTION) {
		// The event was invalid, inform the client
		g_string_prepend(response, EVENT_RESPONSE_INVALID_SUBSCRIPTION);
		return CLIENT_WRITE;
	} else if (status == CLIENT_TOO_MANY_SUBSCRIPTIONS) {
		// The client is subscribed to the maximum number of rooms already, may not add any more
		g_string_prepend(response, EVENT_RESPONSE_MAX_SUBSCRIPTIONS);
		return CLIENT_WRITE;
	} else if (status == CLIENT_ALREADY_SUBSCRIBED) {
		// Why is he subscribing again?
		g_string_prepend(response, EVENT_RESPONSE_ALREADY_SUBSCRIBED);
		return CLIENT_WRITE;
	}
	
	// The event was valid, so there's nothing more to do
	return CLIENT_GOOD;
}

status_t evs_server_unsubscribe(client_t *client, event_t *event, GString *response) {
	#warning create test case for empty responses when they should be empty
	DEBUGF("event_unsubscribe: %s", event->data);
	
	// External clients aren't allowed to know about UNSUBSCRIBED
	if (strcmp(event->data, UNSUBSCRIBED) == 0) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	status_t status = evs_client_unsub_client(event->data, client);
	
	if (status == CLIENT_CANNOT_UNSUBSCRIBE) {
		g_string_prepend(response, EVENT_RESPONSE_CANNOT_UNSUBSRIBE);
		return CLIENT_WRITE;
	}
	
	return CLIENT_GOOD;
}

status_t evs_server_handle(client_t *client) {
	#warning allow forcing apps to namespaces
	
	event_t event;
	event_info_t *einfo = NULL;
	status_t status = _event_new(client->message, &einfo, &event);
	
	DEBUGF("Event: %s", event.name);
	
	// If everything went according to plan, then there's a handler and it's safe to
	// send the handler everything
	if (status == CLIENT_GOOD) {
		// The client->message->buffer is now empty, as free'd by _event_new
		#warning Create test case to verify buffer is empty after _event_new
		status = einfo->handler(client, &event, client->message->buffer);
	}
	
	_event_free(&event);
	return status;
}

void evs_server_on(const gchar *event_name, handler_fn handler, on_subscribe_cb on_subscribe, on_subscribe_cb on_unsubscribe, gboolean handle_children) {
	// Don't allow events with EVENT_DELIMITER in the name
	if (g_strstr_len(event_name, -1, EVENT_DELIMITER) != NULL) {
		ERRORF("Could not add event \"%s\", \":\" not allowed in event names.", event_name);
		return;
	}
	
	// Make sure the event starts with a "/"
	if (*event_name != '/') {
		ERRORF("Could not add event \"%s\", event must start with \"/\"", event_name);
		return;
	}
	
	// Remove any duplicated slashes
	GString *event = g_string_new(event_name);
	gchar prev = event->str[0];
	gsize i = 1;
	while (i < event->len) {
		if (event->str[i] == '/' && prev == '/') {
			// Remove the single slash
			g_string_erase(event, i, 1);
		} else {
			prev = event->str[i];
			i++;
		}
	}
	
	// Remove any trailing slashes
	if (event->str[--i] == '/') {
		g_string_erase(event, i, 1);
	}
	
	// Grab some memory to store the event info
	event_info_t *einfo = malloc(sizeof(*einfo));
	einfo->handler = handler;
	einfo->on_subscribe = on_subscribe;
	einfo->on_unsubscribe = on_unsubscribe;
	einfo->handle_children = handle_children;
	
	// Store the handler for the event
	g_hash_table_insert(_events, event->str, einfo);
	
	// Don't free the string we just inserted into the hash table
	g_string_free(event, FALSE);
}

gboolean evs_server_init() {
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	
	evs_server_on("/ping", _evs_server_ping, NULL, NULL, FALSE);
	evs_server_on("/sub", evs_server_subscribe, NULL, NULL, FALSE);
	evs_server_on("/send", _evs_server_send, NULL, NULL, FALSE);
	evs_server_on("/unsub", evs_server_unsubscribe, NULL, NULL, FALSE);
	
	// Internal commands are ready, let the app register its commands.
	apps_register_events();
	
	return TRUE;
}