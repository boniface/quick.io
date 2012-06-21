#pragma once
#include <glib.h>
#include <libsoup/soup.h>

/**
 * Function pointer for anything that handles a WebSocket handshake.
 */
typedef gboolean (*handshake_handler)(const int, SoupMessageHeaders*);

/**
 * Setup internal structures for handling incoming handshakes
 */
gboolean ws_init();

/**
 * Handle a websocket connection, or drop it if it's not.
 */
void ws_handle(int);