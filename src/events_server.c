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
 * Since event registration can happen in many different threads simultaneously,
 * we need to lock the hash tables.
 */
static GMutex _events_lock;

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
	event->path = message->buffer->str;
	
	// Pointer to the end of the buffer, for error checking
	gchar *buffer_end = event->path + message->buffer->len - 1;
	
	// Replace what we just took in the message, for responses
	g_string_free(message->buffer, FALSE);
	message->buffer = g_string_sized_new(STRING_BUFFER_SIZE);
	
	// Make sure the event was given in the correct format:
	// event:msgId:type=data
	
	// Start by reading out the event specifier
	curr = event->path;
	end = tmp_end = g_strstr_len(event->path, -1, EVENT_DELIMITER);
	
	// If the first : in the string was the first character....
	if (end == NULL || curr == end) {
		DEBUG("Bad event specifier.");
		return CLIENT_FATAL;
	}
	
	// Null terminate our event
	*end = '\0';
	
	// Attempt to find what handles this event
	// ATTENTION: DO NOT DO THE HANDLER NULL CHECK HERE:
	// it is imperative that the client_callback be found if there
	// is one, so that we might return a proper error.
	// Our mandate is that if there is a callback, we will send something to it, no matter
	// what.
	*handler = evs_server_get_handler(curr, &(event->extra));
	
	// Time to parse out the message Id
	curr = end + 1;
	end = g_strstr_len(curr, -1, EVENT_DELIMITER);
	
	if (end == NULL) {
		DEBUG("Bad messageId");
		return CLIENT_FATAL;
	}
	
	// Make the sub-string NULL terminated so this works
	*end = '\0';
	
	// Support a message like: "/noop::plain="
	// There doesn't need to be a callback specified, but if there is one, and it's wrong,
	// that is still an error
	if (end - curr > 0) {
		gchar *endptr;
		event->client_callback = g_ascii_strtoull(curr, &endptr, 10);
		
		if (event->client_callback == 0 && endptr == curr) {
			DEBUG("Bad callback id");
			return CLIENT_FATAL;
		}
	} else {
		event->client_callback = 0;
	}
	
	// Since the callback has been parsed, we may now do the handler check
	if (*handler == NULL) {
		return CLIENT_ERROR;
	}
	
	// And the data type
	curr = end + 1;
	end = g_strstr_len(curr, -1, "=");
	
	// There has to be something found for this to work
	if (end == NULL) {
		DEBUG("Bad message format: data type");
		return CLIENT_FATAL;
	}
	
	// To allow the string compares to work -- overwrites the "="
	*end = '\0';
	
	if (g_strcmp0(curr, "plain") == 0) {
		event->data_type = d_plain;
	} else if (g_strcmp0(curr, "json") == 0) {
		event->data_type = d_json;
	} else {
		DEBUG("No matching data type found");
		return CLIENT_FATAL;
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
	free(event->path);
	event->path = NULL;
	
	if (event->extra != NULL) {
		g_ptr_array_unref(event->extra);
		event->extra = NULL;
	}
}

static void _evs_server_callback_free(struct client_cb_s cb) {
	// Get rid of the callback's data, that's over
	if (cb.data != NULL && cb.free_fn != NULL) {
		cb.free_fn(cb.data);
	}
}

static status_t _evs_server_callback(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	if (event->extra->len != 1) {
		return CLIENT_ERROR;
	}
	
	callback_t compacted = g_ascii_strtoull(g_ptr_array_index(event->extra, 0), NULL, 10);
	guint8 slot, id;
	SERVER_CALLBACK_PARTS(client, compacted, slot, id);
	
	qev_client_lock(client);
	
	// If a callback came through that was evicted, ignore it
	if (client->callbacks[slot].id != id) {
		qev_client_unlock(client);
		return CLIENT_ERROR;
	}
	
	// Get a copy of the callback so we can unlock faster
	struct client_cb_s cb = client->callbacks[slot];
	
	// Mark the callback as empty
	client->callbacks[slot].fn = NULL;
	
	qev_client_unlock(client);
	
	status_t status = cb.fn(client, cb.data, event);
	_evs_server_callback_free(cb);
	
	return status;
}

static status_t _evs_server_ping(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	// This command just needs to send back whatever text we recieved
	// Only do this if a callback is given (the logic in the handler takes
	// care of that)
	g_string_append(response, event->data);
	return CLIENT_GOOD;
}

