#include "qio.h"

/**
 * A table of all of the commands that a client can send, referenced by the path.
 */
static GHashTable* _events_by_path;

/**
 * A table of all of the commands that a client can send, referenced by the handler.
 */
static GHashTable* _events_by_handler;

/**
 * Given a message, it parses out all the different aspects of the event
 * and populates the event
 */
static status_t _event_new(message_t *message, event_handler_t **handler, event_t *event) {
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
		return CLIENT_BAD_MESSAGE_FORMAT;
	}
	
	// Null terminate our event
	*end = '\0';
	
	// Attempt to find what handles this event
	*handler = evs_server_get_handler(curr, &(event->extra_segments), &(event->extra_segments_len));
	
	// If there isn't handler, that's wrong
	if (*handler == NULL) {
		return CLIENT_UNKNOWN_EVENT;
	}
	
	// Time to parse out the message Id
	curr = end + 1;
	end = g_strstr_len(curr, -1, EVENT_DELIMITER);
	
	if (end == NULL) {
		DEBUG("Bad messageId");
		return CLIENT_BAD_MESSAGE_FORMAT;
	}
	
	// Make the sub-string NULL terminated so this works
	*end = '\0';
	
	// Support a message like: "/noop::plain="
	// There doesn't need to be a callback specified, but if there is one, and it's wrong,
	// that is still an error
	if (end - curr > 1) {
		gchar *endptr;
		event->callback = g_ascii_strtoull(curr, &endptr, 10);
		
		if (event->callback == 0 && endptr == curr) {
			DEBUG("Bad callback id");
			return CLIENT_BAD_MESSAGE_FORMAT;
		}
	} else {
		event->callback = 0;
	}
	
	// And the data type
	curr = end + 1;
	end = g_strstr_len(curr, -1, "=");
	
	// There has to be something found for this to work
	if (end == NULL) {
		DEBUG("Bad message format: data type");
		return CLIENT_BAD_MESSAGE_FORMAT;
	}
	
	// To allow the string compares to work -- overwrites the "="
	*end = '\0';
	
	if (g_strcmp0(curr, "plain") == 0) {
		event->type = d_plain;
	} else if (g_strcmp0(curr, "json") == 0) {
		event->type = d_json;
	} else {
		DEBUG("No matching data type found");
		return CLIENT_BAD_MESSAGE_FORMAT;
	}
	
	// Don't allow memory to go rampant
	if (++end > buffer_end) {
		event->data = "";
	} else {
		// Finally, set the pointer to the data, everything following the "="
		event->data = end;
	}
	
	return CLIENT_GOOD;
}

/**
 * Free up all the allocated memory for an event.
 */
static void _event_free(event_t *event) {
	// Since all the pointers in the event struct are just into the original buffer,
	// we don't need to free anything but this one string, and everything else is done
	free(event->name);
	event->name = NULL;
	
	if (event->extra_segments != NULL) {
		g_list_free_full(event->extra_segments, g_free);
		event->extra_segments = NULL;
	}
}

/**
 * Clean up the name of the event.
 *
 * @param event_path The path to clean
 *
 * @return The cleaned path. This MUST be g_free()'d when done.
 */
static gchar* _clean_event_name(const gchar *event_path) {
	// Remove any duplicated slashes
	GString *ep = g_string_new(event_path);
	gchar prev = ep->str[0];
	gsize i = 1;
	while (i < ep->len) {
		if (ep->str[i] == '/' && prev == '/') {
			// Remove the single slash
			g_string_erase(ep, i, 1);
		} else {
			prev = ep->str[i];
			i++;
		}
	}
	
	// Remove any trailing slashes
	if (ep->str[--i] == '/') {
		g_string_erase(ep, i, 1);
	}
	
	// Make sure it starts with a slash
	if (*(ep->str) != '/') {
		g_string_prepend_c(ep, '/');
	}
	
	gchar *path = ep->str;
	g_string_free(ep, FALSE);
	
	return path;
}

/**
 * The "ping:" command
 */
static status_t _evs_server_ping(const client_t *client, event_t *event, GString *response) {
	// This command just needs to send back whatever text we recieved
	g_string_append(response, event->data);
	return CLIENT_WRITE;
}

/**
 * Does nothing, just says "good" back.
 */
static status_t _evs_server_noop(const client_t *client, event_t *event, GString *response) {
	g_string_set_size(response, 0);
	if (event->callback) {
		return CLIENT_WRITE;
	} else {
		return CLIENT_GOOD;
	}
}

