#pragma once
#include <glib.h>
#include "socket.h"

/**
 * Handle a connection from a client. Once it's handled, update the client
 * to be inited.
 */
void client_handshake(client_t*);

/**
 * Get 1 message from the client socket.  If the message is multi-part, will return NULL.
 */
void client_message(client_t*);