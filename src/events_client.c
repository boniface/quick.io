#include "qio.h"

/**
 * All of the subscriptions registered on the server.
 *
 * By default, all clients are placed into UNSUBSCRIBED until they subscribe.
 */
static GHashTable* _events;

/**
 * The list of messages that we need to send in our next loop.
 */
static GPtrArray *_messages;

/**
 * For handling messages from multiple threads.
 *
 * From the GLib docs: "It is not necessary to initialize a mutex that has been
 * created that has been statically allocated."
 */
static GMutex _messages_lock;

/** 
 * Shortcut for getting a subscription, or creating it if it doesn't exist.
 *
 * This function IS NOT thread safe.  Only the main thread that is listening
 * on clients should ever call it with and_create == TRUE.
 */
static evs_client_sub_t* _get_subscription(const gchar *event_path, const gboolean and_create) {
	evs_client_sub_t *sub = g_hash_table_lookup(_events, event_path);
	
	if (sub == NULL && and_create) {
		// To validate this event, check the server events to make sure there
		// is a handler registered
		path_extra_t extra;
		guint extra_len;
		event_handler_t *handler = evs_server_get_handler(event_path, &extra, &extra_len);
		
		// If no handler was found, don't create anything
		if (handler == NULL) {
			return NULL;
		}
		
		sub = g_try_malloc0(sizeof(*sub));
		
		// Well, this could be bad
		if (sub == NULL) {
			g_list_free_full(extra, g_free);
			return NULL;
		}
		
		// Keep around a reference to the key for passing around for easier
		// persistence when the event names are freed from a message
		sub->event_path = g_strdup(event_path);
		
		// Use the default hashing functions, only using memory locations anyway
		// (*client->*client is what is being stored)
		sub->clients = g_hash_table_new(NULL, NULL);
		
		// Because we'll need this for some future references, let's keep the
		// handler around: this will also save us on future lookups for the same
		// event
		sub->handler = handler;
		
		// Save the extra elements on the path, we'll need those again later
		sub->extra = extra;
		sub->extra_len = extra_len;
		
		// The hash table relies on the key existing for its
		// lifetime: duplicate and insert
		g_hash_table_insert(_events, sub->event_path, sub);
	}
	
	return sub;
}

/**
 * Create a new messages array in a thread-safe way.
 */
static void _new_messages() {
	g_mutex_lock(&_messages_lock);
	_messages = g_ptr_array_new();
	g_mutex_unlock(&_messages_lock);
}

void evs_client_client_ready(client_t *client) {
	// Initialize our internal management from the client
	client->subs = g_ptr_array_sized_new(MIN(option_max_subscriptions(), EVS_CLIENT_CLIENT_INTIAL_COUNT));
	
	// Add the user to UNSUBSCRIBED until we get a subscribe request
	evs_client_sub_client(UNSUBSCRIBED, client);
}

