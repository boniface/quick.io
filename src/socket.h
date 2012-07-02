#pragma once
#include <glib.h>

#include "client.h"

#define LISTEN_BACKLOG 1000

// Most messages won't exceed this: the initial headers are typically
// around 150 characters
#define STRING_BUFFER_SIZE 100

// Headers are typically longer than messages, so minimized reallocs
#define STRING_HEADER_BUFFER_SIZE 200

#define STRING_BUFFER(client) client->initing == 1 ? STRING_HEADER_BUFFER_SIZE : STRING_BUFFER_SIZE

// Externally, clients MUST not send more than 1000 characters per message.
#define MAX_BUFFER_SIZE 1008*2 // Allow for the masking key and headers, two queued messages
#define MAX_MESSAGE_SIZE 1000

// Events epoll should wait for
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET
#define EPOLL_MAX_EVENTS 100
#define EPOLL_WAIT -1

/**
 * Open up the listening socket in preparation for forking.
 */
gboolean socket_init(void);

/**
 * Start running.
 */
void socket_loop(void);

/**
 * Setup the necessary control structures.
 */
gboolean socket_init_epoll(void);

/**
 * Closes and cleans up the client.
 */
void socket_close(client_t*);

/**
 * Free the client message, but not necessarily the underlying socket buffer.
 *
 * If purge_socket_buffer is TRUE, then the socket buffer will be forcibly
 * dumped even if it is not empty.  Otherwise, the socket buffer is left 
 * intact for futher reading / appending.
 */
void socket_message_free(client_t*, gboolean);

/**
 * Sets a timer on a socket; when the timer expires, the corresponding client will
 * be killed.
 */
void socket_set_timer(client_t*);

/**
 * The client is behaving again.  Removing his timer.
 */
void socket_clear_timer(client_t*);