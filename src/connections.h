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
 * The number of clients to balance before yielding
 */
#define CONNS_YIELD 100

/**
 * Quickly determine how large the intial buffer should be.
 */
#define STRING_BUFFER(client) client->state == cstate_initing ? STRING_HEADER_BUFFER_SIZE : STRING_BUFFER_SIZE

/**
 * The maximum number of queued messages.
 */
#define MAX_BUFFER_SIZE_MULTIPLIER 2

/**
 * The number of loops to run between timeouts.
 * We only want to do this once per second.
 */
#define CONNS_MAINTENANCE_TIMEOUTS ((gint)(1000 / MAINTENANCE_TICK))

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
 * @attention This is ONLY to be used as a callback for quick-event
 *
 * @param client The client to close and free.
 */
void conns_client_close(client_t *client);

/**
 * There is data waiting in a client.
 *
 * @param client The client that has messages waiting.
 *
 * @return If the client should be closed.
 */
gboolean conns_client_data(client_t *client);

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
 * Get a function callback on each client.
 *
 * @note Clients may be closed / removed from the clients table during this operation,
 * and it is safe to do so as long as you use g_ptr_array_remove_index_fast().
 *
 * @attention It's possible you might see the same client twice, and you WILL NOT
 * see clients added after the iteration begins: this function yields during its
 * runtime, and since it runs from the end of the list of clients, any removed client
 * will cause the last client in the list to move to the position of the removed
 * client, so if you're between the two when the client is removed, you'll see the
 * same client again.
 *
 * @internal Use: _conns_clients_remove()
 *
 * @param _callback The function to be called with the current client. Return FALSE any
 * time to cancel the iteration
 */
void conns_clients_foreach(gboolean(*_callback)(client_t*));

/**
 * Release the lock on the clients, yield, and relock after the yield.
 */
void conns_clients_yield();

/**
 * Release the lock on the clients array.
 */
void conns_clients_unlock();

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