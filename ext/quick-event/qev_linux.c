/**
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * The events that a socket can be read on.
 */
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET;

/**
 * A shortcut for getting things into epoll
 */
#define EPOLL_ADD(fd, ev) \
	if (epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &ev) == -1) { \
		perror("EPOLL_ADD()"); \
		return -1; \
	}

/**
 * Held inside _timers, for quick reference
 */
typedef struct {
	/**
	 * The callback function
	 */
	qev_timer_cb fn;
	
	/**
	 * The timer's file descriptor
	 */
	int fd;
} _timer_t;

/**
 * Everything happens on the same instance of epoll
 */
static int _epoll;

#ifndef QEV_NO_TIMERS
	/**
	 * Any registered timers
	 */
	static _timer_t _timers[] = {
		#define QEV_TIMER(fn, sec, ms) {fn, -1},
			QEV_TIMERS
		#undef QEV_TIMER
	};
#endif

/**
 * Accept connections on the socket.
 */
static int _qev_accept(QEV_CLIENT_T *server) {
	// Loop until there are no errors accepting a socket and setting options
	while (1) {
		qev_socket_t client_sock = accept(QEV_CSLOT(server, socket), NULL, NULL);
		if (client_sock == -1) {
			return 0;
		}
		
		if (fcntl(client_sock, F_SETFL, O_NONBLOCK) == -1) {
			perror("Could not set client non-blocking");
			close(client_sock);
			continue;
		}
		
		qev_flags_t flags = 0;
		SSL *ctx = NULL;
		
		if (QEV_CSLOT(server, _flags) & QEV_CMASK_SSL) {
			ctx = SSL_new(QEV_CSLOT(server, ssl_ctx));
			
			int err;
			if ((err = SSL_set_fd(ctx, client_sock)) == 0) {
				fprintf(stderr, "SSL_set_fd failed: %s\n", ERR_reason_error_string(SSL_get_error(ctx, err)));
				close(client_sock);
				SSL_free(ctx);
				continue;
			}
			
			if (_qev_ssl_handshake(ctx, &flags) != 0) {
				close(client_sock);
				SSL_free(ctx);
				continue;
			}
			
			flags |= QEV_CMASK_SSL;
		}
		
		QEV_CLIENT_T *client = qev_create_client();
		QEV_CSLOT(client, socket) = client_sock;
		QEV_CSLOT(client, ssl_ctx) = ctx;
		QEV_CSLOT(client, _flags) = flags;
		
		struct epoll_event ev;
		ev.events = EPOLL_READ_EVENTS;
		ev.data.ptr = client;
		EPOLL_ADD(client_sock, ev);
		
		QEV_CLIENT_NEW_FN(client);
	}
}

/**
 * Listen on an IP address + port
 */
qev_socket_t qev_sys_listen(char *ip_address, uint16_t port, QEV_CLIENT_T **client) {
	qev_socket_t sock;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("qev_listen()->socket()");
		return -1;
	}
	
	struct sockaddr_in addy;
	addy.sin_family = AF_INET;
	addy.sin_port = htons(port);
	addy.sin_addr.s_addr = inet_addr(ip_address);
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		close(sock);
		perror("qev_listen()->setsockopt()");
		return -1;
	}
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		close(sock);
		perror("qev_listen()->fcntl()");
		return -1;
	}
	
	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		close(sock);
		perror("qev_listen()->bind()");
		return -1;
	}
	
	if (listen(sock, QEV_LISTEN_BACKLOG) == -1) {
		close(sock);
		perror("qev_listen()->listen()");
		return -1;
	}
	
	QEV_CLIENT_T *_client = qev_create_client();
	QEV_CSLOT(_client, socket) = sock;
	QEV_CSLOT(_client, _flags) |= QEV_CMASK_LISTENING;
	
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = _client;
	EPOLL_ADD(sock, ev);
	
	if (client != NULL) {
		*client = _client;
	}
	
	return 0;
}

