#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "commands.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "pubsub.h"

status_t client_handshake(client_t *client) {
	gboolean all_good = TRUE;
	enum handlers handler = none;
	GString *buffer = client->message->socket_buffer;

	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	
	// Parse the headers and see if we can handle them
	if (buffer->len && soup_headers_parse(buffer->str, buffer->len, req_headers)) {
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
			return CLIENT_WAIT;
		} else {
			// The handshake is complete, and there is a handler.
			// We're done here.
			client->initing = 0;
			client->handler = handler;
			sub_client_ready(client);
			
			return CLIENT_GOOD;
		}
	} else {
		DEBUG("Client handler not found");
		socket_close(client);
		return CLIENT_ABORTED;
	}
}

status_t client_message(client_t* client) {
	status_t status;
	
	switch (client->handler) {
		case rfc6455:
			status = rfc6455_incoming(client);
			break;
		
		case none:
		default:
			// This should NEVER happen
			ERROR("Client went into message processing without a handler");
			status = CLIENT_ABORTED;
			break;
	}
	
	// If everything went well with the handler, process the message
	if (status == CLIENT_GOOD) {
		message_t *message = client->message;
		status = command_handle(message);
		
		if (status == CLIENT_GOOD && !client_write_response(client)) {
			status = CLIENT_ABORTED;
		}
	}
	
	return status;
}

status_t client_write(client_t *client, opcode_t type, char *msg) {
	int frame_len;
	char *frame;
	
	switch (client->handler) {
		case rfc6455:
			frame = rfc6455_prepare_frame(type, msg, &frame_len);
			break;
		
		default:
			return CLIENT_ABORTED;
	}
	
	if (frame_len > -1) {
		write(client->sock, frame, frame_len);
		free(frame);
		return CLIENT_GOOD;
	} else {
		ERROR("Something went wrong with frame creation...");
		return CLIENT_ABORTED;
	}
}

status_t client_write_response(client_t *client) {
	message_t *message = client->message;
	return client_write(client, message->type, message->buffer->str);
}