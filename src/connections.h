/**
 * Handles the raw socket connections.
 * @file connections.h
 */
#ifndef QIO_CONNECTIONS_H
#define QIO_CONNECTIONS_H

#include "qio.h"

/**
 * The size to intialize a message buffer to. Most messages won't exceed this:
 * the initial headers are typically around 150 characters
 */
#define STRING_BUFFER_SIZE 512

/**
 * Headers are typically longer than messages, so initialize at a larger size, so
 * to minimize reallocs.
 */
#define STRING_HEADER_BUFFER_SIZE 256

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
 * The balance request.
 */
struct conns_balance_request {
	/**
	 * The number of clients to move.
	 */
	guint count;

	/**
	 * The external address (with port) that the clients should be moved to.
	 */
	gchar *to;
};

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
void conns_client_new(struct client *client);

/**
 * A notification that a client has been killed.
 *
 * @attention This is ONLY to be used as a callback for quick-event
 *
 * @param client The client that was killed.
 */
void conns_client_killed(struct client *client);

/**
 * Closes and cleans up the client.
 *
 * @attention This is ONLY to be used as a callback for quick-event
 *
 * @param client The client to close clean up.
 */
void conns_client_close(struct client *client);

/**
 * Does the final freeing of all the client stuff once the ref count == 0.
 *
 * @param client The client to free.
 */
void conns_client_free(struct client *client);

/**
 * There is data waiting in a client.
 *
 * @param client The client that has messages waiting.
 *
 * @return If the client should be closed.
 */
gboolean conns_client_data(struct client *client);

/**
 * Clears any timeout that has been set on a client.
 *
 * @param client The client to be cleared.
 */
void conns_client_timeout_clear(struct client *client);

/**
 * Sets a timeout on the client, for closure if the timer elapses.
 * If a timer is already set, this does nothing.
 *
 * @param client The client to be timed.
 */
void conns_client_timeout_set(struct client *client);

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
 * You can safely assume that any client you receive from this function will be
 * valid during this iterator of the event loop: since the client was active
 * when it was sent, it cannot possibly be freed until this thread ticks in QEV,
 * so local references are safe. Persistent references must still use client_ref().
 *
 * When you receive the client, no locks will be held, so you are free to take your
 * time doing whatever you want.
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
void conns_clients_foreach(gboolean(*_callback)(struct client*));

/**
 * Clean up the client message buffers.
 *
 * @param client The client whose buffers need cleaning.
 * @param truncate_buffer If the buffer should be truncated.
 * @param truncate_socket_buffer If the socket buffer should be truncated.
 */
void conns_message_clean(
	struct client *client,
	gboolean truncate_socket_buffer,
	gboolean truncate_buffer);

/**
 * Free the client's message buffers and remove the message.
 *
 * @param client The client whose message buffers should be removed.
 */
void conns_message_free(struct client *client);

#ifdef TESTING
#include "../test/test_connections.h"
#endif

#endif
