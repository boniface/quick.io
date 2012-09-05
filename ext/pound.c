#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <arpa/inet.h>

#define CLIENTS 1000
#define ADDRESS "127.0.0.1"

#define HANDSHAKE "GET /chat HTTP/1.1\n" \
	"Host: server.example.com\n" \
	"Upgrade: websocket\n" \
	"Connection: Upgrade\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\n" \
	"Origin: http://example.com\n" \
	"Sec-WebSocket-Version: 13\n\n"

#define HANDSHAKE_RESPONSE "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#define DEBUG(out) _LOG("DEBUG", out)
#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#define WARN(out) _LOG("WARN", out)
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)
#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

#define SUBSCRIBE_1477 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""S""V"
#define SUBSCRIBE_1465 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""R""U"
#define SUBSCRIBE_1469 "\x81""\x9D""a""b""c""d""N""\x11""\x16""\x06""[""R""Y""\x14""\r""\x03""\n""\n""\\""M""\n""\x0c""\x13""M""\x17""\x0c""\x14""\x0f""\x01""\x17""N""S""W""R""X"
#define UNSUBSCRIBE_1477 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""U""T"
#define UNSUBSCRIBE_1465 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""T""V"
#define UNSUBSCRIBE_1469 "\x81""\x9F""a""b""c""d""N""\x17""\r""\x17""\x14""\x00""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^""K""\x08""\n""\x11""K""\x15""\n""\x16""\t""\x03""\x11""L""U""U""T""Z"

static GHashTable *clients;
struct event_base *_base;

void readcb(struct bufferevent *bev, void *arg) {
	char buf[1024];
	struct evbuffer *input = bufferevent_get_input(bev);
	
	int *inited = arg;
	
	if (!(*inited)) {
		struct evbuffer_ptr evb = evbuffer_search(input, HANDSHAKE_RESPONSE, sizeof(HANDSHAKE_RESPONSE)-1, NULL);
		
		if (evb.pos != -1) {
			g_hash_table_insert(clients, bev, bev);
			*inited = 1;
		}
	} 
	
	if (*inited) {
		while (evbuffer_remove(input, buf, sizeof(buf)) > 0);
	}
}

void eventcb(struct bufferevent *bev, short events, void *ptr) {
	if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
		g_hash_table_remove(clients, bev);
		bufferevent_free(bev);
	}
}

static void clients_cb(evutil_socket_t fd, short what, void *arg) {
	printf("Clients Connected: %d\n", g_hash_table_size(clients));
}

static void be_random(evutil_socket_t fd, short what, void *arg) {
	GHashTableIter iter;
	struct bufferevent *bev;
	
	g_hash_table_iter_init(&iter, clients);
	while (g_hash_table_iter_next(&iter, (void*)&bev, (void*)&bev)) {
		struct evbuffer *out = bufferevent_get_output(bev);
		
		int *inited = bev->cbarg;
		if (!(*inited)) {
			continue;
		}
		
		switch (g_random_int_range(0, 6)) {
			case 0:
				evbuffer_add(out, SUBSCRIBE_1477, sizeof(SUBSCRIBE_1477)-1);
				break;
			
			case 1:
				evbuffer_add(out, SUBSCRIBE_1465, sizeof(SUBSCRIBE_1465)-1);
				break;
			
			case 2:
				evbuffer_add(out, SUBSCRIBE_1469, sizeof(SUBSCRIBE_1469)-1);
				break;
			
			case 3:
				evbuffer_add(out, UNSUBSCRIBE_1477, sizeof(UNSUBSCRIBE_1477)-1);
				break;
			
			case 4:
				evbuffer_add(out, UNSUBSCRIBE_1465, sizeof(UNSUBSCRIBE_1465)-1);
				break;
				
			case 5:
				evbuffer_add(out, UNSUBSCRIBE_1469, sizeof(UNSUBSCRIBE_1469)-1);
				break;
		}
	}
}

static void create_clients(evutil_socket_t fd, short what, void *arg) {
	static int runs = 0;
	
	if (runs++ == (CLIENTS / 1000)) {
		struct event *me = arg;
		event_del(me);
		return;
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5000);
	inet_pton(AF_INET, ADDRESS, &serv_addr.sin_addr);
	
	for (int i = 0; i < 1000; i++) {
		char *inited = g_malloc0(sizeof(*inited));
		
		struct bufferevent *bev = bufferevent_socket_new(_base, -1, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(bev, readcb, NULL, eventcb, (void*)inited);
		
		bufferevent_enable(bev, EV_READ|EV_WRITE);
		evbuffer_add(bufferevent_get_output(bev), HANDSHAKE, sizeof(HANDSHAKE)-1);
		
		bufferevent_socket_connect(bev, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	}
}

int main(int argc, char **argv) {
	clients = g_hash_table_new(NULL, NULL);
	_base = event_base_new();
	
	struct timeval interval;
	interval.tv_usec = 0;
	
	interval.tv_sec = 2;
	struct event *timer = event_new(_base, -1, EV_PERSIST, clients_cb, NULL);
	evtimer_add(timer, &interval);
	
	interval.tv_sec = 1;
	timer = event_new(_base, -1, EV_PERSIST, be_random, NULL);
	evtimer_add(timer, &interval);
	
	interval.tv_sec = 5;
	timer = event_new(_base, -1, EV_PERSIST, create_clients, NULL);
	timer->ev_arg = timer;
	evtimer_add(timer, &interval);
	
	event_base_dispatch(_base);
	
	return 0;
}