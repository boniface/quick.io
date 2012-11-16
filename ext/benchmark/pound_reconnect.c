#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <arpa/inet.h>

#define TIMEOUT 100
#define ADDRESS "127.0.0.1"
#define PORT 81

#define HANDSHAKE "GET /qio HTTP/1.1\n" \
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

struct event_base *_base;
static int reconnects = 0;
static struct bufferevent *client = NULL;

static void reconnect(evutil_socket_t fd, short what, void *arg);

void readcb(struct bufferevent *bev, void *arg) {
	struct timeval t;
	t.tv_usec = 100000;
	t.tv_sec = 0;
	struct event *timer = event_new(_base, -1, 0, reconnect, NULL);
	evtimer_add(timer, &t);
}

void eventcb(struct bufferevent *bev, short events, void *ptr) {
	if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
		reconnect(0, 0, NULL);
	}
}

static void reconnect(evutil_socket_t fd, short what, void *arg) {
	if (client != NULL) {
		bufferevent_free(client);
	}
	
	reconnects++;
	
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, ADDRESS, &serv_addr.sin_addr);
	
	client = bufferevent_socket_new(_base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(client, readcb, NULL, eventcb, NULL);
	bufferevent_enable(client, EV_READ|EV_WRITE);
	evbuffer_add(bufferevent_get_output(client), HANDSHAKE, sizeof(HANDSHAKE)-1);
	bufferevent_socket_connect(client, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
}

static void reconnects_count_cb(evutil_socket_t fd, short what, void *arg) {
	printf("Reconnects: %d\n", reconnects);
}

int main(int argc, char **argv) {
	_base = event_base_new();
	
	struct timeval interval;
	interval.tv_usec = 0;
	interval.tv_sec = 2;
	struct event *timer = event_new(_base, -1, EV_PERSIST, reconnects_count_cb, NULL);
	evtimer_add(timer, &interval);
	
	reconnect(0, 0, NULL);
	
	event_base_dispatch(_base);
	
	return 0;
}