#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "debug.h"
#include "pubsub.h"
#include "pubsub_const.h"
#include "websocket.h"

/**
 * All of the rooms that you can throw stuff to.
 *
 * By default, all clients go to room UNSUBSCRIBED until they send a subscribe
 * command.
 */
static GHashTable* rooms;

// The threads currently listening on the epoll for client data
static GThread **listen_threads;

// The epoll instance for listening to client writes
static int _listen_epoll;

static GPtrArray* get_room(int room_num) {
	GPtrArray *room = g_hash_table_lookup(rooms, &room_num);
	
	if (room == NULL) {
		room = g_ptr_array_sized_new(INITIAL_ROOM_SIZE);
		g_hash_table_insert(rooms, &room_num, room);
	}
	
	return room;
}

/**
 * Reads messages from the clients and responds.
 */
static void _listen_for_data(gpointer data, gpointer user_data) {
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	while (1) {
		int num_evs = epoll_wait(_listen_epoll, events, EPOLL_MAX_EVENTS, 100);
		
		if (num_evs < 1) {
			continue;
		}
		
		for (int i = 0; i < num_evs; i++) {
			DEBUG("message!");
			Client *client = (Client*)events[i].data.ptr;
		}
	}
}

void sub_client_ready(Client *client) {
	// The data necessary for our callback
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = client;
	
	// Add a watch on the client connection
	if (epoll_ctl(_listen_epoll, EPOLL_CTL_ADD, client->sock, &ev) == -1) {
		ERROR("Could not EPOLL_CTL_ADD on client");
		ERROR(strerror(errno));
		ws_client_close(client);
		return;
	}
	
	// Ask the client what he wants as we store him for later
	ws_client_write(client, QUERY_READY);
	
	// Put the user into the UNSUBSCRIBED room until we get a response
	GPtrArray *room = get_room(UNSUBSCRIBED);
	g_ptr_array_add(room, client);
}

gboolean pubsub_init() {
	rooms = g_hash_table_new(g_int_hash, g_int_equal);
	
	// 1 -> a size must be given; ignored by new kernels
	_listen_epoll = epoll_create(1);
	
	if (_listen_epoll < 1) {
		ERROR("Could not init epoll");
		return FALSE;
	}
	
	listen_threads = malloc(LISTEN_THREADS * sizeof(**listen_threads));
	for (int i = 0; i < LISTEN_THREADS; i++) {
		*(listen_threads + i) = g_thread_create((GThreadFunc)_listen_for_data, NULL, TRUE, NULL);
	}
	
	return TRUE;
}