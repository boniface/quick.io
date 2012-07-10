#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define CLIENTS 10
#define THREADS 4
#define ADDRESS "127.0.0.1"

#define HANDSHAKE "GET /chat HTTP/1.1\n" \
	"Host: server.example.com\n" \
	"Upgrade: websocket\n" \
	"Connection: Upgrade\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\n" \
	"Origin: http://example.com\n" \
	"Sec-WebSocket-Version: 13\n\n"

#define SUBSCRIBE {'\x81', '\x8B', '7', '8', '9', '0', '^', 'P', 'K', 'C', 'B', 'Z', '\x03', '\x01', '\x03', '\x0f', '\x0e'}

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#define DEBUG(out) _LOG("DEBUG", out)
#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#define WARN(out) _LOG("WARN", out)
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)
#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

#define EPOLL_MAX_EVENTS 100
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET

uint which = 0;
int *epoll;
GHashTable *clients;

gpointer hitserver(gpointer none) {
	for (uint i = 0; i < CLIENTS/THREADS; i++) {
		DEBUG(".");
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		
		if (sock < 0) {
			ERRORF("Could not create socket: %s", strerror(errno));
			return NULL;
		}
		
		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(5000);
		inet_pton(AF_INET, ADDRESS, &serv_addr.sin_addr);
		
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
			ERRORF("Could not connect: %s", strerror(errno));
			continue;
		}
		
		fcntl(sock, F_SETFL, O_NONBLOCK);
		
		struct epoll_event ev;
		ev.events = EPOLL_READ_EVENTS;
		ev.data.fd = sock;
		
		if (epoll_ctl(*(epoll + (which++ % THREADS)), EPOLL_CTL_ADD, sock, &ev) == -1) {
			ERRORF("Could not add sock to epoll: %s", strerror(errno));
			continue;
		}
		
		if (send(sock, HANDSHAKE, sizeof(HANDSHAKE)-1, MSG_NOSIGNAL) < 0) {
			ERRORF("Could not send handshake: %s", strerror(errno));
			continue;
		}
		
		int *ptr = malloc(sizeof(*ptr));
		*ptr = sock;
		int *inited = malloc(sizeof(*inited));
		*inited = 0;
		g_hash_table_insert(clients, ptr, inited);
	}
	
	return NULL;
}

gpointer watch(gpointer thread) {
	struct epoll_event events[EPOLL_MAX_EVENTS];
	char buff[1024];
	char sub[] = SUBSCRIBE;
	uint closed = 0;
	
	size_t num = *((int*)thread);
	int poll = *(epoll + num);
	
	while (1) {
		int num_evs = epoll_wait(poll, events, EPOLL_MAX_EVENTS, 1000);
		
		if (num_evs < 1) {
			continue;
		}
		
		for (int i = 0; i < num_evs; i++) {
			struct epoll_event ev = events[i];
			int sock = ev.data.fd;
			
			if (ev.events & EPOLLRDHUP) {
				DEBUGF("Socket closed: %d", ++closed);
				close(sock);
			} else {
				DEBUG("In");
				
				int *inited = g_hash_table_lookup(clients, &sock);
				if (!(*inited)) {
					send(sock, sub, sizeof(sub), MSG_NOSIGNAL);
					*inited = 1;
				}
				
				while (read(sock, buff, sizeof(buff)) > 0) {
					;
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	epoll = malloc(THREADS * sizeof(*epoll));
	clients = g_hash_table_new(g_int_hash, g_int_equal);
	
	for (int i = 0; i < THREADS; i++) {
		*(epoll + i) = epoll_create(1);
	}
	
	for (int i = 0; i < THREADS; i++) {
		g_thread_new("connect", hitserver, NULL);
	}
	
	for (int i = 0; i < THREADS-1; i++) {
		size_t *num = malloc(sizeof(*num));
		*num = i;
		g_thread_new("watch", watch, num);
	}
	
	size_t *num = malloc(sizeof(*num));
	*num = THREADS-1;
	watch(num);
	
	return 0;
}