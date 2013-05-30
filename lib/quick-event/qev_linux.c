/**
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * The events that a socket can be read on.
 */
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;

/**
 * Everything happens on the same instance of epoll
 */
static int _epoll;

#ifndef QEV_NO_TIMERS
	/**
	 * Any registered timers
	 */
	static _timer_t _timers[] = {
		#define QEV_TIMER(fn, sec, ms, flags) {fn, -1, 0, flags},
			QEV_TIMERS
		#undef QEV_TIMER
	};
#endif

/**
 * Accept connections on the socket.
 */
static void _qev_accept(QEV_CLIENT_T *server)
{
	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;

	while (1) {
		qev_socket_t client_sock = accept(QEV_CSLOT(server, socket), NULL, NULL);
		if (client_sock == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}

			g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL,
					"Could not accept client: %s", strerror(errno));

			// If anything else goes wrong, be sure to re-arm the listen FD so that the OS
			// knows it should fire it again, otherwise it will NEVER be triggered again
			// (this is edge triggering, not level)
			ev.data.ptr = server;
			if (epoll_ctl(_epoll, EPOLL_CTL_MOD, QEV_CSLOT(server, socket), &ev) == -1) {
				g_log(QEV_DOMAIN, G_LOG_LEVEL_ERROR, "Could not re-arm listen FD: %s",
						 strerror(errno));
			}

			return;
		}

		if (fcntl(client_sock, F_SETFL, O_NONBLOCK) == -1) {
			g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING,
					"Could not set client non-blocking (fd %d)", client_sock);
			close(client_sock);
			continue;
		}

		qev_flags_t flags = 0;
		SSL *ctx = NULL;

		if (QEV_CSLOT(server, _flags) & QEV_CMASK_SSL) {
			ctx = SSL_new(QEV_CSLOT(server, ssl_ctx));

			int err;
			if ((err = SSL_set_fd(ctx, client_sock)) == 0) {
				g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING, "SSL_set_fd failed: %s",
						ERR_reason_error_string(SSL_get_error(ctx, err)));
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

		QEV_CLIENT_T *client = qev_client_create();
		QEV_CSLOT(client, socket) = client_sock;
		QEV_CSLOT(client, ssl_ctx) = ctx;
		QEV_CSLOT(client, _flags) = flags;

		ev.data.ptr = client;
		if (epoll_ctl(_epoll, EPOLL_CTL_ADD, client_sock, &ev) == -1) {
			g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING, "EPOLL_ADD(fd %d): %s",
					client_sock, strerror(errno));

			close(client_sock);
			SSL_free(ctx);
			g_slice_free1(sizeof(*client), client);
			continue;
		}

		QEV_CLIENT_NEW_FN(client);
	}
}

static int _qev_sys_listen(
	const char *ip_address,
	const uint16_t port,
	QEV_CLIENT_T **client,
	int socket_type,
	char flags)
{
	qev_socket_t sock;

	if ((sock = socket(AF_INET, socket_type, 0)) == -1) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen()->socket(): %s", strerror(errno));
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
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen()->setsockopt(): %s", strerror(errno));
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		close(sock);
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen()->fcntl(): %s", strerror(errno));
		return -1;
	}

	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		close(sock);
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen()->bind(): %s", strerror(errno));
		return -1;
	}

	if (socket_type != SOCK_DGRAM) {
		if (listen(sock, QEV_LISTEN_BACKLOG) == -1) {
			close(sock);
			g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen()->listen(): %s", strerror(errno));
			return -1;
		}
	}

	QEV_CLIENT_T *_client = qev_client_create();
	QEV_CSLOT(_client, socket) = sock;
	QEV_CSLOT(_client, _flags) |= flags;

	struct epoll_event ev;
	ev.events = EPOLL_READ_EVENTS;
	ev.data.ptr = _client;

	if (epoll_ctl(_epoll, EPOLL_CTL_ADD, sock, &ev) == -1) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "EPOLL_ADD(): %s", strerror(errno));

		close(sock);
		g_slice_free1(sizeof(*_client), _client);
		return -1;
	}

	if (client != NULL) {
		*client = _client;
	}

	return 0;
}

qev_socket_t qev_sys_listen(const char *ip_address, const uint16_t port, QEV_CLIENT_T **client)
{
	return _qev_sys_listen(ip_address, port, client, SOCK_STREAM, QEV_CMASK_LISTENING);
}

qev_socket_t qev_sys_listen_udp(const char *ip_address, const uint16_t port)
{
	return _qev_sys_listen(ip_address, port, NULL, SOCK_DGRAM, QEV_CMASK_UDP);
}

