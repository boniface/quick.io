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
#define STRING_BUFFER(client) client->initing == 1 ? STRING_HEADER_BUFFER_SIZE : STRING_BUFFER_SIZE

/**
 * The maximum number of queued messages.
 */
#define MAX_BUFFER_SIZE_MULTIPLIER 2

/**
 * The time between maintenance tasks - in milliseconds
 * @warning This number MAY NEVER be larger than 999.
 */
#define CONNS_MAINTENANCE_WAIT 100

/**
 * The number of loops to run before cleaning up
 */
#define CONNS_MAINTENANCE_CLEANUP 10

/**
 * Setup the necessary control structures on a process-level (post-fork), once
 * the system is ready to process everything in the event loop.
 *
 * @return If the setup went well.
 */
gboolean conns_qsys_ready();

/**
 * Accept a new connection into our midst.
 * It is perfectly acceptable to close the client at this point, or do anything
 * you want with him. Typically, the best thing to do is just set some init 
 * values and a timer, and then wait for him to send a handshake.
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
 * The timer set on a client has expired.
 *
 * @param client The client whose timer expired.
 */
void conns_client_timer(client_t *client);

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