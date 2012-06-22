#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "handler_rfc6455.h"
#include "pubsub.h"
#include "websocket.h"

// The number of threads to keep in the pool
#define HANDSHAKE_THREADS 2
#define WRITE_THREADS 2

// Our thread pool for shaking hands
static GThreadPool *_pool_handshake;

// Our thread pool for writing to the clients
static GThreadPool *_pool_write;

/**
 * Send a message to the client that we're ready to go.
 */
static void _ws_client_ready(int sock, enum Handlers handler) {
	Client* client = malloc(sizeof(Client));
	
	if (client == NULL) {
		ERROR("Client could not be allocated");
		close(sock);
		return;
	}
	
	client->sock = sock;
	client->handler = handler;
	sub_client_ready(client);
}

/**
 * user_data is always NULL
 */
static void _ws_client_handshake(gpointer data, gpointer user_data) {
	gboolean all_good = FALSE;
	enum Handlers handler;
	
	int sock = *((int*)data);
	free(data);
	
	// TODO: Better handling of reading first headers from the socket
	char buff[8192];
	int len = read(sock, buff, sizeof(buff)-1); //remove the null, message headers doesn't like it
	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	
	//make sure that we're actually getting decent headers back
	//if we don't, just close the connection
	if (len && soup_headers_parse(buff, strlen(buff), req_headers)) {
		if (rfc6455_handles(req_headers)) {
			handler = rfc6455;
			all_good = rfc6455_handshake(sock, req_headers);
		}
	}
	
	if (all_good) {
		_ws_client_ready(sock, handler);
	} else {
		DEBUG("Socket handler not found");
		close(sock);
	}
	
	soup_message_headers_free(req_headers);
}

/**
 * Write the given message to the subscribed clients
 *
 * data - 
 */
static void _ws_broadcast(gpointer data, gpointer user_data) {
	
}

/**
 * Write the message out to the socket.
 */
void ws_client_write(Client *client, char* msg) {
	int frame_len;
	char *frame = rfc6455_prepare_frame(msg, &frame_len);
	
	if (frame_len > -1) {
		write(client->sock, frame, frame_len);
		free(frame);
	} else {
		ERROR("Something went wrong with frame creation...");
	}
}

void ws_client_close(Client *client) {
	close(client->sock);
	free(client);
}

/**
 * Setup the thread pools
 */
gboolean ws_init() {
	_pool_handshake = g_thread_pool_new(_ws_client_handshake, NULL, HANDSHAKE_THREADS, TRUE, NULL);
	_pool_write = g_thread_pool_new(_ws_broadcast, NULL, WRITE_THREADS, TRUE, NULL);
	
	return TRUE;
}

/**
 * Pass a new socket to the thread pool for processing
 */
void ws_handle(int sock) {
	// Should be freed by the child handler
	gpointer data = malloc(sizeof(int));
	
	if (data == NULL) {
		ERROR("New client could not be handled, malloc() returned null");
		return;
	}
	
	*((int*)data) = sock;
	g_thread_pool_push(_pool_handshake, data, NULL);
}