void qev_dispatch() {
	// Where the OS will put events for us
	struct epoll_event events[QEV_MAX_EVENTS];
	
	int num_evs = epoll_wait(_epoll, events, QEV_MAX_EVENTS, -1);
	
	// It's possible that there just aren't any events, but still print an error
	if (num_evs < 1) {
		if (errno != EINTR) {
			perror("epoll_wait()");
		}
		return;
	}
	
	for (int i = 0; i < num_evs; i++) {
		struct epoll_event ev = events[i];
		QEV_CLIENT_T *client = ev.data.ptr;
		uint32_t events = ev.events;
		
		#ifndef QEV_NO_TIMERS
			if (((gsize)client) < (sizeof(_timers) / sizeof(*_timers))) {
				// Have to read the timer for it to continue to fire on an interval
				char buff[8];
				read(_timers[(gsize)client].fd, buff, sizeof(buff));
				
				_timers[(gsize)client].fn();
			} else
		#endif
		
		if (QEV_CSLOT(client, _flags) & QEV_CMASK_LISTENING) {
			_qev_accept(client);
		} else {
			if (events & EPOLLRDHUP) {
				qev_close(client);
			} else if (events & EPOLLIN) {
				qev_client_read(client);
			}
		}
	}
}

int qev_read(QEV_CLIENT_T *client, char *buff, size_t buff_size) {
	if (QEV_CSLOT(client, socket) == -1) {
		return 0;
	}
	
	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		return SSL_read(QEV_CSLOT(client, ssl_ctx), buff, buff_size);
	}
	
	return read(QEV_CSLOT(client, socket), buff, buff_size);
}

int qev_write(QEV_CLIENT_T *client, char *buff, size_t buff_size) {
	if (QEV_CSLOT(client, socket) == -1) {
		return -1;
	}
	
	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		int sent = SSL_write(QEV_CSLOT(client, ssl_ctx), buff, buff_size);
		return sent <= 0 ? -1 : sent;
	}
	
	return send(QEV_CSLOT(client, socket), buff, buff_size, MSG_NOSIGNAL);
}

void qev_sys_client_closed(QEV_CLIENT_T *client) {
	// This will also remove it from epoll!
	close(QEV_CSLOT(client, socket));
	QEV_CSLOT(client, socket) = -1;
}

int qev_sys_init() {
	// 1 -> a positive, int size must be given; ignored by new kernels
	_epoll = epoll_create(1);
	if (_epoll < 1) {
		perror("epoll_create()");
		return -1;
	}
	
	#ifndef QEV_NO_TIMERS
		// For adding the timers
		struct epoll_event ev;
		ev.events = EPOLL_READ_EVENTS;
		
		struct itimerspec spec;
		memset(&spec, 0, sizeof(spec));
		
		// On Linux, there's no way a pointer is going to be
		// anywhere near the number of timers, even if that number
		// is very large, so this should be fine
		size_t i = 0;
	#endif
	
	#define QEV_TIMER(fn, sec, ms) \
		ev.data.ptr = (void*)i; \
		_timers[i].fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK); \
		if (_timers[i].fd == -1) { \
			perror("timerfd_create()"); \
			return -1; \
		} \
		spec.it_value.tv_sec = sec; \
		spec.it_value.tv_nsec = QEV_MS_TO_NSEC(ms); \
		spec.it_interval.tv_sec = sec; \
		spec.it_interval.tv_nsec = QEV_MS_TO_NSEC(ms); \
		timerfd_settime(_timers[i].fd, 0, &spec, NULL); \
		EPOLL_ADD(_timers[i].fd, ev); \
		i++;
		
		QEV_TIMERS
	#undef QEV_TIMER
	
	return 0;
}

int qev_chuser(char *username) {
	struct passwd *user = getpwnam(username);
	
	if (user == NULL) {
		perror("qev_chuser()");
		return -1;
	}
	
	if (user->pw_gid == 0 || user->pw_uid == 0) {
		fprintf(stderr, "Cowardly refusing to run as root\n");
		return -1;
	}
	
	if (setgid(user->pw_gid) == -1) {
		perror("qev_chuser()");
		return -1;
	}
	
	if (setuid(user->pw_uid) == -1) {
		perror("qev_chuser()");
		return -1;
	}
	
	return 0;
}