void qev_dispatch()
{
	// Where the OS will put events for us
	struct epoll_event events[QEV_MAX_EVENTS];

	#ifndef QEV_NO_TIMERS
		// Set to true to indicate that a delayed timer fired this round
		gboolean _delayed_timers[G_N_ELEMENTS(_timers)];
		memset(&_delayed_timers, FALSE, sizeof(_delayed_timers));
	#endif

	int num_evs = epoll_wait(_epoll, events, QEV_MAX_EVENTS, QEV_EPOLL_TIMEOUT);
	if (num_evs < 1) {
		if (num_evs == -1 && errno != EINTR) {
			g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING, "epoll_wait(): %s", strerror(errno));
		}
		return;
	}

	for (int i = 0; i < num_evs; i++) {
		QEV_STATS_INC(qev_event);

		struct epoll_event ev = events[i];
		QEV_CLIENT_T *client = ev.data.ptr;
		uint32_t events = ev.events;

		#ifndef QEV_NO_TIMERS
			if (((gsize)client) < G_N_ELEMENTS(_timers)) {
				// Have to read the timer for it to continue to fire on an interval
				char buff[8];
				int unused __attribute__((unused));
				unused = read(_timers[(gsize)client].timer, buff, sizeof(buff));

				if (_timers[(gsize)client].flags & QEV_TIMER_DELAYED) {
					_delayed_timers[(gsize)client] = TRUE;
				} else {
					qev_timer_fire(&_timers[(gsize)client]);
				}
			} else
		#endif

		if (QEV_CSLOT(client, _flags) & QEV_CMASK_LISTENING) {
			_qev_accept(client);

		#ifdef QEV_CLIENT_READ_UDP_FN
		} else if (QEV_CSLOT(client, _flags) & QEV_CMASK_UDP) {
			qev_client_read_udp(client);
		#endif

		} else {
			if (events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
				qev_close(client);
			} else if (events & EPOLLIN) {
				qev_client_read(client);
			} else {
				g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING,
						"qev_dispatch: Unknown event from epoll: %d", events);
			}
		}
	}

	#ifndef QEV_NO_TIMERS
		for (gsize i = 0; i < G_N_ELEMENTS(_delayed_timers); i++) {
			if (_delayed_timers[i]) {
				qev_timer_fire(&_timers[i]);
			}
		}
	#endif
}

int qev_read(QEV_CLIENT_T *client, char *buff, size_t buff_size)
{
	if (QEV_CSLOT(client, socket) == -1) {
		return 0;
	}

	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		qev_client_lock(client);
		int res = SSL_read(QEV_CSLOT(client, ssl_ctx), buff, buff_size);
		qev_client_unlock(client);
		return res;
	}

	return read(QEV_CSLOT(client, socket), buff, buff_size);
}

int qev_write(QEV_CLIENT_T *client, char *buff, size_t buff_size)
{
	if (QEV_CSLOT(client, socket) == -1) {
		return -1;
	}

	int ret;

	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		qev_client_lock(client);
		int sent = SSL_write(QEV_CSLOT(client, ssl_ctx), buff, buff_size);
		qev_client_unlock(client);

		ret = sent <= 0 ? -1 : sent;
	} else {
		ret = send(QEV_CSLOT(client, socket), buff, buff_size, MSG_NOSIGNAL);
	}

	return ret;
}

void qev_sys_client_closed(QEV_CLIENT_T *client)
{
	qev_client_lock(client);

	// This will also remove it from epoll!
	close(QEV_CSLOT(client, socket));
	QEV_CSLOT(client, socket) = -1;

	qev_client_unlock(client);
}

int qev_sys_init()
{
	// 1 -> a positive, int size must be given; ignored by new kernels
	_epoll = epoll_create(1);
	if (_epoll < 1) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "epoll_create(): %s", strerror(errno));
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

	#define QEV_TIMER(fn, sec, ms, flags) \
		ev.data.ptr = (void*)i; \
		_timers[i].timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK); \
		if (_timers[i].timer == -1) { \
			g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "timerfd_create(): %s", strerror(errno)); \
			return -1; \
		} \
		spec.it_value.tv_sec = sec; \
		spec.it_value.tv_nsec = QEV_MS_TO_NSEC(ms); \
		spec.it_interval.tv_sec = sec; \
		spec.it_interval.tv_nsec = QEV_MS_TO_NSEC(ms); \
		timerfd_settime(_timers[i].timer, 0, &spec, NULL); \
		if (epoll_ctl(_epoll, EPOLL_CTL_ADD, _timers[i].timer, &ev) == -1) { \
			g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "EPOLL_ADD(): %s", strerror(errno)); \
			close(_timers[i].timer); \
			return -1; \
		} \
		i++;

		QEV_TIMERS
	#undef QEV_TIMER

	return 0;
}

int qev_chuser(const char *username)
{
	struct passwd *user = getpwnam(username);

	if (user == NULL) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_chuser(): %s", strerror(errno));
		return -1;
	}

	if (user->pw_gid == 0 || user->pw_uid == 0) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "Cowardly refusing to run as root: %s",
				strerror(errno));
		return -1;
	}

	if (setgid(user->pw_gid) == -1) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_chuser(): %s", strerror(errno));
		return -1;
	}

	if (setuid(user->pw_uid) == -1) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_chuser(): %s", strerror(errno));
		return -1;
	}

	return 0;
}
