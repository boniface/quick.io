#include <sys/timerfd.h> // For all timer functions
#include "qio.h"

// The number of bytes necessary to read a timer
#define TIMER_BUFFER_SIZE 8

// Our epoll instance
static int _epoll;

// The client that is notified for accept events
static client_t *_accept;
static int _accept_socket;

// The timer used for maintenance
static int _timer;

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

void _qsys_accept() {
	// Loop until there are no errors accepting a socket and setting options
	while (TRUE) {
		qsys_socket client_socket = accept(_accept_socket, (struct sockaddr*)NULL, NULL);
		
		if (client_socket == -1) {
			return;
		}
		
		if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1) {
			ERROR("Could not set client non-blocking");
			_qsys_close(client_socket);
			continue;
		}
		
		int opt = 1;
		if (setsockopt(_accept_socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
			ERROR("Could not set socket keep alive");
			_qsys_close(client_socket);
			continue;
		}
		
		client_t *client = g_slice_alloc0(sizeof(*client));
		client->socket = client_socket;
		
		// Listen for events from the client
		if (!_epoll_add(client_socket, client)) {
			ERROR("Could not add client to epoll");
			_qsys_close(client_socket);
			g_slice_free1(sizeof(*client), client);
			continue;
		}
		
		conns_client_new(client);
	}
}

gboolean _qsys_init(gchar *address, guint16 port) {
	if ((_accept_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR("Could not create socket");
		return 0;
	}
	
	struct sockaddr_in addy;
	addy.sin_family = AF_INET;
	addy.sin_port = htons(port);
	addy.sin_addr.s_addr = inet_addr(address);
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(_accept_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return FALSE;
	}
	
	if (fcntl(_accept_socket, F_SETFL, O_NONBLOCK) == -1) {
		ERROR("Could not set accept socket non-blocking");
		return FALSE;
	}
	
	if (bind(_accept_socket, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not bind: %s", strerror(errno));
		return FALSE;
	}
	
	if (listen(_accept_socket, LISTEN_BACKLOG) == -1) {
		ERRORF("Could not listen: %s", strerror(errno));
		return FALSE;
	}
		
	_accept = g_malloc0(sizeof(*_accept));
	
	// 1 -> a positive, int size must be given; ignored by new kernels
	_epoll = epoll_create(1);
	
	if (_epoll < 1) {
		ERRORF("Could not init epoll: %s", strerror(errno));
		return FALSE;
	}
	
	if (!_epoll_add(_accept_socket, _accept)) {
		ERROR("Could not add listening socket to epoll.");
		return FALSE;
	}
	
	return TRUE;
}

void _qsys_dispatch() {
	// Where the OS will put events for us
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	// If the maintenance tick should be run this round
	gboolean tick = FALSE;
	
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
	
	for (int i = 0; i < num_evs; i++) {
		struct epoll_event ev = events[i];
		client_t *client = ev.data.ptr;
		guint32 events = ev.events;
		
		if (client == NULL) {
			tick = TRUE;
		} else if (client == _accept) {
			_qsys_accept();
		} else if (events & EPOLLRDHUP) {
			// The underlying socket was closed
			conns_client_hup(client);
		} else if (events & EPOLLIN) {
			conns_client_data(client);
		}
	}
	
	// Run this after everything else so that if any clients made themselves good
	// in this round, we're not trampling them out
	if (tick) {
		conns_maintenance_tick();
		
		// Have to read the timer for it to continue to fire on an interval
		char buff[8];
		read(_timer, buff, sizeof(buff));
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

gboolean _qsys_init_maintenance() {
	_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
	if (_timer == -1) {
		return FALSE;
	}
	
	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));
	spec.it_value.tv_nsec = MS_TO_NSEC(MAINTENANCE_TICK);
	spec.it_interval.tv_nsec = MS_TO_NSEC(MAINTENANCE_TICK);
	timerfd_settime(_timer, 0, &spec, NULL);
	return _epoll_add(_timer, NULL);
}
