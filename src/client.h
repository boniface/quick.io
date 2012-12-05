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
 * The data sent to a callback when there is an error.
 * This has data_type: d_plain
 */
#define QIO_ERROR "qio_error"

/**
 * Everything went as expected. Send any callbacks, include any data in buffers, and move on.
 */
#define CLIENT_GOOD 1 << 0

/**
 * That bastard of a client aborted / should be aborted, or did something incredibly stupid, so
 * he should just be killed.
 */
#define CLIENT_FATAL 1 << 1

/**
 * A non-fatal error occurred while processing. This response trips the event handler
 * to respond with an error event to callbacks.
 */
#define CLIENT_ERROR 1 << 2

/**
 * An async operation is being performed. This should be treated like CLIENT_GOOD in
 * most cases, but it MUST NOT send back any information to the client. The function that returned
 * this status becomes responsible for that.
 */
#define CLIENT_ASYNC 1 << 3

/**
 * There is data in the response buffer that should be written back to the client.
 */
#define CLIENT_WRITE 1 << 4

/**
 * Something happened that we need to wait for more from the client. Set a wait,
 * and notify us later.
 */
#define CLIENT_WAIT 1 << 5

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
} message_t;

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
	 * This is useful mainly for unit testing.
	 */
	cstate_dead,
};

/**
 * Basic information about a connected client.
 */
struct client_s {
	/**
	 * The quick-event data
	 */
	struct qev_client qevclient;
	
	/**
	 * A timer for killing stupid clients.
	 * This is the number of ticks a client may misbehave before he is killed.
	 * A value of -1 indicates no timer is set.
	 */
	gint8 timer;
	
	/**
	 * The last time the client was sent data.
	 */
	gint64 heartbeat;
	
	/**
	 * The state of the client.
	 */
	enum client_state state;
	
	/**
	 * The handler for this client.
	 */
	enum handlers handler;
	
	/**
	 * Which events the client is currently subscribed to.
	 */
	GPtrArray *subs;
	
	/**
	 * The current message(s) that the client is sending / are being processed.
	 */
	message_t *message;
	
	/**
	 * Extra data that is stored on the client.  This exists until ref_count == 0.
	 * This table is lazy-allocated, so it MIGHT be NULL.
	 */
	GHashTable *external_data;
	
	/**
	 * The position in the clients table (this is index + 1 such that 0 indicates it's
	 * not in the table).
	 *
	 * This is protected by _clients_lock in connections.c
	 */
	guint clients_pos;
	
	/**
	 * The callbacks registered on the client.
	 */
	struct client_cb_s callbacks[MAX_CALLBACKS];
	
	/**
	 * The number of references the client has.
	 */
	gsize ref_count;
};

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
 * @param message The messages to send to the client. If this paramter is null, it will
 * attempt to extract the message from the client.
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

/**
 * Sets a piece of information on a client. All strings will be duplicated before being
 * stored. To remove a key, simply call `client_set(client, "some_key", NULL)`. All values
 * will be available as long as the client exists (ie. as long as a reference to the client
 * exists).
 *
 * @note This information is visible to all apps.
 * 
 * @param client The client to store the information on.
 * @param key The key to set.
 * @param value The value to set.
 */
APP_EXPORT void client_set(client_t *client, const gchar *key, const gchar *value);

/**
 * Gets a piece of information on a client.
 *
 * @param client The client to get the information from.
 * @param key The key to get.
 *
 * @return A copy of the value, or NULL if not found. If the value is returned, it MUST be
 * g_free()'d.
 */
APP_EXPORT gchar* client_get(client_t *client, const gchar *key);

/**
 * Determines if a piece of information exists on a client.
 *
 * @param client The client to check the information on.
 * @param key The key to check.
 *
 * @return TRUE if the information if there, FALSE otherwise;
 */
APP_EXPORT gboolean client_has(client_t *client, const gchar *key);

#ifdef TESTING
#include "../test/test_client.h"
#endif
