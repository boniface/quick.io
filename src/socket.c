#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "client.h"
#include "debug.h"
#include "option.h"
#include "socket.h"

// The number of threads to keep in the pool
#define THREADS 4

// Our threads for managing the clients
static GThread **threads;

// The socket we accept connections on
static int _listen_sock;

// All our epoll instances across the threads
static int _epoll[THREADS];

/**
 * Adds an epoll listener on the specified fd.
 */
static void _socket_epoll_add(int fd, client_t *client) {
	// Events that we listen for on the new client socket
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = client;
	
	// Start listening on the socket for anything the client has to offer
	if (epoll_ctl(_epoll[client->thread], EPOLL_CTL_ADD, fd, &ev) == -1) {
		ERROR("Could not listen on client socket");
		socket_close(client);
		return;
	}
}

/**
 * Accepts a new client into our midst
 */
static void _socket_accept_client(short thread) {
	int sock = accept(_listen_sock, (struct sockaddr*)NULL, NULL);
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ERROR("Could not set client non-blocking");
		close(sock);
		return;
	}
	
	client_t *client = malloc(sizeof(*client));
	if (client == NULL) {
		ERROR("Client could not be malloc()'d");
		close(sock);
		return;
	}
	
	// Make sure our memory is nice and spiffy-clean
	memset(client, 0, sizeof(*client));
	
	// Basic information about this client
	client->sock = sock;
	client->initing = 1;
	client->thread = thread;
	
	// Make the client finish the handshake quickly, or drop him
	socket_set_timer(client);
	
	// Add the client to this thread's epoll
	_socket_epoll_add(sock, client);
}

/**
 * Create a new command struct in the client
 */
static void _socket_command_new(client_t *client) {
	// Don't overwrite any command that currently exists
	if (client->command != NULL) {
		return;
	}

	command_t *command = malloc(sizeof(*command));
	memset(command, 0, sizeof(*command));
	
	// Initing clients get larger buffers because they'll typically send more
	if (client->initing) {
		command->socket_buffer = g_string_sized_new(STRING_HEADER_BUFFER_SIZE);
		command->buffer = g_string_sized_new(STRING_HEADER_BUFFER_SIZE);
	} else {
		command->socket_buffer = g_string_sized_new(STRING_BUFFER_SIZE);
		command->buffer = g_string_sized_new(STRING_BUFFER_SIZE);
	}
	
	client->command = command;
}

static void _socket_handle_client(client_t *client, uint32_t evs) {
	gchar buffer[1000];
	
	if (evs & EPOLLRDHUP) {
		DEBUG("A client closed");
		socket_close(client);
	} else if (evs & EPOLLIN) {
		// Check the timer to see if it has expired
		// A read operation on a timerfd will return > -1 if the 
		// timer has expired.
		if (client->timer && read(client->timer, buffer, 8) > -1) {
			// If the client has been caught misbehaving...
			DEBUG("Misbehaving client closed because of timeout");
			socket_close(client);
			return;
		}
	
		// Clients typically aren't sending commands
		_socket_command_new(client);
		
		// Read the message the client sent, unless it's too large,
		// then kill the client
		int len;
		while ((len = read(client->sock, buffer, sizeof(buffer))) > 0) {
			// Put the buffer into our string
			g_string_append_len(client->command->socket_buffer, buffer, len);
			
			// If the client needs to ehance his calm, kill the connection.
			if (client->command->socket_buffer->len > MAX_BUFFER_SIZE) {
				DEBUG("Client needs to ehance his calm");
				socket_close(client);
				return;
			}
		}
		
		short status;
		if (client->initing) {
			DEBUG("Client handshake");
			status = client_handshake(client);
		} else {
			DEBUG("Message from client");
			status = client_message(client);
		}
		
		// If the client becomes good, then clear the timer and let him live
		if (status == CLIENT_GOOD) {
			socket_clear_timer(client);
			socket_command_free(client);
		
		// The client is misbehaving. Close him.
		} else if (status & CLIENT_BAD) {
			socket_close(client);
		
		// The client gets 1 timer to make itself behave. If it doesn't in this
		// time, then we summarily kill it.
		} else if (status == CLIENT_WAIT && !client->timer) {
			socket_set_timer(client);
		}
	}
}

