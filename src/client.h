#pragma once
#include <glib.h>

#define CLIENT_GOOD 1 << 0
#define CLIENT_WAIT 1 << 1
#define CLIENT_ABORTED 1 << 2
#define CLIENT_NEED_MASK 1 << 3
#define CLIENT_MESSAGE_TOO_LONG 1 << 4
#define CLIENT_UNKNOWN_COMMAND 1 << 5
#define CLIENT_UNSUPPORTED_OPCODE 1 << 6

#define CLIENT_BAD (CLIENT_ABORTED | CLIENT_NEED_MASK | CLIENT_MESSAGE_TOO_LONG | CLIENT_UNKNOWN_COMMAND | CLIENT_UNSUPPORTED_OPCODE)

/**
 * The currently-processing command.
 */
struct command_s {
	// The remaining length of the incoming message to read
	int remaining_length;
	
	guint32 mask;
	
	// The data buffer of what has been read but not processed for the client
	GString *socket_buffer;
	
	// Buffer of data procesed from socket_buffer (defragmented)
	GString *buffer;
} __attribute__((__packed__));

typedef struct command_s command_t;

/**
 * The websocket handlers
 */
enum handlers {
	none,
	rfc6455,
};

/**
 * Basic information about a connected client.
 */
struct client_s {
	// The underlying socket for this client
	int sock;
	
	// A timer for killing stupid clients
	int timer;
	
	// If the client is still in the initing (handshake) process
	char initing;
	
	// The thread this client runs in
	short thread;
	
	// The handler for this client
	enum handlers handler;
	
	// The current command the client is sending
	command_t *command;
} __attribute__((__packed__));

typedef struct client_s client_t;

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