#pragma once
#include <glib.h>

#define CLIENT_GOOD 1 << 0
#define CLIENT_WAIT 1 << 1
#define CLIENT_ABORTED 1 << 2
#define CLIENT_NEED_MASK 1 << 3
#define CLIENT_MESSAGE_TOO_LONG 1 << 4
#define CLIENT_UNKNOWN_COMMAND 1 << 5
#define CLIENT_BAD_COMMAND 1 << 6
#define CLIENT_UNSUPPORTED_OPCODE 1 << 7

#define CLIENT_BAD (CLIENT_ABORTED | CLIENT_NEED_MASK | CLIENT_MESSAGE_TOO_LONG | CLIENT_BAD_COMMAND | CLIENT_UNKNOWN_COMMAND | CLIENT_UNSUPPORTED_OPCODE)

/**
 * The client status type.
 */
typedef short status_t;

/**
 * The opcodes that we support.
 */
typedef enum opcode {
	op_pong,
	op_text,
} opcode_t;

/**
 * The currently-processing message.  Kept as part of client becuase the client
 * populates the message, then passes it off to command_handle() for processing.
 */
struct message_s {
	// The remaining length of the incoming message to read
	int remaining_length;
	
	// The opcode from the message
	opcode_t type;
	
	// The mask used for all the data in this message
	guint32 mask;
	
	// The data buffer of what has been read but not processed for the client
	GString *socket_buffer;
	
	// Buffer of data procesed from socket_buffer (defragmented)
	GString *buffer;
} __attribute__((__packed__));

typedef struct message_s message_t;

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
	
	// The handler for this client
	enum handlers handler;
	
	// The current message the client is sending
	message_t *message;
} __attribute__((__packed__));

typedef struct client_s client_t;

/**
 * Handle a connection from a client. Once it's handled, update the client
 * to be inited.
 */
status_t client_handshake(client_t*);

/**
 * Process a message from the client.  If the message isn't complete return false
 * and hope it completes itself.
 */
status_t client_message(client_t*);

/**
 * Write a specific message to a client.
 */
status_t client_write(client_t*, opcode_t, char*);

/**
 * Write 1 single message to 1 single client. THIS SHOULD NOT BE USED FOR BULK WRITES.
 *
 * Takes the message object attached to the client and sends back the buffer, formatted
 * based on the given command and the text.  This also checks that the buffer has
 * something to write back (if a TEXT message), or anything with a PING/PONG message.
 */
status_t client_write_response(client_t*);