#include "qio.h"

// The client used to do maintenance tasks
static client_t *_fake_client;

/**
 * Create a new message struct in the client
 */
static void _socket_message_new(client_t *client) {
	message_t *message = client->message;
	
	// Replace any slots in the message that might have been freed
	if (message == NULL) {
		message = g_try_malloc0(sizeof(*message));
	}
	
	if (message->socket_buffer == NULL) {
		message->socket_buffer = g_string_sized_new(STRING_BUFFER(client));
	}
	
	if (message->buffer == NULL) {
		message->buffer = g_string_sized_new(STRING_BUFFER(client));
	}
	
	client->message = message;
}

/**
 * Tick on the socket to run maintenance operations.  This function also resets itself to
 * run again later.
 */
static void _conns_tick() {
	static guint ticks = 0;
	
	// Send out messages
	evs_client_pub_messages();
	
	if (ticks++ % CONNS_MAINTENANCE_CLEANUP == 0) {
		// Clean up any client subscriptions
		evs_client_cleanup();
	}
	
	// Reset the timer
	qsys_timer_clear(_fake_client);
	qsys_timer_set(_fake_client, 0, CONNS_MAINTENANCE_WAIT);
}

gboolean conns_qsys_ready() {
	_fake_client = g_try_malloc0(sizeof(*_fake_client));
	
	if (_fake_client == NULL) {
		ERROR("_fake_client could not be malloc()'d");
		return FALSE;
	}
	
	_conns_tick();
	
	return TRUE;
}

void conns_client_new(client_t *client) {
	// Basic information about this client
	client->initing = 1;
	
	// This must happen before creating a timer and adding on epoll,
	// otherwise, the close callback could be fired with a client that
	// was never app_client_new()'d
	apps_client_connect(client);
	
	// Make the client finish the handshake quickly, or drop him
	// If we can't setup the timer, then move on
	if (!qsys_timer_set(client, 0, 0)) {
		ERROR("Could not set timer on client: closing client");
		conns_client_close(client);
	}
}

void conns_client_close(client_t *client) {
	// It's possible that we're working with _fake_client
	if (client == NULL) {
		return;
	}
	
	DEBUGF("A client closed: %d", client->socket);
	
	// Inform any apps that are tracking clients that a client has died
	apps_client_close(client);
	
	// Remove the client from all his subscriptions
	evs_client_client_clean(client);
	
	// Closing the socket also causes the OS to remove it from epoll
	qsys_close(client);
	conns_message_free(client);
	free(client);
	mprobe(client);
}

void conns_client_hup(client_t *client) {
	UTILS_STATS_INC(conns_hups);
		
	DEBUGF("Client HUP: %d", client->socket);
	
	// The underlying socket was closed
	conns_client_close(client);
}

void conns_client_timer(client_t *client) {
	if (client == _fake_client) {
		_conns_tick();
	} else {
		UTILS_STATS_INC(conns_timeouts);
		
		// If the client has been caught misbehaving...
		DEBUG("Misbehaving client closed because of timeout");
		conns_client_close(client);
	}
}

void conns_client_data(client_t *client) {
	// Where data from the client's socket will be stored
	gchar buffer[option_max_message_size()];
	
	// Clients typically aren't sending messages
	_socket_message_new(client);
	
	// Read the message the client sent, unless it's too large,
	// then kill the client
	gssize len;
	while ((len = qsys_read(client, buffer, sizeof(buffer))) > 0) {
		DEBUG("READING");
		// Put the buffer into our string
		g_string_append_len(client->message->socket_buffer, buffer, len);
		
		// If the client needs to enhance his calm, kill the connection.
		if (client->message->socket_buffer->len > (option_max_message_size() * MAX_BUFFER_SIZE_MULTIPLIER)) {
			UTILS_STATS_INC(conns_bad_clients);
			
			DEBUG("Client needs to enhance his calm");
			conns_client_close(client);
			return;
		}
	}
	
	// While there is still something on the socket buffer to process
	while (client->message->socket_buffer->len > 0) {
		status_t status;
		if (client->initing == 1) {
			DEBUG("Client handshake");
			status = client_handshake(client);
			
			// Headers are sent without encoding, don't use the client wrapper
			if (status & CLIENT_WRITE) {
				status = client_write_frame(client, client->message->buffer->str, client->message->buffer->len);
				
				// The handshake is complete, we're done here.
				client->initing = 0;
				
				// Set the user into a room
				evs_client_client_ready(client);
			}
		} else {
			UTILS_STATS_INC(conns_messages);
			
			DEBUG("Message from client");
			status = client_message(client);
			
			// Send back a framed response for the websocket
			if (status == CLIENT_WRITE && (status = client_write(client, NULL)) != CLIENT_GOOD) {
				status = CLIENT_ABORTED;
			}
		}
		
		// If the client becomes good, then clear the timer and let him live
		if (status == CLIENT_GOOD) {
			qsys_timer_clear(client);
			
			// Clean up the message buffer, since we just finished processing him
			conns_message_clean(client, FALSE, TRUE);
		
		// The client gets 1 timer to make itself behave. If it doesn't in this
		// time, then we summarily kill it.
		} else if (status == CLIENT_WAIT) {
			UTILS_STATS_INC(conns_client_wait);
			
			qsys_timer_set(client, 0, 0);
			
			// The buffer will still be set, we're waiting, so just exit
			break;
			
		// The client is misbehaving. Close him.
		} else {
			UTILS_STATS_INC(conns_bad_clients);
			
			DEBUGF("Bad client, closing: status=%d", status);
			conns_client_close(client);
			client = NULL;
			
			// The client is gone...we're done.
			break;
		}
	}
	
	if (client != NULL && client->message->socket_buffer->len == 0 && client->message->remaining_length == 0) {
		conns_message_free(client);
	}
}

void conns_message_free(client_t *client) {
	// If the client doesn't have a message, don't be stupid
	if (client->message == NULL) {
		return;
	}
	
	g_string_free(client->message->buffer, TRUE);
	g_string_free(client->message->socket_buffer, TRUE);
	free(client->message);
	client->message = NULL;
}

void conns_message_clean(client_t *client, gboolean truncate_socket_buffer, gboolean truncate_buffer) {
	message_t *message = client->message;
	
	if (message == NULL) {
		// Nothing to free
		return;
	}
	
	message->remaining_length = 0;
	message->type = 0;
	message->mask = 0;
	
	// Get rid of the message buffer, the message has been handled
	if (truncate_buffer) {
		g_string_truncate(message->buffer, 0);
	}
	
	if (truncate_socket_buffer) {
		g_string_truncate(message->socket_buffer, 0);
	}
}

#ifdef TESTING
#include "../test/test_connections.c"
#endif