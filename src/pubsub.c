#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "debug.h"
#include "pubsub.h"
#include "pubsub_const.h"

/**
 * All of the rooms that you can throw stuff to.
 *
 * By default, all clients go to room UNSUBSCRIBED until they send a subscribe
 * command.
 */
static GHashTable* rooms;

/** 
 * Shortcut for getting a room, or creating it if it doesn't exist
 */
static GPtrArray* _get_room(int room_num) {
	GPtrArray *room = g_hash_table_lookup(rooms, &room_num);
	
	if (room == NULL) {
		room = g_ptr_array_sized_new(INITIAL_ROOM_SIZE);
		g_hash_table_insert(rooms, &room_num, room);
	}
	
	return room;
}

void sub_client_ready(client_t *client) {
	// // Put the user into the UNSUBSCRIBED room until we get a response
	GPtrArray *room = _get_room(UNSUBSCRIBED);
	g_ptr_array_add(room, client);
}

gboolean pubsub_init() {
	rooms = g_hash_table_new(g_int_hash, g_int_equal);
	return TRUE;
}