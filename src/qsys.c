#include <sys/epoll.h>
#include <sys/timerfd.h>
#include "qio.h"

// The number of bytes necessary to read a timer
#define TIMER_BUFFER_SIZE 8

// Our epoll instance
static int _epoll;

// The client that is notified for accept events
static client_t *_accept;
static int _accept_socket;

// The timer used for maintenance
static int _timers[2];

#define TIMER_MAINT 0
#define TIMER_STATS 1

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

static gboolean _qsys_init(gchar *address, guint16 port) {
	_accept_socket = qsys_listen(address, port);
	if (_accept_socket == -1) {
		ERROR("Failed to listen on server socket. That blows.");
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

static gboolean _qsys_init_timers() {
	struct timespec waits[2] = {
		{0, MAINTENANCE_TICK},
		{option_stats_flush(), 0}
	};
	
	for (gsize i = 0; i < G_N_ELEMENTS(waits); i++) {
		_timers[i] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
		if (_timers[i] == -1) {
			return FALSE;
		}
		
		struct itimerspec spec;
		memset(&spec, 0, sizeof(spec));
		spec.it_value.tv_sec = waits[i].tv_sec;
		spec.it_value.tv_nsec = MS_TO_NSEC(waits[i].tv_nsec);
		spec.it_interval.tv_sec = waits[i].tv_sec;
		spec.it_interval.tv_nsec = MS_TO_NSEC(waits[i].tv_nsec);
		timerfd_settime(_timers[i], 0, &spec, NULL);
		
		if (!_epoll_add(_timers[i], (void*)i)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

static void _qsys_accept() {
	// Loop until there are no errors accepting a socket and setting options
	while (TRUE) {
		qsys_socket_t client_socket = accept(_accept_socket, NULL, NULL);
		
		if (client_socket == -1) {
			return;
		}
		
		if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1) {
			ERROR("Could not set client non-blocking");
			close(client_socket);
			continue;
		}
		
		int opt = 1;
		if (setsockopt(_accept_socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
			ERROR("Could not set socket keep alive");
			close(client_socket);
			continue;
		}
		
		client_t *client = g_slice_alloc0(sizeof(*client));
		client->socket = client_socket;
		
		// Listen for events from the client
		if (!_epoll_add(client_socket, client)) {
			ERROR("Could not add client to epoll");
			close(client_socket);
			g_slice_free1(sizeof(*client), client);
			continue;
		}
		
		conns_client_new(client);
	}
}

static void _qsys_dispatch() {
	// Where the OS will put events for us
	struct epoll_event events[EPOLL_MAX_EVENTS];
	
	// If the maintenance tick should be run this round
	gboolean maintenance = FALSE;
	
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
		
		if ((gsize)client <= G_N_ELEMENTS(_timers)+1) {
			// Have to read the timer for it to continue to fire on an interval
			char buff[8];
			read(_timers[(gsize)client], buff, sizeof(buff));
			
			if ((gsize)client == TIMER_STATS) {
				stats_flush();
			} else {
				maintenance = TRUE;
			}
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
	if (maintenance) {
		conns_maintenance_tick();
	}
}

void qsys_main_loop() {
	if (_qsys_init(option_bind_address(), option_port())) {
		DEBUGF("Sys inited, listening for connections on %d", option_port());
	} else {
		ERROR("_qsys_init() failed");
		return;
	}
	
	if (_qsys_init_timers()) {
		DEBUG("Timers inited");
	} else {
		ERROR("Coud not init timers");
		return;
	}
	
	while (TRUE) {
		_qsys_dispatch();
	}
}

qsys_socket_t qsys_listen(gchar *address, guint16 port) {
	qsys_socket_t sock;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR("Could not create socket");
		return -1;
	}
	
	struct sockaddr_in addy;
	addy.sin_family = AF_INET;
	addy.sin_port = htons(port);
	addy.sin_addr.s_addr = inet_addr(address);
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return -1;
	}
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ERROR("Could not set accept socket non-blocking");
		return -1;
	}
	
	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not bind: %s", strerror(errno));
		return -1;
	}
	
	if (listen(sock, LISTEN_BACKLOG) == -1) {
		ERRORF("Could not listen: %s", strerror(errno));
		return -1;
	}
	
	return sock;
}

gssize qsys_read(client_t *client, gchar *buff, gsize buff_size) {
	return read(client->socket, buff, buff_size);
}

gssize qsys_write(client_t *client, gchar *buff, gsize buff_size) {
	if (client->socket != 0) {
		return send(client->socket, buff, buff_size, MSG_NOSIGNAL);
	}
	
	return -1;
}

void qsys_close(client_t *client) {
	close(client->socket);
	client->socket = 0;
}

#ifdef TESTING
#include "../test/test_qsys.c"
#endif