#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "option.h"
#include "pubsub.h"
#include "pubsub_const.h"
#include "validate.h"

/**
 * All of the subscriptions registered on the server.
 *
 * By default, all clients are placed into UNSUBSCRIBED until they subscribe.
 */
static GHashTable* _events;

/** 
 * Shortcut for getting a subscription, or creating it if it doesn't exist.
 *
 * This function IS NOT thread safe.  Only the main thread that is listening
 * on clients should ever call it with and_create == TRUE. It is ALWAYS wrong
 * for the gossip thread to use and_create == TRUE.
 */
static GHashTable* _get_subscriptions(gchar* event_name, gboolean and_create) {
	GHashTable *subs = g_hash_table_lookup(_events, event_name);
	
	if (subs == NULL && and_create && validate_subscription(event_name)) {
		// Use the default hashing functions, only using memory locations anyway
		// (*client->*client is what is being stored)
		subs = g_hash_table_new(NULL, NULL);
		
		// The hash table relies on the key existing for its
		// lifetime: duplicate and insert
		g_hash_table_insert(_events, g_strdup(event_name), subs);
	}
	
	return subs;
}

void sub_client_ready(client_t *client) {
	// Initialize our internal management from the client
	client->subs = g_ptr_array_sized_new(PUBSUB_CLIENT_INTIAL_COUNT);
	
	// Add the user to UNSUBSCRIBED until we get a subscribe request
	sub_client(UNSUBSCRIBED, client);
}

status_t sub_client(gchar *event_name, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions(event_name, TRUE);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	// Since the user will be added to the sub, make sure they're not in the
	// unsubscribed sub
	sub_unsub_client(UNSUBSCRIBED, client);
	
	if (client->sub_count > option_max_subscriptions()) {
		return CLIENT_TOO_MANY_SUBSCRIPTIONS;
	}
	
	DEBUGF("Adding subscription list: %s", event_name);
	
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
	for (int i = 0; i < client->subs->len; i++) {
		GHashTable *event = g_ptr_array_index(client->subs, i);
		g_hash_table_remove(event, client);
	}
	
	g_ptr_array_free(client->subs, TRUE);
}

void sub_unsub_client(gchar *event_name, client_t *client) {
	// Attempt to get the subscription to check if it exists
	GHashTable *subs = _get_subscriptions(event_name, TRUE);
	
	if (subs == NULL) {
		return;
	}
	
	// If the user is subscribed
	if (g_hash_table_remove(subs, client)) {
		client->sub_count--;
	}
}

status_t pub_message(gchar *event_name, message_t *message) {
	GHashTable *subs = _get_subscriptions(event_name, FALSE);
	
	if (subs == NULL) {
		return CLIENT_INVALID_SUBSCRIPTION;
	}
	
	gchar *msgs[h_len];
	int msglen[h_len];
	
	msgs[h_rfc6455] = rfc6455_prepare_frame(message, &msglen[h_rfc6455]);
	
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
			// Let the client loop clean up the dead clients.
			client_kill(client);
			continue;
		}
		
		if (client_write_frame(client, msgs[handler], msglen[handler]) == CLIENT_ABORTED) {
			client_kill(client);
		}
	}
	
	// Clean up all the allocated frames
	for (int i = 0; i < h_len; i++) {
		free(msgs[i]);
	}
	
	return CLIENT_GOOD;
}

gboolean pubsub_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	return TRUE;
}