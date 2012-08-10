#include "qio.h"

status_t client_handshake(client_t *client) {
	status_t status = CLIENT_GOOD;
	GString *buffer = client->message->socket_buffer;
	
	// If the client hasn't yet sent the terminating \n\n or \r\n\r\n, then just
	// don't parse anything, because it's not complete
	if (!g_string_ends_with(buffer, HTTP_HEADER_TERMINATOR) && !g_string_ends_with(buffer, WEB_SOCKET_HEADER_TERMINATOR)) {
		return CLIENT_WAIT;
	}
	
	// Used by soup for header parsing
	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	gchar *path = NULL;
	
	// Parse the headers and see if we can handle them
	if (soup_headers_parse_request(buffer->str, buffer->len, req_headers, NULL, &path, NULL) == SOUP_STATUS_OK) {
		if (rfc6455_handles(path, req_headers)) {
			client->handler = h_rfc6455;
			status = rfc6455_handshake(client, req_headers);
		} else {
			// No handler was found, and the headers were parsed OK, so
			// we just can't support this client
			status = CLIENT_UNSUPPORTED;
		}
	} else {
		// If we can't parse the headers, then the client is being an idiot
		// So screw him, close the connection
		status = CLIENT_ABORTED;
	}
	
	// If the path couldn't even be read from the headers...yikes
	if (path != NULL) {
		g_free(path);
	}
	
	// Done reading headers
	soup_message_headers_free(req_headers);
	
	if (status == CLIENT_WRITE) {
		// We read the header successfully, clean up after ourselves
		g_string_truncate(client->message->socket_buffer, 0);
	}
	
	return status;
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
				// This should technically be impossible, nevertheless...
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
	gsize frame_len = 0;
	
	switch (client->handler) {
		case h_rfc6455:
			frame = rfc6455_prepare_frame_from_message(message, &frame_len);
			break;
		
		default:
			return CLIENT_ABORTED;
	}
	
	if (frame == NULL) {
		return CLIENT_ABORTED;
	}
	
	status_t status = client_write_frame(client, frame, frame_len);
	free(frame);
	return status;
}

status_t client_write_frame(client_t *client, char *frame, gsize frame_len) {
	// Frames will ALWAYS be larger than 0, there MUST be a header
	if (frame_len > 0) {
		send(client->sock, frame, frame_len, MSG_NOSIGNAL);
		return CLIENT_GOOD;
	} else {
		ERROR("Something went wrong with frame creation...");
		return CLIENT_ABORTED;
	}
}