/**
 * Send a message to the channel a user is subscribed to.
 *
 * CURRENTLY DISABLED UNTIL I GET SOME BETTER IDEAS.
 *
 * Syntax: "send:1477:some random message string"
 */
/**
static status_t _evs_server_send(client_t *client, event_t *event, GString *response) {
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
	
	return CLIENT_GOOD;
}
*/

static status_t _evs_server_subscribe(const client_t *client, event_t *event, GString *response) {
	DEBUGF("event_subscribe: %s", event->data);
	
	gchar *event_path = _clean_event_name(event->data);
	
	// So we break a rule: subscribe has to change the client for subscriptions, so allow it
	status_t status = evs_client_sub_client(event_path, (client_t*)client);
	
	// Attempt to subscribe the client to the event
	if (status == CLIENT_INVALID_SUBSCRIPTION) {
		// The event was invalid, inform the client
		g_string_append(response, EVENT_RESPONSE_INVALID_SUBSCRIPTION);
		status = CLIENT_WRITE;
	} else if (status == CLIENT_TOO_MANY_SUBSCRIPTIONS) {
		// The client is subscribed to the maximum number of rooms already, may not add any more
		g_string_append(response, EVENT_RESPONSE_MAX_SUBSCRIPTIONS);
		status = CLIENT_WRITE;
	} else if (status == CLIENT_ALREADY_SUBSCRIBED) {
		// Why is he subscribing again?
		g_string_append(response, EVENT_RESPONSE_ALREADY_SUBSCRIBED);
		status = CLIENT_WRITE;
	}
	
	if (status == CLIENT_WRITE) {
		g_string_append_printf(response, ":%s", event_path);
	}
	
	g_free(event_path);
	
	// The event was valid, so there's nothing more to do
	return status;
}

static status_t _evs_server_unsubscribe(const client_t *client, event_t *event, GString *response) {
	DEBUGF("event_unsubscribe: %s", event->data);
	
	gchar *event_path = _clean_event_name(event->data);
	
	// So we break a rule: unsubscribe has to change the client for subscriptions, so allow it
	status_t status = evs_client_unsub_client(event_path, (client_t*)client);
	
	if (status == CLIENT_CANNOT_UNSUBSCRIBE) {
		g_string_printf(response, "%s:%s", EVENT_RESPONSE_CANNOT_UNSUBSCRIBE, event_path);
		status = CLIENT_WRITE;
	}
	
	g_free(event_path);
	
	return status;
}

event_handler_t* evs_server_get_handler(const gchar *event_path, path_extra_t *extra, guint16 *extra_len) {
	event_handler_t *handler = NULL;
	
	// Internal extras
	path_extra_t iextra = NULL;
	guint16 iextra_len = 0;
	
	// We cannot modify event_path, so let's modify this!
	gchar *ep = g_strdup(event_path);
	
	// Get a pointer to the end of the string
	gchar *end = ep + strlen(ep);
	
	// Move from the end of the string to the front, looking for less-specific
	// event specifiers as we move on
	while (end > ep) {
		// Make the end pointer NULL terminate the string so that we can use
		// the buffer as a series of strings
		*end = '\0';
		
		// Attempt to find the handler
		handler = (event_handler_t*)g_hash_table_lookup(_events_by_path, ep);
		
		// If we found the most-specific command
		// This one is a bit hard to read: if there is a handler and any extra parameters,
		// then make sure the handler supports it; if there are no extra parameters, then
		// it's good
		if (handler != NULL && (iextra_len == 0 || (iextra_len > 0 && handler->handle_children))) {
			break;
		}
		
		// Since the handler wasn't the right one, reset it to NULL so that it won't be
		// accidentally returned
		handler = NULL;
		
		// Rewind the end pointer to the next '/'
		// Important: don't allow this to pass the start of the string -- this also changes
		// the condition for the while loop
		while (*(--end) != '/' && end > ep);
		
		// If the event is in the form "/some/event/" (trailing slash), this
		// could be null, so ignore
		if (*(end + 1) == '\0') {
			continue;
		}
		
		// If it falls through here, then no command was found, so plop the current
		// segment onto the extra segments list
		// +1 -> the iterator moves onto the '/', and we don't want that in the segment
		gchar *c = g_strdup(end + 1);
		
		iextra = g_list_prepend(iextra, c);
		iextra_len++;
	}
	
	if (handler == NULL && iextra != NULL) {
		g_list_free_full(iextra, g_free);
		iextra = NULL;
		iextra_len = 0;
	}
	
	if (extra != NULL) {
		*extra = iextra;
	}
	
	if (extra_len != NULL) {
		*extra_len = iextra_len;
	}
	
	g_free(ep);
	
	// If we made it to the beginning of the string, we couldn't find the event
	// (also implies that handler == NULL)
	return handler;
}

