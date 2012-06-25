#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "pubsub.h"

void client_handshake(client_t *client) {
	gboolean all_good = TRUE;
	enum handlers handler = none;

	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	
	// Parse the headers and see if we can handle them
	if (client->buffer->len && soup_headers_parse(client->buffer->str, client->buffer->len, req_headers)) {
		if (rfc6455_handles(req_headers)) {
			handler = rfc6455;
			all_good = rfc6455_handshake(client, req_headers);
		}
	}
	
	soup_message_headers_free(req_headers);
	
	if (all_good) {
		if (handler == none) {
			// Everything went well, but we don't have a handler yet
			// Maybe the client hasn't finished sending headers? Let's
			// wait for another read
			DEBUG("Timer set on client");
			socket_set_timer(client);
		} else {
			// The handshake is complete, and there is a handler.
			// We're done here.
			client->initing = 0;
			client->handler = handler;
			
			socket_clear_timer(client);
			socket_clear_buffer(client);
			sub_client_ready(client);
		}
	} else {
		DEBUG("Client handler not found");
		socket_close(client);
	}
}

void client_message(client_t* client) {
	
}

/**
 * Write the message out to the socket.
 */
void ws_client_write(client_t *client, char* msg) {
	int frame_len;
	char *frame = rfc6455_prepare_frame(msg, &frame_len);
	
	if (frame_len > -1) {
		write(client->sock, frame, frame_len);
		free(frame);
	} else {
		ERROR("Something went wrong with frame creation...");
	}
}