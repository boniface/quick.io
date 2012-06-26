#pragma once
#include <glib.h>

#define LISTEN_BACKLOG 1000

// Most messages won't exceed this: the initial headers are typically
// around 150 characters
#define STRING_BUFFER_SIZE 200

// Since messages coming from the client are masked, there is some additional
// overhead in the protocol that must be allowed for
// Externally, clients MUST not send more than 1000 characters per message.
#define MAX_SOCKET_BUFFER_SIZE 1200

// Events epoll should wait for
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET
#define EPOLL_MAX_EVENTS 100

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
	
	// The data buffer of what has been read but not processed for the client
	GString* socket_buffer;
	
	// Buffer of data procesed from socket_buffer (defragmented)
	GString* buffer;
} __attribute__((__packed__));

typedef struct client_s client_t;

/**
 * Setup internal structures for handling all the connections
 */
gboolean socket_init(void);

/**
 * Close down all the threads.
 */
void socket_finish(void);

/**
 * Closes and cleans up the client.
 */
void socket_close(client_t*);

/**
 * Clear the buffer on the client.  We're done reading it.
 */
void socket_clear_buffer(client_t*);

/**
 * Sets a timer on a socket; when the timer expires, the corresponding client will
 * be killed.
 */
void socket_set_timer(client_t*);

/**
 * The client is behaving again.  Removing his timer.
 */
void socket_clear_timer(client_t*);