status_t evs_server_handle(client_t *client) {
	event_t event;
	event_handler_t *handler = NULL;
	status_t status = _event_new(client->message, &handler, &event);
	
	DEBUGF("Event: %s", event.name);
	
	// If everything went according to plan, then there's a handler and it's safe to
	// send the handler everything
	if (status == CLIENT_GOOD) {
		if (handler->fn != NULL) {
			// The client->message->buffer is now empty, as free'd by _event_new
			status = handler->fn(client, &event, client->message->buffer);
		}
	}
	
	// Prepare the event for writing back to the client
	if (status == CLIENT_WRITE) {
		// If there is data, then it needs to be stolen from the buffer
		// Otherwise, there could be all sorts of issues with formatting
		// the buffer using what's already in the buffer
		gchar *data;
		if (client->message->buffer->len == 0) {
			data = "";
		} else {
			data = client->message->buffer->str;
			g_string_free(client->message->buffer, FALSE);
			client->message->buffer = g_string_sized_new(100);
		}
		
		#warning TODO: Server callbacks
		status = evs_client_format_message(handler, event.callback, 0, event.extra_segments, event.type, data, client->message->buffer);
		
		// If we get CLIENT_GOOD back from format_message, then we still need it to be
		// CLIENT_WRITE, otherwise we want to send back the error message
		if (status == CLIENT_GOOD) {
			status = CLIENT_WRITE;
		}
		
		// len == 0 results in this being null, so it won't need free'd
		if (*data != '\0') {
			g_free(data);
		}
	}
	
	_event_free(&event);
	return status;
}

event_handler_t* evs_server_on(const gchar *event_path, const handler_fn fn, const on_subscribe_cb on_subscribe, const on_subscribe_cb on_unsubscribe, const gboolean handle_children) {
	
	// Don't allow events with EVENT_DELIMITER in the name
	if (g_strstr_len(event_path, -1, EVENT_DELIMITER) != NULL) {
		ERRORF("Could not add event \"%s\", \""EVENT_DELIMITER"\" not allowed in event names.", event_path);
		return NULL;
	}
	
	// Don't allow events with EVENT_DATA_DELIMITER in the name
	if (g_strstr_len(event_path, -1, EVENT_DATA_DELIMITER) != NULL) {
		ERRORF("Could not add event \"%s\", \""EVENT_DATA_DELIMITER"\" not allowed in event names.", event_path);
		return NULL;
	}
	
	gchar *path = _clean_event_name(event_path);
	
	// Grab some memory to store the event info
	event_handler_t *handler = g_try_malloc0(sizeof(*handler));
	handler->fn = fn;
	handler->on_subscribe = on_subscribe;
	handler->on_unsubscribe = on_unsubscribe;
	handler->handle_children = handle_children;
	
	// Store the handler for the event
	g_hash_table_insert(_events_by_path, path, handler);
	g_hash_table_insert(_events_by_handler, handler, path);
	
	return handler;
}

gchar* evs_server_path_from_handler(const event_handler_t *handler) {
	return g_hash_table_lookup(_events_by_handler, handler);
}

gboolean evs_server_init() {
	_events_by_path = g_hash_table_new(g_str_hash, g_str_equal);
	_events_by_handler = g_hash_table_new(NULL, NULL);
	
	evs_server_on("/qio/ping", _evs_server_ping, NULL, NULL, FALSE);
	evs_server_on("/qio/sub", _evs_server_subscribe, NULL, NULL, FALSE);
	evs_server_on("/qio/unsub", _evs_server_unsubscribe, NULL, NULL, FALSE);
	evs_server_on("/qio/noop", _evs_server_noop, NULL, NULL, FALSE);
	// evs_server_on("/send", _evs_server_send, NULL, NULL, FALSE);
	
	// Internal commands are ready, let the app register its commands.
	apps_register_events();
	
	return TRUE;
}

#ifdef TESTING
#include "../test/test_events_server.c"
#endif