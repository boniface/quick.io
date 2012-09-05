#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

#define CLIENTS 25000
#define THREADS 4
#define ADDRESS "127.0.0.1"

// A list of addresses you can bind to (if opening tons of connections)
char *addresses[] = {"127.0.0.1", "0.0.0.0"};

#define HANDSHAKE "GET /chat HTTP/1.1\n" \
	"Host: server.example.com\n" \
	"Upgrade: websocket\n" \
	"Connection: Upgrade\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\n" \
	"Origin: http://example.com\n" \
	"Sec-WebSocket-Version: 13\n\n"

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#define DEBUG(out) _LOG("DEBUG", out)
#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#define WARN(out) _LOG("WARN", out)
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)
#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

#define EPOLL_MAX_EVENTS 100
#define EPOLL_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLOUT

#define SUBSCRIBE_1477 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""S""V"
#define SUBSCRIBE_1465 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""R""U"
#define SUBSCRIBE_1469 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""R""X"
#define UNSUBSCRIBE_1477 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""U""T"
#define UNSUBSCRIBE_1465 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""T""V"
#define UNSUBSCRIBE_1469 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""T""Z"

uint which = 0;
int *epoll;
GHashTable *clients;

guint64 now() {
	struct timeval tv;
	gettimeofday(&tv, NULL); 
	
	// In format: SECS + MS, right next to each other
	// 4567: 4 seconds, 567 ms
	
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void connect_clients() {
	guint64 cnt = 1;
	
	int which_address = 0;
	
	for (uint i = 0; i < CLIENTS; i++) {
		if (cnt++ % 1000 == 0) {
			DEBUG("1K Created");
		}
	
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		
		if (sock < 0) {
			ERRORF("Could not create socket: %s", strerror(errno));
			return;
		}
		
		struct sockaddr_in client_addr;
		memset(&client_addr, 0, sizeof(client_addr));
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = htons(0);
		char *address = *(addresses + (which_address++ % G_N_ELEMENTS(addresses)));
		inet_pton(AF_INET, address, &client_addr.sin_addr);
		
		fcntl(sock, F_SETFL, O_NONBLOCK);
		
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
			ERROR("Could not set socket option");
			close(sock);
			continue;
		}
		
		if (bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
			ERRORF("Could not bind: %s", strerror(errno));
			close(sock);
			continue;
		}
		
		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(5000);
		inet_pton(AF_INET, ADDRESS, &serv_addr.sin_addr);
		
		if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
			if (errno != EINPROGRESS) {
				ERRORF("Could not connect: %s", strerror(errno));
				close(sock);
				continue;
			}
		}
		
		struct epoll_event ev;
		ev.events = EPOLL_EVENTS;
		ev.data.fd = sock;
		
		int *ptr = malloc(sizeof(*ptr));
		*ptr = sock;
		int *inited = malloc(sizeof(*inited));
		*inited = 0;
		g_hash_table_insert(clients, ptr, inited);
		
		if (epoll_ctl(*(epoll + (which++ % THREADS)), EPOLL_CTL_ADD, sock, &ev) == -1) {
			ERRORF("Could not add sock to epoll: %s", strerror(errno));
			g_hash_table_remove(clients, ptr);
			continue;
		}
	}
}

gpointer be_random(gpointer none) {
	GHashTableIter iter;
	int *sock;
	int *inited;
	
	usleep(100000);
	
	while (1) {
		g_hash_table_iter_init(&iter, clients);
		while (g_hash_table_iter_next(&iter, (void*)&sock, (void*)&inited)) {
			if (!(*inited)) {
				continue;
			}
			
			char *message;
			gsize len;
			switch (g_random_int_range(0, 6)) {
				case 0:
					message = SUBSCRIBE_1477;
					len = sizeof(SUBSCRIBE_1477)-1;
					break;
				
				case 1:
					message = SUBSCRIBE_1465;
					len = sizeof(SUBSCRIBE_1465)-1;
					break;
				
				case 2:
					message = SUBSCRIBE_1469;
					len = sizeof(SUBSCRIBE_1469)-1;
					break;
				
				case 3:
					message = UNSUBSCRIBE_1477;
					len = sizeof(UNSUBSCRIBE_1477)-1;
					break;
				
				case 4:
					message = UNSUBSCRIBE_1465;
					len = sizeof(UNSUBSCRIBE_1465)-1;
					break;
					
				case 5:
					message = UNSUBSCRIBE_1469;
					len = sizeof(UNSUBSCRIBE_1469)-1;
					break;
			}
			
			send(*sock, message, len, MSG_NOSIGNAL);
		}
		
		usleep(1000000);
	}
	
	return NULL;
}

gpointer watch(gpointer thread) {
	struct epoll_event events[EPOLL_MAX_EVENTS];
	char buff[1024];
	uint closed = 0;
	
	size_t num = *((int*)thread);
	int poll = *(epoll + num);
	
	guint64 tick = 0;
	
	guint64 prev = now();
	
	while (1) {
		int num_evs = epoll_wait(poll, events, EPOLL_MAX_EVENTS, 1000);
		
		if (num_evs < 1) {
			continue;
		}
		
		for (int i = 0; i < num_evs; i++) {
			struct epoll_event ev = events[i];
			int sock = ev.data.fd;
			
			if (ev.events & EPOLLRDHUP) {
				closed++;
				DEBUGF("Socket closed: %d", closed);
				close(sock);
			} else if (ev.events & EPOLLOUT) {
				int *inited = g_hash_table_lookup(clients, &sock);
				if (!(*inited)) {
					send(sock, HANDSHAKE, sizeof(HANDSHAKE)-1, MSG_NOSIGNAL);
					*inited = 1;
				}
			}
			
			if (ev.events & EPOLLIN) {
				memset(&buff, 0, sizeof(buff));
				while (read(sock, buff, sizeof(buff)-1) > 0) {
					;
				}
				
				if (tick++ % 10000 == 0) {
					guint64 n = now();
					
					DEBUGF("+10K messages; Closed: %d - Elapsed: %"G_GUINT64_FORMAT, closed, n-prev);
					prev = n;
				}
			}
		}
	}
	
	return NULL;
}

int main(int argc, char *argv[]) {
	epoll = malloc(THREADS * sizeof(*epoll));
	clients = g_hash_table_new_full(g_int_hash, g_int_equal, free, free);
	
	for (int i = 0; i < THREADS; i++) {
		*(epoll + i) = epoll_create(1);
	}
	
	connect_clients();
	g_thread_new("random", be_random, NULL);
	
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