status_t evs_client_sub_client(const gchar *event_path, client_t *client) {
	// Attempt to get the subscription to check if it exists
	evs_client_sub_t *sub = _get_subscription(event_path, TRUE);
	
	if (sub == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Since the user will be added to the sub, make sure they're not in the
	// unsubscribed sub
	evs_client_unsub_client(UNSUBSCRIBED, client);
	
	if (client->subs->len > option_max_subscriptions()) {
		return CLIENT_TOO_MANY_SUBSCRIPTIONS;
	}
	
	// Don't add the client if he's already subscribed
	if (g_hash_table_contains(sub->clients, client)) {
		return CLIENT_ALREADY_SUBSCRIBED;
	}
	
	DEBUGF("Subscribing client to: %s", event_path);
	
	// Subscribe the client
	g_hash_table_insert(sub->clients, client, client);
	
	// Give the client a reference to the key of the event he is subscribed to
	g_ptr_array_add(client->subs, sub);
	
	// Send the apps callback
	apps_evs_client_subscribe(client, sub);
	
	return CLIENT_GOOD;
}

status_t evs_client_unsub_client(const gchar *event_path, client_t *client) {
	// Attempt to get the subscription to check if it exists
	evs_client_sub_t *sub = _get_subscription(event_path, FALSE);
	
	if (sub == NULL) {
		return CLIENT_CANNOT_UNSUBSCRIBE;
	}
	
	// If the client isn't a member of the subscription, that's an error
	if (!g_hash_table_remove(sub->clients, client)) {
		return CLIENT_CANNOT_UNSUBSCRIBE;
	}
	
	// The client isn't subscribed anymore, remove from his list
	g_ptr_array_remove_fast(client->subs, sub);
	
	// Send the apps callback
	apps_evs_client_unsubscribe(client, sub);
	
	// If the client has removed all subscriptions, add him back to UNSUBSCRIBED
	// unless he was just removed from UNSUBSCRIBED, then just leave empty (assumes
	// the client is being added to another subscription)
	if (client->subs->len == 0 && strcmp(sub->event_path, UNSUBSCRIBED) != 0) {
		evs_client_sub_client(UNSUBSCRIBED, client);
	}
	
	return CLIENT_GOOD;
}

void evs_client_client_free(client_t *client) {
	// It's possible the client never behaved and was killed before
	// its subscriptions were setup
	if (client->subs == NULL) {
		return;
	}
	
	// Remove all the subscriptions from the client, firing
	// off all the remove events
	for (gsize i = 0; i < client->subs->len; i++) {
		evs_client_sub_t *sub = g_ptr_array_index(client->subs, i);
		
		// If we can't remove the client, then he wasn't there to begin with,
		// so just move on
		g_hash_table_remove(sub->clients, client);
		
		// Send the apps callback
		apps_evs_client_unsubscribe(client, sub);
	}
	
	// Clean up the excess memory (and the underlying array used to hold it)
	g_ptr_array_free(client->subs, TRUE);
}

void evs_client_pub_messages() {
	// If there are no messages to publish, then skip this
	if (_messages->len == 0) {
		return;
	}
	
	// Swap the message queue out for a new one so that we can be thread-safe
	// in our message processing
	GPtrArray *messages = _messages;
	_new_messages();
	
	// Go through all the messages and publish!
	for (gsize i = 0; i < messages->len; i++) {
		#warning This might be freed before it can be published
		evs_client_message_t *message = g_ptr_array_index(messages, i);
		
		GPtrArray *dead_clients = g_ptr_array_new();
		gchar *msgs[h_len];
		gsize msglen[h_len];
		
		msgs[h_rfc6455] = rfc6455_prepare_frame(
			message->type,
			message->message,
			message->message_len,
			&msglen[h_rfc6455]
		);
		
		// Go through all the clients and give them their messages
		GHashTableIter iter;
		client_t *client, *client2;
		g_hash_table_iter_init(&iter, message->sub->clients);
		while (g_hash_table_iter_next(&iter, (gpointer)&client, (gpointer)&client2)) {
			enum handlers handler = client->handler;
			
			// This is very bad...
			if (handler == h_none) {
				// We cannot remove the client from this hash table directly or it
				// invalidates the iterator, so we have to delay it.
				g_ptr_array_add(dead_clients, client);
				continue;
			}
			
			if (client_write_frame(client, msgs[handler], msglen[handler]) == CLIENT_ABORTED) {
				g_ptr_array_add(dead_clients, client);
			}
		}
		
		// Clean up all the allocated frames
		for (int i = 0; i < h_len; i++) {
			free(msgs[i]);
		}
		
		// Clean up dead clients
		for (gsize i = 0; i < dead_clients->len; i++) {
			socket_close(g_ptr_array_index(dead_clients, i));
		}
			
		// Clean all that memory
		g_ptr_array_free(dead_clients, TRUE);
		g_free(message->message);
		g_free(message);
	}
	
	// Clean up the messages array we copied
	g_ptr_array_free(messages, TRUE);
}

status_t evs_client_pub_event(const event_handler_t *handler, const path_extra_t extra, const enum data_t type, const gchar *data) {
	// First, start by constructing the name of the event we're publishing to, complete
	// with all extra path elements
	gchar *event_path = evs_server_path_from_handler(handler);
	
	if (event_path == NULL) {
		return CLIENT_UNKNOWN_EVENT;
	}
	
	evs_client_sub_t *sub;
	
	// Skip all the loopy logic unless there are items
	if (extra != NULL) {
		// Add all of the extra path elements to the event path so we can resolve
		// the actual event and clients we're publishing to
		GString *ep = g_string_new(event_path);
		
		path_extra_t curr = g_list_first(extra);
		do {
			g_string_append_printf(ep, "/%s", (gchar*)curr->data);
		} while ((curr = g_list_next(curr)) != NULL);
		
		// DO NOT create a subscription from here, this MUST be thread safe.
		sub = _get_subscription(ep->str, FALSE /* DON'T TOUCH */);
		
		g_string_free(ep, TRUE);
	} else {
		// DO NOT create a subscription from here, this MUST be thread safe.
		sub = _get_subscription(event_path, FALSE /* DON'T TOUCH */);
	}
	
	if (sub == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Attempt to create a new publishable message
	evs_client_message_t *emsg = g_try_malloc0(sizeof(*emsg));
	if (emsg == NULL) {
		return CLIENT_SERVER_OVERLOADED;
	}
	
	// Format the message that will be sent to the clients
	GString *message = g_string_sized_new(100);
	g_string_printf(message, F_EVENT, sub->event_path, 0, DATA_TYPE(type), data);
	
	emsg->sub = sub;
	emsg->type = op_text;
	emsg->message = message->str;
	emsg->message_len = message->len;
	
	// We need to add a message in a thread-safe way
	g_mutex_lock(&_messages_lock);
	g_ptr_array_add(_messages, emsg);
	g_mutex_unlock(&_messages_lock);
	
	g_string_free(message, FALSE);
	
	return CLIENT_GOOD;
}

gboolean evs_client_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	_new_messages();
	return TRUE;
}

void evs_client_cleanup() {
	// Go through the rooms and clean up any that have 0 subscribers
	GHashTableIter iter;
	gchar *key;
	evs_client_sub_t *sub;
	
	g_hash_table_iter_init(&iter, _events);
	while (g_hash_table_iter_next(&iter, (void*)&key, (void*)&sub)) {
		DEBUGF("Hitting subscription: %s - %d", sub->event_path, g_hash_table_size(sub->clients));
		if (g_hash_table_size(sub->clients) == 0) {
			DEBUGF("Removing empty subscription: %s", sub->event_path);
			
			g_free(sub->event_path);
			g_list_free_full(sub->extra, g_free);
			g_hash_table_unref(sub->clients);
			g_hash_table_iter_remove(&iter);
		}
	}
}