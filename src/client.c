#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "commands.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "pubsub.h"

/**
 * The list of dead clients that need to be removed.
 */
static GPtrArray *_dead_clients;

/**
 * Dead clients lock.
 *
 * From the GLib docs: "It is not necessary to initialize a mutex that has been
 * created that has been statically allocated."
 */
static GMutex _dead_clients_lock;

/**
 * Anything that needs to happen once a client is ready for talking.
 */
static void _client_ready(client_t *client) {
	// Set the user into a room
	sub_client_ready(client);
}

status_t client_handshake(client_t *client) {
	gboolean all_good = TRUE;
	enum handlers handler = h_none;
	GString *buffer = client->message->socket_buffer;

	SoupMessageHeaders *req_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
	
	// Parse the headers and see if we can handle them
	if (buffer->len && soup_headers_parse(buffer->str, buffer->len, req_headers)) {
		if (rfc6455_handles(req_headers)) {
			handler = h_rfc6455;
			all_good = rfc6455_handshake(client, req_headers);
		}
	}
	
	soup_message_headers_free(req_headers);
	
	if (all_good) {
		if (handler == h_none) {
			// Everything went well, but we don't have a handler yet
			// Maybe the client hasn't finished sending headers? Let's
			// wait for another read
			return CLIENT_WAIT;
		} else {
			// The handshake is complete, and there is a handler.
			// We're done here.
			client->initing = 0;
			client->handler = handler;
			_client_ready(client);
			
			return CLIENT_GOOD;
		}
	} else {
		DEBUG("Client handler not found");
		socket_close(client);
		return CLIENT_ABORTED;
	}
}

void client_cleanup() {
	// Lock the dead clients stuff to make sure we're ready for a swap
	g_mutex_lock(&_dead_clients_lock);
	
	// Only do cleanup if there are clients to cleanup
	if (_dead_clients->len == 0) {
		g_mutex_unlock(&_dead_clients_lock);
		return;
	}
	
	// The new array to hold the dead clients
	GPtrArray *clients = _dead_clients;
	
	// Create a new array for the new dead clients since it can't add to the old
	_dead_clients = g_ptr_array_new();
	
	g_mutex_unlock(&_dead_clients_lock);
	
	// Kill the client...muahahaha!
	for (int i = 0; i < clients->len; i++) {
		socket_close(g_ptr_array_index(clients, i));
	}
	
	// Clean up the now-freed array
	g_ptr_array_free(clients, TRUE);
}

void client_kill(client_t *client) {
	// Make sure we're not interefering with a swap on the dead clients
	g_mutex_lock(&_dead_clients_lock);
	
	g_ptr_array_add(_dead_clients, client);
	
	g_mutex_unlock(&_dead_clients_lock);
}

status_t client_message(client_t* client) {
	status_t status;
	
	switch (client->handler) {
		case h_rfc6455:
			status = rfc6455_incoming(client);
			break;
		
		case h_none:
		default:
			// This should NEVER happen
			ERROR("Client went into message processing without a handler");
			status = CLIENT_ABORTED;
			break;
	}
	
	// If everything went well with the handler, process the message
	if (status == CLIENT_GOOD) {
		status = command_handle(client);
		
		#warning Need to handle different client status messages from handlers appropriately
		
		if (status == CLIENT_GOOD && client_write(client, NULL) != CLIENT_GOOD) {
			status = CLIENT_ABORTED;
		}
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
			frame = rfc6455_prepare_frame(message, &frame_len);
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
		write(client->sock, frame, frame_len);
		return CLIENT_GOOD;
	} else {
		ERROR("Something went wrong with frame creation...");
		return CLIENT_ABORTED;
	}
}

gboolean client_init() {
	_dead_clients = g_ptr_array_new();
	return TRUE;
}