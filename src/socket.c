#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>

#include "qio.h"

// The socket we accept connections on
static int _listen_sock;

// The thread that accepts connections
static GThread *_thread;

// Our epoll instance
static int _epoll;

// The client used to do maintenance tasks
static client_t *_fake_client;

/**
 * Performs maintenance routines.
 */
static void _socket_maintenance() {
	evs_client_cleanup();
}

/**
 * Adds an epoll listener on the specified fd.
 */
static gboolean _socket_epoll_add(int fd, client_t *client) {
	// Events that we listen for on the new client socket
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = client;
	
	// Start listening on the socket for anything the client has to offer
	if (epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &ev) == -1) {
		ERROR("Could not add socket to epoll");
		socket_close(client);
		return FALSE;
	}
	
	return TRUE;
}

/**
 * Accepts a new client into our midst
 */
static gpointer _socket_accept_client(gpointer unused) {
	while (1) {
		int sock = accept(_listen_sock, (struct sockaddr*)NULL, NULL);
		
		// Couldn't accept...move on
		if (sock == -1) {
			continue;
		}
		
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
			ERROR("Could not set client non-blocking");
			close(sock);
			continue;
		}
		
		client_t *client = g_try_malloc0(sizeof(*client));
		if (client == NULL) {
			ERROR("Client could not be malloc()'d");
			close(sock);
			continue;
		}
		
		// Basic information about this client
		client->sock = sock;
		client->initing = 1;
		
		// This must happen before creating a timer and adding on epoll,
		// otherwise, the close callback could be fired with a client that
		// was never app_client_new()'d
		apps_client_connect(client);
		
		// Make the client finish the handshake quickly, or drop him
		// If we can't setup the timer, then move on
		if (socket_set_timer(client, 0, 0)) {
			// Add the client to our epoll
			_socket_epoll_add(sock, client);
			
			DEBUGF("A new client connected: %d", client->sock);
		}
	}
	
	return NULL;
}

/**
 * Create a new message struct in the client
 */
static void _socket_message_new(client_t *client) {
	message_t *message = client->message;
	
	// Replace any slots in the message that might have been freed
	if (message == NULL) {
		message = g_try_malloc0(sizeof(*message));
	}
	
	if (message->socket_buffer == NULL) {
		message->socket_buffer = g_string_sized_new(STRING_BUFFER(client));
	}
	
	if (message->buffer == NULL) {
		message->buffer = g_string_sized_new(STRING_BUFFER(client));
	}
	
	client->message = message;
}

static void _socket_handle_client(client_t *client, uint32_t evs) {
	gchar buffer[option_max_message_size()];
	
	DEBUGF("Event: %d", client->sock);

	if (evs & EPOLLRDHUP) {
		UTILS_STATS_INC(socket_hups);
		
		DEBUGF("Client HUP: %d", client->sock);
		// The underlying socket was closed
		socket_close(client);
	} else if (evs & EPOLLIN) {
		// Check the timer to see if it has expired
		// A read operation on a timerfd will return > -1 if the 
		// timer has expired.
		// 8 is the most bytes the int returned can be
		if (client->timer && read(client->timer, buffer, 8) > -1) {
			UTILS_STATS_INC(socket_timeouts);
			
			// If the client has been caught misbehaving...
			DEBUG("Misbehaving client closed because of timeout");
			socket_close(client);
			return;
		}
		
		// Clients typically aren't sending messages
		_socket_message_new(client);
		
		// Read the message the client sent, unless it's too large,
		// then kill the client
		gssize len;
		while ((len = read(client->sock, buffer, sizeof(buffer))) > 0) {
			// Put the buffer into our string
			g_string_append_len(client->message->socket_buffer, buffer, len);
			
			// If the client needs to enhance his calm, kill the connection.
			if (client->message->socket_buffer->len > (option_max_message_size() * MAX_BUFFER_SIZE_MULTIPLIER)) {
				UTILS_STATS_INC(socket_bad_clients);
				
				DEBUG("Client needs to enhance his calm");
				socket_close(client);
				return;
			}
		}
		
		// While there is still something on the socket buffer to process
		while (client->message && client->message->socket_buffer->len > 0) {
			status_t status;
			if (client->initing == 1) {
				DEBUG("Client handshake");
				status = client_handshake(client);
				
				// Headers are sent without encoding, don't use the client wrapper
				if (status & CLIENT_WRITE) {
					status = client_write_frame(client, client->message->buffer->str, client->message->buffer->len);
					
					// The handshake is complete, we're done here.
					client->initing = 0;
					
					// Set the user into a room
					evs_client_client_ready(client);
				}
			} else {
				UTILS_STATS_INC(socket_messages);
				
				DEBUG("Message from client");
				status = client_message(client);
				
				// Send back a framed response for the websocket
				if (status == CLIENT_WRITE && (status = client_write(client, NULL)) != CLIENT_GOOD) {
					status = CLIENT_ABORTED;
				}
			}
			
			// If the client becomes good, then clear the timer and let him live
			if (status == CLIENT_GOOD) {
				socket_clear_timer(client);
				
				// Clean up the message buffer, since we just finished processing him
				socket_message_clean(client, TRUE, FALSE);
			
			// The client gets 1 timer to make itself behave. If it doesn't in this
			// time, then we summarily kill it.
			} else if (status == CLIENT_WAIT && !client->timer) {
				UTILS_STATS_INC(socket_client_wait);
				
				socket_set_timer(client, 0, 0);
				
				// The buffer will still be set, we're waiting, so just exit
				break;
				
			// The client is misbehaving. Close him.
			} else {
				UTILS_STATS_INC(socket_bad_clients);
				
				DEBUGF("Bad client, closing: status=%d", status);
				socket_close(client);
				
				// The client is gone...we're done.
				break;
			}
		}
	}
}