static status_t _evs_server_noop(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	g_string_set_size(response, 0);
	return CLIENT_GOOD;
}

static status_t _evs_server_subscribe(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	DEBUG("event_subscribe: %s; callback: %u", event->data, event->client_callback);
	
	gchar *event_path = evs_server_format_path(event->data, NULL);
	status_t status = evs_client_sub_client(event_path, client, event->client_callback);
	g_free(event_path);
	
	return status;
}

static status_t _evs_server_unsubscribe(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	DEBUG("event_unsubscribe: %s", event->data);
	
	gchar *event_path = evs_server_format_path(event->data, NULL);
	status_t status = evs_client_unsub_client(event_path, client);
	g_free(event_path);
	
	return status;
}

event_handler_t* evs_server_get_handler(const gchar *event_path, path_extra_t **extra) {
	event_handler_t *handler = NULL;
	
	// Internal extras
	path_extra_t *iextra = g_ptr_array_new_with_free_func(g_free);
	
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
		if (handler != NULL && (iextra->len == 0 || (iextra->len > 0 && handler->handle_children))) {
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
		gchar *seg = g_strdup(end + 1);
		
		g_ptr_array_add(iextra, seg);
	}
	
	if (handler == NULL && iextra != NULL) {
		g_ptr_array_unref(iextra);
		iextra = NULL;
	}
	
	if (extra != NULL) {
		if (iextra != NULL) {
			// Reverse the array with a sort
			gint reverse(gconstpointer a, gconstpointer b) {
				return 1;
			}
			g_ptr_array_sort(iextra, reverse);
		}
		
		*extra = iextra;
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
	
	DEBUG("Event: %s", event.path);
	
	// If everything went according to plan, then there's a handler and it's safe to
	// send the handler everything
	if (status == CLIENT_GOOD && handler->fn != NULL) {
		// The client->message->buffer is now empty, as free'd by _event_new
		status = handler->fn(client, handler, &event, client->message->buffer);
	}
	
	// If there's a callback, and we're not going async, we MUST send something
	// back to the client
	if (status == CLIENT_ASYNC) {
		// CLIENT_ASYNC is ALWAYS good, and we're not sending anything: the handler
		// has taken full control at this point
		status = CLIENT_GOOD;
	} else if (event.client_callback > 0) {
		if (status == CLIENT_GOOD) {
			gchar *data = client->message->buffer->str;
			g_string_free(client->message->buffer, FALSE);
			client->message->buffer = g_string_sized_new(STRING_BUFFER_SIZE);
			
			status = evs_client_format_message(handler, event.client_callback, event.server_callback, event.extra, event.data_type, data, client->message->buffer);
			
			g_free(data);
		} else if (status == CLIENT_ERROR) {
			status = evs_client_format_message(handler, event.client_callback, 0, NULL, d_plain, QIO_ERROR, client->message->buffer);
		}
		
		if (status == CLIENT_GOOD) {
			// If everything went right in message formatting, then send it back to the client
			status = CLIENT_WRITE;
		} else {
			// If anything else happens, then just kill the client, there's no use at this point
			status = CLIENT_FATAL;
		}
	} else if (event.server_callback > 0) {
		// There's no client callback, but they attempted to send back a server callback...weird
		evs_server_callback_free(client, event.server_callback);
		
		// This should technically never happen, but just in case, we don't want things going
		// wrong above us
		if (status != CLIENT_GOOD) {
			status = CLIENT_FATAL;
		}
	}
	
	_event_free(&event);
	return status;
}

event_handler_t* evs_server_on(const gchar *event_path, const handler_fn fn, const on_subscribe_handler_cb on_subscribe, const on_unsubscribe_handler_cb on_unsubscribe, const gboolean handle_children) {
	// Don't allow events with EVENT_DELIMITER in the name
	if (g_strstr_len(event_path, -1, EVENT_DELIMITER) != NULL) {
		CRITICAL("Could not add event \"%s\", \""EVENT_DELIMITER"\" not allowed in event names.", event_path);
		return NULL;
	}
	
	// Don't allow events with EVENT_DATA_DELIMITER in the name
	if (g_strstr_len(event_path, -1, EVENT_DATA_DELIMITER) != NULL) {
		CRITICAL("Could not add event \"%s\", \""EVENT_DATA_DELIMITER"\" not allowed in event names.", event_path);
		return NULL;
	}
	
	gchar *path = evs_server_format_path(event_path, NULL);
	
	g_mutex_lock(&_events_lock);
	
	if (g_hash_table_contains(_events_by_path, path)) {
		CRITICAL("Event handler is already registered: %s", path);
		g_free(path);
		return NULL;
	}
	
	// Grab some memory to store the event info
	event_handler_t *handler = g_malloc0(sizeof(*handler));
	handler->fn = fn;
	handler->on_subscribe = on_subscribe;
	handler->on_unsubscribe = on_unsubscribe;
	handler->handle_children = handle_children;
	
	// Store the handler for the event
	g_hash_table_insert(_events_by_path, path, handler);
	g_hash_table_insert(_events_by_handler, handler, path);
	
	g_mutex_unlock(&_events_lock);
	
	return handler;
}

gchar* evs_server_path_from_handler(const event_handler_t *handler) {
	return g_hash_table_lookup(_events_by_handler, handler);
}

gchar* evs_server_format_path(const gchar *event_path, path_extra_t *extra) {
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
	
	// Handle any extra parameters
	if (extra != NULL) {
		for (guint i = 0; i < extra->len; i++) {
			g_string_append_printf(ep, "/%s", (gchar*)g_ptr_array_index(extra, i));
		}
	}
	
	gchar *path = ep->str;
	g_string_free(ep, FALSE);
	
	return path;
}

status_t evs_no_subscribe(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback) {
	return CLIENT_ERROR;
}

callback_t evs_server_callback_new(client_t *client, callback_fn fn, void *data, callback_free_fn free_fn) {
	
	if (fn == NULL) {
		CRITICAL("No callback function passed into evs_server_callback_new(). You probably didn't mean that.");
		
		if (data != NULL && free_fn != NULL) {
			free_fn(data);
		}
		
		return 0;
	}
	
	qev_client_lock(client);
	
	// Attempt to find an empty place to put the callback
	guint8 i = 0;
	for ( ; i < G_N_ELEMENTS(client->callbacks); i++) {
		if (client->callbacks[i].fn == NULL) {
			break;
		}
	}
	
	// Could not find an open callback slot, just evict at random
	if (i >= G_N_ELEMENTS(client->callbacks)) {
		i = (guint8)g_random_int_range(0, G_N_ELEMENTS(client->callbacks));
		_evs_server_callback_free(client->callbacks[i]);
	}
	
	client->callbacks[i].id++;
	client->callbacks[i].fn = fn;
	client->callbacks[i].data = data;
	client->callbacks[i].free_fn = free_fn;
	
	// Callbacks might overlap on the client: the callback might be active on the client
	// when we kill it, and there's a chance the client could send it to the server,
	// so to make sure we don't fire the wrong callback, we keep an ID in the callback
	// and incremement to indicate that it's new. Thus, when a callback comes in from
	// the client and we deconstruct it, if the ID doesn't match, we just discard it and
	// move on.
	callback_t id = SERVER_CALLBACK(i, client->callbacks[i].id);
	
	qev_client_unlock(client);
	
	return id;
}

void evs_server_callback_free(client_t *client, callback_t server_callback) {
	// Make sure we're not just killing a callback at random
	guint8 slot, id;
	SERVER_CALLBACK_PARTS(client, server_callback, slot, id);
	
	if (client->callbacks[slot].id == id) {
		_evs_server_callback_free(client->callbacks[slot]);
	}
}

void evs_server_client_close(client_t *client) {
	for (callback_t i = 0; i < G_N_ELEMENTS(client->callbacks); i++) {
		if (client->callbacks[i].fn != NULL) {
			_evs_server_callback_free(client->callbacks[i]);
		}
	}
}

gboolean evs_server_init() {
	_events_by_path = g_hash_table_new(g_str_hash, g_str_equal);
	_events_by_handler = g_hash_table_new(NULL, NULL);
	
	evs_server_on("/qio/callback", _evs_server_callback, evs_no_subscribe, NULL, TRUE);
	evs_server_on("/qio/noop", _evs_server_noop, NULL, NULL, FALSE);
	evs_server_on("/qio/ping", _evs_server_ping, NULL, NULL, FALSE);
	evs_server_on("/qio/sub", _evs_server_subscribe, NULL, NULL, FALSE);
	evs_server_on("/qio/unsub", _evs_server_unsubscribe, NULL, NULL, FALSE);
	
	return TRUE;
}

#ifdef TESTING
#include "../test/test_events_server.c"
#endif
