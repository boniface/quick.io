#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "debug.h"
#include "option.h"
#include "pubsub.h"
#include "pubsub_const.h"
#include "validate.h"

/**
 * All of the rooms that you can throw stuff to.
 *
 * By default, all clients go to room UNSUBSCRIBED until they send a subscribe
 * command.
 */
static GHashTable* _rooms;

// Global lock for adding and deleting rooms
// "It is not necessary to initialize a mutex that has been created that has been statically allocated."
static GMutex _rooms_lock;

/** 
 * Shortcut for getting a room, or creating it if it doesn't exist.
 *
 * This function IS NOT thread safe.  Only the main thread that is listening
 * on clients should ever call it with and_create == TRUE. It is ALWAYS wrong
 * for the gossip thread to use and_create == TRUE.
 */
static GHashTable* _get_room(gchar* room_name, gboolean and_create) {
	GHashTable *room = g_hash_table_lookup(_rooms, room_name);
	
	if (room == NULL && and_create && validate_room(room_name)) {
		room = g_hash_table_new(g_int_hash, g_int_equal);
		
		// The hash table relies on the key existing for its
		// lifetime: duplicate and insert
		g_hash_table_insert(_rooms, g_strdup(room_name), room);
	}
	
	return room;
}

void sub_client_ready(client_t *client) {
	// Put the user into the UNSUBSCRIBED room until we get a subscribe request
	sub_client(UNSUBSCRIBED, client);
}

status_t sub_client(gchar *room_name, client_t *client) {
	// Attempt to get the room to check if it exists
	GHashTable *room = _get_room(room_name, TRUE);
	
	if (room == NULL) {
		return CLIENT_INVALID_ROOM;
	}
	
	// Since the user will be added to the room, make sure they're not in the
	// unsubscribed room
	sub_unsub_client(UNSUBSCRIBED, client);
	
	if (client->room_count > option_max_rooms()) {
		return CLIENT_TOO_MANY_ROOMS;
	}
	
	DEBUGF("Adding room: %s", room_name);
	
	// Add the client to the room and inc his count
	g_hash_table_insert(room, &client->sock, client);
	client->room_count++;
	
	return CLIENT_GOOD;
}

void sub_client_free(client_t *client) {
	#warning Need to remove client from all subscribed rooms
}

void sub_unsub_client(gchar *room_name, client_t *client) {
	// Attempt to get the room to check if it exists
	GHashTable *room = _get_room(room_name, TRUE);
	
	if (room == NULL) {
		return;
	}
	
	// If the user is in the room
	if (g_hash_table_remove(room, &client->sock)) {
		client->room_count--;
	}
}

status_t pub_message(gchar *room_name, message_t *message) {
	GHashTable *room = _get_room(room_name, FALSE);
	
	if (room == NULL) {
		return CLIENT_INVALID_ROOM;
	}
	
	DEBUGF("Len: %d", h_len);
	for (int i = 0; i < h_len; i++) {
		DEBUGF("MESSAGE: %d", i);
	}
	
	guint clients = g_hash_table_size(room);
	while (clients-- > 0) {
		
	}
	
	return CLIENT_GOOD;
}

gboolean pubsub_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_rooms = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	return TRUE;
}