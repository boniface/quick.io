#include <sys/timerfd.h> // For all timer functions
#include "qio.h"

// The number of bytes necessary to read a timer
#define TIMER_BUFFER_SIZE 8

// Our epoll instance
static int _epoll;

/**
 * Adds an epoll listener on the specified fd.
 */
static gboolean _epoll_add(int fd, client_t *client) {
	// Events that we listen for on the new client socket
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = client;
	
	// Start listening on the socket for anything the client has to offer
	if (epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &ev) == -1) {
		ERROR("Could not add FD to epoll");
		return FALSE;
	}
	
	return TRUE;
}

gboolean _qsys_init() {
	// 1 -> a positive, int size must be given; ignored by new kernels
	_epoll = epoll_create(1);
	
	if (_epoll < 1) {
		ERRORF("Could not init epoll: %s", strerror(errno));
		return FALSE;
	}
	
	return TRUE;
}

client_t* _qsys_accept(qsys_socket socket) {
	// Loop until there are no errors accepting a socket and setting options
	while (TRUE) {
		qsys_socket client_socket = accept(socket, (struct sockaddr*)NULL, NULL);
		
		// Couldn't accept...move on
		if (client_socket == -1) {
			ERROR("Could not accept connection on socket");
			continue;
		}
		
		if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1) {
			ERROR("Could not set client non-blocking");
			_qsys_close(client_socket);
			continue;
		}
		
		client_t *client = g_try_malloc0(sizeof(*client));
		if (client == NULL) {
			ERROR("Client could not be malloc()'d");
			_qsys_close(client_socket);
			continue;
		}
		
		client->socket = client_socket;
		
		// Listen for events from the client
		if (!_epoll_add(client_socket, client)) {
			ERROR("Could not add client to epoll");
			_qsys_close(client_socket);
			free(client);
			continue;
		}
		
		return client;
	}
}

qsys_socket _qsys_listen(gchar *address, guint16 port) {
	qsys_socket listen_socket;
	
	if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR("Could not create socket");
		return 0;
	}
	
	struct sockaddr_in addy;
	addy.sin_family = AF_INET;
	addy.sin_port = htons(port);
	addy.sin_addr.s_addr = inet_addr(address);
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return 0;
	}
	
	if (bind(listen_socket, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not bind: %s", strerror(errno));
		return 0;
	}
	
	if (listen(listen_socket, LISTEN_BACKLOG) == -1) {
		ERRORF("Could not listen: %s", strerror(errno));
		return 0;
	}
	
	return listen_socket;
}

void _qsys_dispatch() {
	// Where the OS will put events for us
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	int num_evs = epoll_wait(_epoll, events, EPOLL_MAX_EVENTS, EPOLL_WAIT);
	
	// Since we're polling at an interval, it's possible no events
	// will have happened
	if (num_evs < 1) {
		ERROR("epoll_wait failed");
		return;
	}
	
	#ifdef TESTING
		test_lock_acquire();
		test_lock_release();
	#endif
	
	// Some events actually did happen; go through them all!
	for (int i = 0; i < num_evs; i++) {
		struct epoll_event ev = events[i];
		client_t *client = ev.data.ptr;
		guint32 events = ev.events;
		
		if (events & EPOLLRDHUP) {
			// The underlying socket was closed
			conns_client_hup(client);
		} else if (events & EPOLLIN) {
			char buffer[TIMER_BUFFER_SIZE];
			
			// Check the timer to see if it has expired
			// A read operation on a timerfd will return > -1 if the 
			// timer has expired.
			// 8 is the most bytes the int returned can be
			if (client->timer && read(client->timer, buffer, sizeof(buffer)) > -1) {
				conns_client_timer(client);
			} else {
				conns_client_data(client);
			}
		}
	}
}

gssize _qsys_read(qsys_socket socket, gchar *buff, gsize buff_len) {
	return read(socket, buff, buff_len);
}

gssize _qsys_write(qsys_socket socket, gchar *buff, gsize buff_len) {
	return send(socket, buff, buff_len, MSG_NOSIGNAL);
}

void _qsys_close(qsys_socket socket) {
	close(socket);
}

gboolean _qsys_timer_set(client_t *client, guint16 sec, guint16 ms) {
	qsys_timer timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
	// If something goes wrong with the timer, just kill the stupid client
	if (timer == -1) {
		ERROR("Could not create a timer for client");
		return FALSE;
	}
	
	struct itimerspec spec;
	
	// No interval on the timer
	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;
	
	spec.it_value.tv_sec = sec;
	spec.it_value.tv_nsec = MS_TO_NSEC(ms);
	
	timerfd_settime(timer, 0, &spec, NULL);
	
	// Store the timer on the client for cancelling
	client->timer = timer;
	
	// Add the timer to this epoll
	return _epoll_add(timer, client);
}

void _qsys_timer_clear(client_t *client) {
	// Don't close stdin, stdout, or stderr
	if (client->timer > 2) {
		close(client->timer);
		client->timer = 0;
	}
}