#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"

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