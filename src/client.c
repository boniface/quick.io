#include "qio.h"

status_t client_handshake(client_t *client) {
	status_t status = CLIENT_GOOD;
	enum handlers handler = h_none;
	GString *buffer = client->message->socket_buffer;

	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	gchar *path;
	
	// Parse the headers and see if we can handle them
	if (buffer->len && soup_headers_parse_request(buffer->str, buffer->len, req_headers, NULL, &path, NULL)) {
		if (rfc6455_handles(path, req_headers)) {
			handler = h_rfc6455;
			status = rfc6455_handshake(client, req_headers);
		}
	}
	
	g_free(path);
	soup_message_headers_free(req_headers);
	
	if (status & CLIENT_GOOD && handler == h_none) {
		// Everything went well, but we don't have a handler yet
		// Maybe the client hasn't finished sending headers? Let's
		// wait for another read
		return CLIENT_WAIT;
	} else if (status & CLIENT_WRITE) {
		// Set the client's handler for future reference
		client->handler = handler;
		
		// We read the header successfully, clean up after ourselves
		g_string_truncate(client->message->socket_buffer, 0);
		
		return CLIENT_WRITE;
	} else {
		DEBUG("Client handler not found");
		return CLIENT_ABORTED;
	}
}

status_t client_message(client_t* client) {
	status_t status;
	
	if (client->message->remaining_length) {
		switch (client->handler) {
			case h_rfc6455:
				status = rfc6455_continue(client);
				break;
			
			default:
			case h_none:
				ERROR("Client went into message processing without a handler");
				return CLIENT_ABORTED;
		}
	} else {
		switch (client->handler) {
			case h_rfc6455:
				status = rfc6455_incoming(client);
				break;
			
			default:
			case h_none:
				ERROR("Client went into message processing without a handler");
				return CLIENT_ABORTED;
		}
	}
	
	// If everything went well with the handler, process the message
	if (status == CLIENT_GOOD) {
		status = evs_server_handle(client);
		
		#warning Need to handle different client status messages from handlers appropriately
	}
	
	return status;
}

status_t client_write(client_t *client, message_t *message) {
	// Extract the message from the client, if necessary
	if (message == NULL) {
		if (client->message == NULL) {
			return CLIENT_BAD_MESSAGE;
		}
		
		message = client->message;
	}
	
	// The actual frame to send to the client
	char *frame;
	
	// The amount of data to write to the socket
	int frame_len = 0;
	
	switch (client->handler) {
		case h_rfc6455:
			frame = rfc6455_prepare_frame_from_message(message, &frame_len);
			break;
		
		default:
			return CLIENT_ABORTED;
	}
	
	status_t status = client_write_frame(client, frame, frame_len);
	free(frame);
	return status;
}

status_t client_write_frame(client_t *client, char *frame, int frame_len) {
	if (frame_len > -1) {
		send(client->sock, frame, frame_len, MSG_NOSIGNAL);
		return CLIENT_GOOD;
	} else {
		ERROR("Something went wrong with frame creation...");
		return CLIENT_ABORTED;
	}
}