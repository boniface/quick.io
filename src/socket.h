/**
 * Handles the raw socket connections.
 * @file socket.h
 */

#pragma once
#include "qio.h"

/**
 * The number of connections the operating system can queue.
 */
#define LISTEN_BACKLOG 1000

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
 * Events epoll should wait for.
 */
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET

/**
 * The maximum number of events to return per epoll round.
 */
#define EPOLL_MAX_EVENTS 100

/**
 * How long the epoll should wait for events.
 * -1 just to wait forever until something happens.
 */
#define EPOLL_WAIT -1

/**
 * The time between maintenance tasks - in nanoseconds
 */
#define SOCKET_MAINTENANCE_WAIT 100 * 1000000

/**
 * The number of loops to run before cleaning up
 */
#define SOCKET_MAINTENANCE_CLEANUP 10

/**
 * Open up the listening socket in preparation for forking.
 *
 * @return If the setup went well.
 */
gboolean socket_init();

/**
 * Start running the main loop.  Accept clients, do cleanup, and be happy!
 */
void socket_loop();

/**
 * Setup the necessary control structures on a process-level (post-fork).
 *
 * @return If the setup went well.
 */
gboolean socket_init_process();

/**
 * Closes and cleans up the client.
 *
 * @param client The client to close and free.
 */
void socket_close(client_t *client);

/**
 * Free the client message, but not necessarily the underlying socket buffer.
 *
 * The socket buffer will not be freed unless it is empty or it is forced with
 * purge_socket_buffer.
 * 
 * @param client The client to free up and close.
 * @param purge_socket
 * If TRUE, then the socket buffer will be forcibly dumped even if it is not empty.
 * Otherwise, the socket buffer is left intact for futher reading / appending.
 */
void socket_message_free(client_t *client, gboolean purge_socket);

/**
 * Sets a timer on a socket; when the timer expires, the corresponding client will
 * be killed.
 *
 * If the timer parameters are both 0, then the defaults from the config file are used.
 *
 * @param client The client to set the timer on.
 * @param timeout_sec The number of seconds to wait.
 * @param timeout_nano The number of nanoseconds to wait.
 */
gboolean socket_set_timer(client_t *client, int timeout_sec, long timeout_nano);

/**
 * The client is behaving again.  Removing his timer.
 *
 * @param client The now-behaving client.
 */
void socket_clear_timer(client_t *client);