/**
 * Tick on the socket to run maintenance operations.  This function also resets itself to
 * run again later.
 */
static void _socket_tick() {
	static guint ticks = 0;
	
	// Send out messages at the end of a loop (this will always be hit on event loop)
	evs_client_pub_messages();
	
	if (ticks++ % SOCKET_MAINTENANCE_CLEANUP == 0) {
		// Clean up any client subscriptions
		_socket_maintenance();
	}
	
	// Reset the timer
	socket_clear_timer(_fake_client);
	socket_set_timer(_fake_client, 0, SOCKET_MAINTENANCE_WAIT);
}

/**
 */
void socket_loop() {
	// Where the OS will put events for us
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	while (TRUE) {
		int num_evs = epoll_wait(_epoll, events, EPOLL_MAX_EVENTS, EPOLL_WAIT);
		
		// Since we're polling at an interval, it's possible no events
		// will have happened
		if (num_evs < 1) {
			continue;
		}
		
		#ifdef TESTING
			test_lock_acquire();
			test_lock_release();
		#endif
		
		// Some events actually did happen; go through them all!
		for (int i = 0; i < num_evs; i++) {
			struct epoll_event ev = events[i];
			client_t *client = ev.data.ptr;
			
			if (client == _fake_client) {
				_socket_tick();
			} else {
				// Pass off the necessary data to the handler
				_socket_handle_client(client, ev.events);
			}
		}
	}
}

gboolean socket_init() {
	struct sockaddr_in addy;
	
	if ((_listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR("Could not create socket");
		return FALSE;
	}
	
	addy.sin_family = AF_INET;
	addy.sin_port = htons(option_port());
	addy.sin_addr.s_addr = inet_addr(option_bind_address());
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return FALSE;
	}
	
	if (bind(_listen_sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not bind: %s", strerror(errno));
		return FALSE;
	}
	
	if (listen(_listen_sock, LISTEN_BACKLOG) == -1) {
		ERRORF("Could not listen: %s", strerror(errno));
		return FALSE;
	}
	
	return TRUE;
}

gboolean socket_init_process() {
	// 1 -> a positive, int size must be given; ignored by new kernels
	_epoll = epoll_create(1);
	
	if (_epoll < 1) {
		ERRORF("Could not init epoll: %s", strerror(errno));
		return FALSE;
	}
	
	GError *error = NULL;
	_thread = g_thread_try_new(__FILE__, _socket_accept_client, NULL, &error);
	if (_thread == NULL) {
		ERRORF("Could not init socket accept in thread: %s", error->message);
		return FALSE;
	}
	
	_fake_client = g_try_malloc0(sizeof(*_fake_client));
	if (_fake_client == NULL) {
		ERROR("_fake_client could not be malloc()'d");
		return FALSE;
	}
	
	// Setup the maintenance job
	_socket_tick();
	
	return TRUE;
}

void socket_close(client_t *client) {
	// It's possible that we're working with _fake_client
	if (client == NULL) {
		return;
	}
	
	DEBUGF("A client closed: %d", client->sock);
	
	// Inform any apps that are tracking clients that a client has died
	apps_client_close(client);
	
	// Remove the client from all his subscriptions
	evs_client_client_free(client);
	
	// Closing the socket also causes the OS to remove it from epoll
	close(client->sock);
	socket_clear_timer(client);
	socket_message_free(client);
	free(client);
}

void socket_message_free(client_t *client) {
	message_t *message = client->message;
	
	// If the client doesn't have a message, don't be stupid
	if (message == NULL) {
		return;
	}
	
	g_string_free(message->buffer, TRUE);
	g_string_free(message->socket_buffer, TRUE);
	free(message);
	client->message = NULL;
}

void socket_message_clean(client_t *client, gboolean truncate_buffer, gboolean truncate_socket_buffer) {
	message_t *message = client->message;
	
	if (message == NULL) {
		// Nothing to free
		return;
	}
	
	message->remaining_length = 0;
	message->type = 0;
	message->mask = 0;
	
	// Get rid of the message buffer, the message has been handled
	if (truncate_buffer) {
		g_string_truncate(message->buffer, 0);
	}
	
	if (truncate_socket_buffer) {
		g_string_truncate(message->socket_buffer, 0);
	}
}

gboolean socket_set_timer(client_t *client, int timeout_sec, long timeout_nano) {
	int timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
	// If something goes wrong with the timer, just kill the stupid client
	if (timer == -1) {
		socket_close(client);
		return FALSE;
	}
	
	struct itimerspec spec;
	
	// No interval on the timer
	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;
	
	// Only use the default value if both are set to 0
	if (timeout_sec == 0 && timeout_nano == 0) {
		// Specify the number of seconds to wait
		spec.it_value.tv_sec = option_timeout();
		spec.it_value.tv_nsec = 0;
	} else {
		spec.it_value.tv_sec = timeout_sec;
		spec.it_value.tv_nsec = timeout_nano;
	}
	
	timerfd_settime(timer, 0, &spec, NULL);
	
	// Store the timer on the client for cancelling
	client->timer = timer;
	
	if (client != _fake_client) {
		DEBUG("Setting timer on client");
	}
	
	// Add the timer to this epoll
	return _socket_epoll_add(timer, client);
}

void socket_clear_timer(client_t *client) {
	// Don't close stdin, stdout, or stderr
	if (client->timer > 2) {
		close(client->timer);
		client->timer = 0;
	}
}

#ifdef TESTING
#include "../test/test_socket.c"
#endif