/**
 * Handles communications with the clients, regardless of protocol.
 * Does all of the routing, message framing, demasking, etc for all
 * connected clients.
 * @file client.h
 */

#pragma once
#include "qio.h"

/**
 * The HTTP header terminator
 */
#define HTTP_HEADER_TERMINATOR "\n\n"

/**
 * The WebSocket header terminator
 */
#define WEB_SOCKET_HEADER_TERMINATOR "\r\n\r\n"

/**
 * The path that clients are required to connect to.
 */
#define QIO_PATH "/qio"

/**
 * Everything went as expected. Carry on.
 */
#define CLIENT_GOOD 1 << 0

/**
 * There is data in the response buffer that should be sent to the client callback.
 * This is only useful if there is a response in the buffer to send back to the client,
 * and the client has set a callback number.  Otherwise, you need to fire an event
 * to the client, not write a response to the client.
 */
#define CLIENT_WRITE 1 << 1

/**
 * Something happened that we need to wait for more from the client. Set a wait,
 * and notify us later.
 */
#define CLIENT_WAIT 1 << 2

/**
 * That bastard of a client aborted / should be aborted.
 */
#define CLIENT_ABORTED 1 << 3

/**
 * The client sent a message without a mask. That is ALWAYS an error.
 */
#define CLIENT_NEED_MASK 1 << 4

/**
 * The message the client sent is too long. That an error.
 */
#define CLIENT_MESSAGE_TOO_LONG 1 << 5

/**
 * The client sent us an event we're not handling. Send back an error, but don't
 * kill the client.
 */
#define CLIENT_INVALID_SUBSCRIPTION 1 << 6

/**
 * The client sent a bad message.
 */
#define CLIENT_BAD_MESSAGE_FORMAT 1 << 7

/**
 * The client sent a message with an unsupported opcode.  Kill him.
 */
#define CLIENT_UNSUPPORTED_OPCODE 1 << 8

/**
 * The client already has the maximum number of subscriptions and can't add any more.
 */
#define CLIENT_TOO_MANY_SUBSCRIPTIONS 1 << 9

/**
 * The client is already subscribed to this event
 */
#define CLIENT_ALREADY_SUBSCRIBED 1 << 10

/**
 * For some reason, the client cannot be removed from this event. He will, however,
 * recieve no messages from the event in the future.
 */
#define CLIENT_CANNOT_UNSUBSCRIBE 1 << 11

/**
 * The server is overloaded.
 */
#define CLIENT_SERVER_OVERLOADED 1 << 12

/**
 * The client is not supported by this handler / server.
 */
#define CLIENT_UNSUPPORTED 1 << 13

/**
 * An async operation is being performed. This should be treated like CLIENT_GOOD in
 * most cases, but it MUST NOT send back any information to the client. The function that returned
 * this status becomes responsible for that.
 */
#define CLIENT_ASYNC 1 << 14

/**
 * Any event that means the client is just being stupid.
 */
#define CLIENT_BAD (CLIENT_ABORTED | CLIENT_NEED_MASK | CLIENT_MESSAGE_TOO_LONG | CLIENT_UNSUPPORTED_OPCODE | CLIENT_BAD_MESSAGE_FORMAT | CLIENT_UNSUPPORTED)

/**
 * The opcodes that we support.
 */
typedef enum opcode {
	/**
	 * A pong should be sent to the client.
	 */
	op_pong,
	
	/**
	 * Text is being sent to the client.
	 */
	op_text,
} opcode_t;

/**
 * The currently-processing message.  Kept as part of client becuase the client
 * populates the message, then passes it off to command_handle() for processing.
 */
typedef struct message_s {
	/**
	 * The remaining length of the incoming message to read
	 */
	guint16 remaining_length;
	
	/**
	 * The opcode from the message.
	 */
	opcode_t type;
	
	/**
	 * The mask used for all the data in this message; all messages are 32 bits.
	 * We're being tricky with this mask and converting it from a string to a 32bit
	 * integer for easier use.
	 */
	guint32 mask;
	
	/**
	 * The data buffer of what has been read but not processed for the client.
	 * Once data has been processed, it MUST be removed from the socket_buffer.
	 */
	GString *socket_buffer;
	
	/**
	 * Buffer of data procesed from socket_buffer (demasked).
	 */
	GString *buffer;
} STRUCT_PACKED message_t;

/**
 * The websocket handlers.
 */
enum handlers {
	/**
	 * @see handler_6455.h
	 */
	h_rfc6455,
	
	/**
	 * The number of handlers that exist.
	 */
	h_len,
	
	/**
	 * Not included in the length because we don't use it for arrays. It's always
	 * an error if this is the handler when a client sends a message.
	 */
	h_none,
};

/**
 * The current state of the client.
 */
enum client_state {
	/**
	 * The client just connected, no handshake or anything.
	 */
	cstate_initing,
	
	/**
	 * The client is connected and operating like any other client.
	 */
	cstate_running,
	
	/**
	 * The client is disconnected but not yet freed.
	 */
	#warning Actually use this!
	cstate_dead,
};

/**
 * Basic information about a connected client.
 */
struct client_s {
	/**
	 * The underlying socket for this client.
	 */
	qsys_socket socket;
	
	/**
	 * A timer for killing stupid clients.
	 * This is the number of ticks a client may misbehave before he is killed.
	 * A value of -1 indicates no timer is set.
	 */
	gint16 timer;
	
	/**
	 * If the client is still in the initing (handshake) process.
	 */
	enum client_state state;
	
	/**
	 * The handler for this client.
	 */
	enum handlers handler;
	
	/**
	 * Which events the client is currently subscribed to.
	 * @attention This is an array of strings (pointers to the keys in the events
	 * hash table); as such, values MAY NEVER be free'd.
	 * @warning NOT thread safe.
	 */
	GPtrArray *subs;
	
	/**
	 * The current message(s) that the client is sending / are being processed.
	 */
	message_t *message;
	
	/**
	 * The number of references the client has.
	 */
	volatile gsize ref_count;
} STRUCT_PACKED;

/**
 * Handle a connection from a client. Once it's handled, update the client
 * to be inited.
 *
 * @param client The client being handled.
 */
status_t client_handshake(client_t *client);

/**
 * Process a message from the client.  If the message isn't complete return STATUS_WAIT
 * and hope it completes itself.
 *
 * @param client The client being handled.
 */
status_t client_message(client_t *client);

/**
 * Write a specific message to a client.
 *
 * @param client The client being handled.
 * @param message The messages to send to the client. If this paramter is null, it will attempt to extract the message from the client.
 */
status_t client_write(client_t *client, message_t *message);

/**
 * Write the given message to the underlying client socket.
 *
 * This is a separate function for bulk writing: it merely demands a pre-formatted
 * frame and frame length, and then it writes to the client, giving a status in return.
 *
 * @param client The client to send the message to.
 * @param frame The string that should be sent.
 * @param frame_len The length of the string.
 */
status_t client_write_frame(client_t *client, char *frame, gsize frame_len);

/**
 * Increment the reference count on the client.
 */
APP_EXPORT void client_ref(client_t *client);

/**
 * Decrement the reference count on the client.
 *
 * @attention Once you call this function, you MUST consider your pointer to the client to be
 * invalid, and you MAY NEVER use it again.
 */
APP_EXPORT void client_unref(client_t *client);

#ifdef TESTING
#include "../test/test_client.h"
#endif
