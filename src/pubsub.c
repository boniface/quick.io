#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "option.h"
#include "pubsub.h"
#include "validate.h"

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
 * on clients should ever call it with and_create == TRUE. It is ALWAYS wrong
 * for the gossip thread to use and_create == TRUE.
 */
static GHashTable* _get_subscriptions(gchar* event, gboolean and_create) {
	GHashTable *subs = g_hash_table_lookup(_events, event);
	
	if (subs == NULL && and_create && validate_subscription(event)) {
		// Use the default hashing functions, only using memory locations anyway
		// (*client->*client is what is being stored)
		subs = g_hash_table_new(NULL, NULL);
		
		// The hash table relies on the key existing for its
		// lifetime: duplicate and insert
		g_hash_table_insert(_events, g_strdup(event), subs);
	}
	
	return subs;
}

/**
 * Create a new messages array in a thread-safe way.
 */
static void _new_messages() {
	g_mutex_lock(&_messages_lock);
	_messages = g_ptr_array_new();
	g_mutex_unlock(&_messages_lock);
}

void sub_client_ready(client_t *client) {
	// Initialize our internal management from the client
	client->subs = g_ptr_array_sized_new(MIN(option_max_subscriptions(), PUBSUB_CLIENT_INTIAL_COUNT));
	
	// Add the user to UNSUBSCRIBED until we get a subscribe request
	sub_client(UNSUBSCRIBED, client);
}

status_t sub_client(gchar *event, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions(event, TRUE);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Since the user will be added to the sub, make sure they're not in the
	// unsubscribed sub
	sub_unsub_client(UNSUBSCRIBED, client);
	
	if (client->sub_count > option_max_subscriptions()) {
		return CLIENT_TOO_MANY_SUBSCRIPTIONS;
	}
	
	DEBUGF("Adding subscription list: %s", event);
	
	// Subscribe the client inc his count
	g_hash_table_insert(subs, client, client);
	client->sub_count++;
	
	return CLIENT_GOOD;
}

/**
 * THIS FUNCTION IS NOT THREAD SAFE. IT SHOULD NEVER BE CALLED FROM ANYTHING
 * BUT THE MAIN THREAD OF A CHILD PROCESS.
 */
void sub_client_free(client_t *client) {
	// It's possible the client never behaved and was killed before
	// its subscriptions were setup
	if (client->subs == NULL) {
		return;
	}
	
	for (gsize i = 0; i < client->subs->len; i++) {
		GHashTable *event = g_ptr_array_index(client->subs, i);
		g_hash_table_remove(event, client);
	}
	
	g_ptr_array_free(client->subs, TRUE);
}

void sub_unsub_client(gchar *event, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions(event, TRUE);
	
	if (subs == NULL) {
		return;
	}
	
	// If the user is subscribed
	if (g_hash_table_remove(subs, client)) {
		client->sub_count--;
	}
}

void pub_messages() {
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
		pub_message_t *message = g_ptr_array_index(messages, i);
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
		g_free(message->event);
		g_free(message->message);
		free(message);
	}
	
	// Clean up the messages array we copied
	g_ptr_array_free(messages, TRUE);
}

status_t pub_message(gchar *event, message_t *message) {
	GHashTable *subs = _get_subscriptions(event, FALSE);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Attempt to create a new publishable message
	pub_message_t *emsg = malloc(sizeof(*emsg));
	if (emsg == NULL) {
		return CLIENT_OVERLOADED;
	}
	
	emsg->event = g_strdup(event);
	emsg->type = message->type;
	emsg->message = g_strdup(message->buffer->str);
	emsg->message_len = message->buffer->len;
	
	// We need to add a message in a thread-safe way
	g_mutex_lock(&_messages_lock);
	
	g_ptr_array_add(_messages, emsg);
	
	g_mutex_unlock(&_messages_lock);
	
	return CLIENT_GOOD;
}

gboolean pubsub_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	_new_messages();
	return TRUE;
}
