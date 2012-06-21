#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "accepter.h"
#include "debug.h"
#include "websocket.h"

// The number of threads we are going to start
#define THREADS 1

// A table of pointers to our running threads
GThread **threads;

void _watch(gpointer data) {
	int sock = *((int*)data);
	
	// Done with the data, clean it up
	free(data);
	
	while (1) {
		int client = accept(sock, (struct sockaddr*)NULL, NULL);
		
		if (client > 0) {
			ws_handle(client);
		} else {
			ERROR("Accept client failed");
		}
	}
}

gboolean accepter_init(void) {
	int sock;
	struct sockaddr_in addy;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("Could not create socket");
		return FALSE;
	}
	
	addy.sin_family = AF_INET;
	addy.sin_port = htons(5000);
	addy.sin_addr.s_addr = inet_addr("0.0.0.0");
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		ERROR("Could not set socket option");
		return FALSE;
	}
	
	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) < 0) {
		ERROR("Could not bind");
		return FALSE;
	}
	
	if (listen(sock, 100) < 0) {
		ERROR("Could not listen");
		return FALSE;
	}
	
	// Spool up the threads to handle the connections
	threads = malloc(THREADS * sizeof(**threads));
	
	for (int i = 0; i < THREADS; i++) {
		// The socketfd to pass to the thread
		// Must be freed by the thread, not the parent
		gpointer data = malloc(sizeof(int));
		*((int*)data) = sock;
		
		*(threads + i) = g_thread_create((GThreadFunc)_watch, data, TRUE, NULL);
	}
	
	return TRUE;
}

void accepter_finish() {
	for (int i = 0; i < THREADS; i++) {
		g_thread_join(*(threads + i));
	}
}