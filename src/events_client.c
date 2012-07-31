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
static GHashTable* _get_subscriptions_with_key(gchar **event, gboolean and_create) {
	evs_client_sub_t *sub = g_hash_table_lookup(_events, *event);
	
	if (sub == NULL && and_create && validate_subscription(*event)) {
		// Use the default hashing functions, only using memory locations anyway
		// (*client->*client is what is being stored)
		sub = malloc(sizeof(*sub));
		
		// Keep around a reference to the key for passing around for easier
		// persistence when the event names are freed from a message
		sub->event = g_strdup(*event);
		
		// The hash table we use
		sub->clients = g_hash_table_new(NULL, NULL);
		
		// The hash table relies on the key existing for its
		// lifetime: duplicate and insert
		g_hash_table_insert(_events, sub->event, sub);
	}
	
	// Set the event name to be point to the key
	*event = sub->event;
	
	return sub->clients;
}

static GHashTable* _get_subscriptions(gchar *event, gboolean and_create) {
	gchar *e = event;
	return _get_subscriptions_with_key(&e, and_create);
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

status_t evs_client_sub_client(gchar *event, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions_with_key(&event, TRUE);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Since the user will be added to the sub, make sure they're not in the
	// unsubscribed sub
	evs_client_unsub_client(UNSUBSCRIBED, client);
	
	if (client->subs->len > option_max_subscriptions()) {
		return CLIENT_TOO_MANY_SUBSCRIPTIONS;
	}
	
	// Don't add the client if he's already subscribed
	if (g_hash_table_contains(subs, client)) {
		return CLIENT_ALREADY_SUBSCRIBED;
	}
	
	DEBUGF("Subscribing client to: %s", event);
	
	// Subscribe the client
	g_hash_table_insert(subs, client, client);
	
	// Give the client a reference to the key of the event he is subscribed to
	g_ptr_array_add(client->subs, event);
	
	// Send the apps callback
	apps_evs_client_subscribe(event, client);
	
	return CLIENT_GOOD;
}

status_t evs_client_unsub_client(gchar *event, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions_with_key(&event, FALSE);
	
	if (subs == NULL) {
		return CLIENT_CANNOT_UNSUBSCRIBE;
	}
	
	// If the client isn't a member of the subscription, that's an error
	if (!g_hash_table_remove(subs, client)) {
		return CLIENT_CANNOT_UNSUBSCRIBE;
	}
	
	// The client isn't subscribed anymore, remove from his list
	g_ptr_array_remove_fast(client->subs, event);
	
	// Send the apps callback
	apps_evs_client_unsubscribe(event, client);
	
	// If the client has removed all subscriptions, add him back to UNSUBSCRIBED
	// unless he was just removed from UNSUBSCRIBED, then just leave empty (assumes
	// the client is being added to another subscription)
	if (client->subs->len == 0 && strcmp(event, UNSUBSCRIBED) != 0) {
		evs_client_sub_client(UNSUBSCRIBED, client);
	}
	
	return CLIENT_GOOD;
}

/**
 * THIS FUNCTION IS NOT THREAD SAFE. IT SHOULD NEVER BE CALLED FROM ANYTHING
 * BUT THE MAIN THREAD OF A CHILD PROCESS.
 */
void evs_client_client_free(client_t *client) {
	// It's possible the client never behaved and was killed before
	// its subscriptions were setup
	if (client->subs == NULL) {
		return;
	}
	
	// Remove all the subscriptions from the client, firing
	// off all the remove events
	for (gsize i = 0; i < client->subs->len; i++) {
		// Send the apps callback
		apps_evs_client_unsubscribe(g_ptr_array_index(client->subs, i), client);
	}
	
	// Clean up the excess memory (and the underlying array used to hold it)
	g_ptr_array_free(client->subs, TRUE);
}

void evs_client_pub_messages() {
	// If there are no messages to publish, then skip this
	if (_messages->len == 0) {
		return;
	}
	
	#warning TODO: Attempt to clean pub_messages() up?
	
	// Swap the message queue out for a new one so that we can be thread-safe
	// in our message processing
	GPtrArray *messages = _messages;
	_new_messages();
	
	// Go through all the messages and publish!
	for (gsize i = 0; i < messages->len; i++) {
		evs_client_message_t *message = g_ptr_array_index(messages, i);
		GHashTable *subs = _get_subscriptions(message->event, FALSE);
		
		// It's possible for the subscription to have been removed
		// between publish command and now
		if (subs != NULL) {
			GPtrArray *dead_clients = g_ptr_array_new();
			gchar *msgs[h_len];
			int msglen[h_len];
			
			msgs[h_rfc6455] = rfc6455_prepare_frame(
				message->type,
				message->message,
				message->message_len,
				&msglen[h_rfc6455]
			);
			
			// Go through all the clients and give them their messages
			GHashTableIter iter;
			gpointer key, val;
			g_hash_table_iter_init(&iter, subs);
			while (g_hash_table_iter_next(&iter, &key, &val)) {
				client_t *client = val;
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
			
			for (gsize i = 0; i < dead_clients->len; i++) {
				socket_close(g_ptr_array_index(dead_clients, i));
			}
			
			g_ptr_array_free(dead_clients, TRUE);
		}
		
		// Clean up the message
		g_free(message->message);
		free(message);
	}
	
	// Clean up the messages array we copied
	g_ptr_array_free(messages, TRUE);
}

status_t evs_client_pub_message(gchar *event, message_t *message) {
	// DO NOT create a subscription form here, this MUST be thread safe.
	GHashTable *subs = _get_subscriptions_with_key(&event, FALSE /* DONT TOUCH */);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Attempt to create a new publishable message
	evs_client_message_t *emsg = malloc(sizeof(*emsg));
	if (emsg == NULL) {
		return CLIENT_OVERLOADED;
	}
	
	emsg->event = event;
	emsg->type = message->type;
	emsg->message = g_strdup(message->buffer->str);
	emsg->message_len = message->buffer->len;
	
	// We need to add a message in a thread-safe way
	g_mutex_lock(&_messages_lock);
	g_ptr_array_add(_messages, emsg);
	g_mutex_unlock(&_messages_lock);
	
	return CLIENT_GOOD;
}

gboolean evs_client_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	_new_messages();
	return TRUE;
}

/**
 * This function IS NOT threadsafe.
 */
void evs_client_cleanup() {
	// Go through the rooms and clean up any that have 0 subscribers
	GHashTableIter iter;
	gchar *key;
	evs_client_sub_t *sub;
	
	g_hash_table_iter_init(&iter, _events);
	while (g_hash_table_iter_next(&iter, (void*)&key, (void*)&sub)) {
		DEBUGF("Hitting subscription: %s - %d", sub->event, g_hash_table_size(sub->clients));
		if (g_hash_table_size(sub->clients) == 0) {
			DEBUGF("Removing empty subscription: %s", sub->event);
		
			g_hash_table_unref(sub->clients);
			g_hash_table_iter_remove(&iter);
		}
	}
}