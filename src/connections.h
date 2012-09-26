/**
 * Handles the raw socket connections.
 * @file connections.h
 */

#pragma once
#include "qio.h"

/**
 * The size to intialize a message buffer to. Most messages won't exceed this:
 * the initial headers are typically around 150 characters
 */
#define STRING_BUFFER_SIZE 100

/**
 * Headers are typically longer than messages, so initialize at a larger size, so
 * to minimize reallocs.
 */
#define STRING_HEADER_BUFFER_SIZE 200

/**
 * Quickly determine how large the intial buffer should be.
 */
#define STRING_BUFFER(client) client->state == cstate_initing ? STRING_HEADER_BUFFER_SIZE : STRING_BUFFER_SIZE

/**
 * The maximum number of queued messages.
 */
#define MAX_BUFFER_SIZE_MULTIPLIER 2

/**
 * The number of loops to run before cleaning up.
 * We only want to do this once per second, or so.
 */
#define CONNS_MAINTENANCE_CLEANUP ((gint)(1000 / MAINTENANCE_TICK))

/**
 * The balance request.
 */
typedef struct conns_balance_request_s {
	/**
	 * The number of clients to move.
	 */
	guint count;
	
	/**
	 * The external address (with port) that the clients should be moved to.
	 */
	gchar *to;
} conns_balance_request_t;

/**
 * Put a new balance request into the queue.
 *
 * @ingroup AppFunctions
 *
 * @param count The number of clients to move.
 * @param to The external address (with port) the clients should be sent to.
 */
APP_EXPORT void conns_balance(guint count, gchar *to);

/**
 * Accept a new connection into our midst.
 * It is perfectly acceptable to close the client at this point, or do anything
 * you want with him. Typically, the best thing to do is just set some init 
 * values and a timer, and then wait for him to send a handshake.
 *
 * This client is expected to have been allocated with g_slice_alloc*
 *
 * @param client The client to accept.
 */
void conns_client_new(client_t *client);

/**
 * Closes and cleans up the client.
 *
 * @param client The client to close and free.
 */
void conns_client_close(client_t *client);

/**
 * A notification that a client HUP'd (hung up).
 *
 * @param client The client that abandonded us.
 */
void conns_client_hup(client_t *client);

/**
 * There is data waiting in a client.
 *
 * @param client The client that has messages waiting.
 */
void conns_client_data(client_t *client);

/**
 * Clears any timeout that has been set on a client.
 *
 * @param client The client to be cleared.
 */
void conns_client_timeout_clear(client_t *client);

/**
 * Sets a timeout on the client, for closure if the timer elapses.
 * If a timer is already set, this does nothing.
 *
 * @param client The client to be timed.
 */
void conns_client_timeout_set(client_t *client);

/**
 * Setup any internal structures needed.
 */
gboolean conns_init();

/**
 * Tick to run maintenance operations.
 * This does all sorts of internal cleanup, message flushing, and dead client clearing.
 */
void conns_maintenance_tick();

/**
 * Clean up the client message buffers.
 *
 * @param client The client whose buffers need cleaning.
 * @param truncate_buffer If the buffer should be truncated.
 * @param truncate_socket_buffer If the socket buffer should be truncated.
 */
void conns_message_clean(client_t *client, gboolean truncate_socket_buffer, gboolean truncate_buffer);

/**
 * Free the client's message buffers and remove the message.
 *
 * @param client The client whose message buffers should be removed.
 */
void conns_message_free(client_t *client);

#ifdef TESTING
#include "../test/test_connections.h"
#endif