/**
 * Reads data from the client sockets until it can construct a full message.
 * Once a message is constructed, it passes it off to pubsub for routing.
 *
 * `data` is always null.
 */
static void _socket_loop(gpointer data) {
	// Our current thread number
	short thread = *((short*)data);
	free(data);
	
	// 1 -> a positive, int size must be given; ignored by new kernels
	int poll = _epoll[thread] = epoll_create(1);
	if (poll < 1) {
		ERROR("Could not init epoll");
		return;
	}
	
	// Listen on our accept socket for incoming connections
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.fd = _listen_sock;
	if (epoll_ctl(poll, EPOLL_CTL_ADD, _listen_sock, &ev) == -1) {
		ERROR("Could not add epoll listener for socket accept");
		return;
	}

	// Where the OS will put events for us
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	while (1) {
		int num_evs = epoll_wait(poll, events, EPOLL_MAX_EVENTS, 100);
		
		// Since we're polling at an interval, it's possible no events
		// will have happened
		if (num_evs < 1) {
			continue;
		}
		
		// Some events actually did happen; go through them all!
		for (int i = 0; i < num_evs; i++) {
			struct epoll_event ev = events[i];
			
			if (ev.data.fd == _listen_sock) {
				// If we have an incoming connection waiting
				_socket_accept_client(thread);
			} else {
				// Pass off the necessary data to the handler
				_socket_handle_client((client_t*)ev.data.ptr, ev.events);
			}
		}
	}
}

static gboolean _socket_setup_listener() {
	struct sockaddr_in addy;
	
	if ((_listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("Could not create socket");
		return FALSE;
	}
	
	addy.sin_family = AF_INET;
	addy.sin_port = htons(option_port());
	addy.sin_addr.s_addr = inet_addr(option_address());
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return FALSE;
	}
	
	if (bind(_listen_sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERROR("Could not bind");
		return FALSE;
	}
	
	if (listen(_listen_sock, LISTEN_BACKLOG) == -1) {
		ERROR("Could not listen");
		return FALSE;
	}
	
	return TRUE;
}

/**
 * Setup the thread pools
 */
gboolean socket_init() {
	// Setup the listener socket
	if (!_socket_setup_listener()) {
		ERROR("Could not setup listener");
		return FALSE;
	}
	
	threads = malloc(THREADS * sizeof(*threads));
	for (int i = 0; i < THREADS; i++) {
		gpointer thread = malloc(sizeof(short));
		*((short*)thread) = i;
		
		*(threads + i) = g_thread_create((GThreadFunc)_socket_loop, thread, TRUE, NULL);
	}
	
	return TRUE;
}

void socket_finish() {
	for (int i = 0; i < THREADS; i++) {
		g_thread_join(*(threads + i));
	}
}

void socket_close(client_t *client) {
	// Closing the socket also causes the OS to remove it from epoll
	close(client->sock);
	socket_clear_timer(client);
	socket_command_free(client);
	free(client);
}

/**
 * Free everything inside of the client command.
 */
void socket_command_free(client_t *client) {
	if (client->command == NULL) {
		// Nothing to free
		return;
	}
	
	g_string_free(client->command->socket_buffer, TRUE);
	g_string_free(client->command->buffer, TRUE);
	free(client->command);
	client->command = NULL;
}

void socket_set_timer(client_t *client) {
	int timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
	// If something goes wrong with the timer, just kill the stupid client
	if (timer == -1) {
		socket_close(client);
		return;
	}
	
	struct itimerspec spec;
	
	// No interval on the timer
	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;
	
	// Specify the number of seconds to wait
	spec.it_value.tv_sec = option_timeout();
	spec.it_value.tv_nsec = 0;
	
	timerfd_settime(timer, 0, &spec, NULL);
	
	// Store the timer on the client for cancelling
	client->timer = timer;
	
	// Add the timer to this epoll
	_socket_epoll_add(timer, client);
	
	DEBUG("Timer set on client");
}

void socket_clear_timer(client_t *client) {
	// Don't close stdin, stdout, or stderr
	if (client->timer > 2) {
		close(client->timer);
		client->timer = 0;
	}
}