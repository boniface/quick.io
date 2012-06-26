#pragma once
#include <glib.h>
#include "socket.h"

#define CLIENT_GOOD 1
#define CLIENT_WAIT 2
#define CLIENT_ABORTED 3

#define CLIENT_PING "ping"
#define CLIENT_PONG "pong"

/**
 * Handle a connection from a client. Once it's handled, update the client
 * to be inited.
 */
short client_handshake(client_t*);

/**
 * Process a message from the client.  If the message isn't complete return false
 * and hope it completes itself.
 */
short client_message(client_t*);