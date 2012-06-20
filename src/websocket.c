#include "websocket.h"
#include "rfc6455.h"

// The number of threads to keep in the pool
#define THREADS 2

// Our thread pool
GThreadPool *pool;

/**
 * user_data is always NULL
 */
void _handle(gpointer data, gpointer user_data) {
	gboolean all_good = FALSE;
	
	int sock = *((int*)data);
	free(data);
	
	char buff[8192];
	int len = read(sock, buff, sizeof(buff)-1);
	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	
	//make sure that we're actually getting decent headers back
	//if we don't, just close the connection
	if (len && soup_headers_parse(buff, strlen(buff), req_headers)) {
		handshake_handler handler;
		if (rfc6455_handles(req_headers)) {
			handler = rfc6455_handshake;
		}
		
		if (handler) {
			all_good = handler(sock, req_headers);
		}
	}
	
	if (!all_good) {
		close(sock);
	}
	
	soup_message_headers_free(req_headers);
}

/**
 * Setup the thread pool for handling connections
 */
gboolean ws_init() {
	pool = g_thread_pool_new(_handle, NULL, THREADS, TRUE, NULL);
	return TRUE;
}

/**
 * Pass a new socket to the thread pool for processing
 */
void ws_handle(int sock) {
	// Should be freed by the child handler
	gpointer data = malloc(sizeof(int));
	*((int*)data) = sock;
	
	g_thread_pool_push(pool, data, NULL);
}