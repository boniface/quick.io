#include <sys/timerfd.h> // For all timer functions
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <glib.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#define DEBUG(out) _LOG("DEBUG", out)
#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#define WARN(out) _LOG("WARN", out)
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)
#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

#define TEST "THIS IS A MESSAGE!\n"

static GArray *clients;

void _array_remove(int val) {
	for (guint i = 0; i < clients->len; i++) {
		if (g_array_index(clients, int, i) == val) {
			g_array_remove_index_fast(clients, i);
			break;
		}
	}
}

void handle(const int client) {
	// Just echo back to the client
	char buff[1000];
	ssize_t len;
	while ((len = read(client, buff, sizeof(buff))) > 0) {
		send(client, buff, len, MSG_NOSIGNAL);
	}
}

int elisten() {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		exit(1);
	}
	
	struct sockaddr_in addy;
	addy.sin_family = AF_INET;
	addy.sin_port = htons(5000);
	addy.sin_addr.s_addr = inet_addr("0.0.0.0");
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	fcntl(sock, F_SETFL, O_NONBLOCK);
	
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		exit(1);
	}
	
	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		exit(1);
	}
	
	if (listen(sock, 10000) == -1) {
		exit(1);
	}
	
	return sock;
}

void handle_timer(int timer) {
	// Interval doesn't fire otherwise
	char buff[100];
	read(timer, buff, sizeof(buff));
	
	DEBUGF("Clients: %"G_GUINT64_FORMAT, clients->len);
	
	for (guint i = 0; i < clients->len; i++) {
		send(g_array_index(clients, int, i), TEST, sizeof(TEST)-1, MSG_NOSIGNAL);
	}
}

static void _sigint_handler(int sig) {
	DEBUG("SIGINT: Dying");
	exit(1);
}

int main() {
	signal(SIGINT, _sigint_handler);
	
	clients = g_array_new(FALSE, TRUE, sizeof(int));
	
	const int server = elisten();
	const int _epoll = epoll_create1(0);
	
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
	ev.data.fd = server;
	epoll_ctl(_epoll, EPOLL_CTL_ADD, server, &ev);
	
	struct itimerspec spec;
	spec.it_value.tv_sec = 1;
	spec.it_value.tv_nsec = 1;
	spec.it_interval.tv_sec = 1;
	spec.it_interval.tv_nsec = 1;
	
	int timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	timerfd_settime(timer, 0, &spec, NULL);
	ev.data.fd = timer;
	epoll_ctl(_epoll, EPOLL_CTL_ADD, timer, &ev);
	
	struct epoll_event *events = malloc(sizeof(*events) * 100);
	while (1) {
		int num_evs = epoll_wait(_epoll, events, 100, -1);
		
		if (num_evs < 1) {
			if (errno == EINTR) {
				continue;
			}
			
			if (errno == EFAULT) {
				ERROR("EFAULT");
			}
			
			perror("epoll_wait()");
			exit(1);
		}
		
		for (int i = 0; i < num_evs; i++) {
			if (events[i].data.fd == server) {
				while (1) {
					int conn = accept(server, (struct sockaddr*)NULL, NULL);
					
					if (conn == -1) {
						break;
					}
					
					if (fcntl(conn, F_SETFL, O_NONBLOCK) == -1) {
						printf("Error NONBLOCK: %s\n", strerror(errno));
						close(conn);
						continue;
					}
					
					struct epoll_event eve;
					eve.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
					eve.data.fd = conn;
					
					if (epoll_ctl(_epoll, EPOLL_CTL_ADD, conn, &eve) == -1) {
						close(conn);
						continue;
					}
					
					g_array_append_val(clients, conn);
				}
			} else if (events[i].data.fd == timer) {
				handle_timer(timer);
			} else {
				if (events[i].events & EPOLLRDHUP) {
					_array_remove(events[i].data.fd);
					close(events[i].data.fd);
				} else {
					handle(events[i].data.fd);
				}
			}
		}
	}
}