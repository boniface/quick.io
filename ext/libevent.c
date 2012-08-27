#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define TEST "THIS IS A MESSAGE!"

static int clients = 0;

static void
echo_read_cb(struct bufferevent *bev, void *ctx)
{
		/* This callback is invoked when there is data to read on bev. */
		struct evbuffer *input = bufferevent_get_input(bev);
		struct evbuffer *output = bufferevent_get_output(bev);

		/* Copy all the data from the input buffer to the output buffer. */
		evbuffer_add_buffer(output, input);
}

static void
echo_event_cb(struct bufferevent *bev, short events, void *arg) {
	struct event *timer = arg;

	if (events & BEV_EVENT_ERROR) {
		perror("Error from bufferevent");
	}
	
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
		event_del(timer);
		clients--;
	}
}

static void timer_cb(evutil_socket_t fd, short what, void *arg) {
	struct bufferevent *bev = arg;
	struct evbuffer *output = bufferevent_get_output(bev);
	evbuffer_add(output, TEST, sizeof(TEST));
}

static void clients_cb(evutil_socket_t fd, short what, void *arg) {
	printf("Clients: %d\n", clients);
}

static void accept_conn_cb(struct evconnlistener *listener,	evutil_socket_t fd, struct sockaddr *address, int socklen,	void *ctx) {
	/* We got a new connection! Set up a bufferevent for it. */
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	struct event *timer = event_new(base, fd, EV_PERSIST, timer_cb, bev);

	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, timer);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	
	struct timeval one_sec;
	one_sec.tv_sec = 1;
	one_sec.tv_usec = 0;
	
	clients++;
	
	evtimer_add(timer, &one_sec);
}

int main(int argc, char **argv) {
		struct event_base *base;
		struct evconnlistener *listener;
		struct sockaddr_in sin;

		int port = 5000;

		if (argc > 1) {
			port = atoi(argv[1]);
		}
		if (port<=0 || port>65535) {
			puts("Invalid port");
			return 1;
		}

		base = event_base_new();
		if (!base) {
			puts("Couldn't open event base");
			return 1;
		}
		

		/* Clear the sockaddr before using it, in case there are extra
		 * platform-specific fields that can mess us up. */
		memset(&sin, 0, sizeof(sin));
		/* This is an INET address */
		sin.sin_family = AF_INET;
		/* Listen on 0.0.0.0 */
		sin.sin_addr.s_addr = htonl(0);
		/* Listen on the given port. */
		sin.sin_port = htons(port);

		listener = evconnlistener_new_bind(base, accept_conn_cb, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
		if (!listener) {
			perror("Couldn't create listener");
			return 1;
		}
		
		struct timeval one_sec;
		one_sec.tv_sec = 1;
		one_sec.tv_usec = 0;
		
		struct event *timer = event_new(base, -1, EV_PERSIST, clients_cb, NULL);
		evtimer_add(timer, &one_sec);

		event_base_dispatch(base);
		return 0;
}