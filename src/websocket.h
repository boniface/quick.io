#pragma once
#include <glib.h>
#include <libsoup/soup.h>

/**
 * The websocket handlers
 */
enum Handlers {
	rfc6455,
};

/**
 * Basic information about a connected client.
 */
typedef struct {
	int sock;
	enum Handlers handler;
} Client;

/**
 * Setup internal structures for handling incoming handshakes
 */
gboolean ws_init();

/**
 * Handle a websocket connection, or drop it if it's not.
 */
void ws_handle(int);

/**
 * Write the specified data to the client.
 */
void ws_client_write(Client*, char*);

/**
 * Closes and cleans up the client.
 */
void ws_client_close(Client*);