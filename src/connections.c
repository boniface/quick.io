#include "qio.h"

/**
 * All of the clients that have timeouts set on them.
 */
static GHashTable *_client_timeouts;

/**
 * The number of ticks a client is allowed to be waited on.
 */
static gint32 _client_timeout_ticks;

/**
 * All of the clients currently connected.
 * Functions as a set.
 */
static GHashTable *_clients;

/**
 * A list of all the balance requests.
 */
static GArray *_balances;

/**
 * The handler for balance events.
 */
static event_handler_t *_balance_handler;

/**
 * For locking the _balances array for incoming balance requests from different threads.
 */
static GMutex _balances_lock;

/**
 * Create a new message struct in the client
 */
static void _conns_message_new(client_t *client) {
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

static void _conns_client_timeout_clean() {
	client_t *client, *ignored;
	GHashTableIter iter;
	g_hash_table_iter_init(&iter, _client_timeouts);
	while (g_hash_table_iter_next(&iter, (void*)&client, (void*)&ignored)) {
		client->timer--;
		if (client->timer == -1) {
			UTILS_STATS_INC(conns_timeouts);
			
			DEBUGF("Timer on client expired: %d", client->socket);
			conns_client_close(client);
			g_hash_table_iter_remove(&iter);
		}
	}
}

static void _new_balances() {
	g_mutex_lock(&_balances_lock);
	_balances = g_array_new(FALSE, FALSE, sizeof(conns_balance_request_t));
	g_mutex_unlock(&_balances_lock);
}

/**
 * Go through any balance requests and fulfill them.
 */
static void _conns_balance() {
	if (_balances->len == 0) {
		return;
	}
	
	GArray *reqs = _balances;
	_new_balances();
	
	GHashTableIter iter;
	client_t *client;
	g_hash_table_iter_init(&iter, _clients);
	
	message_t message;
	message.type = op_text;
	message.buffer = g_string_sized_new(100);
	
	guint i = reqs->len;
	while (i-- > 0) {
		conns_balance_request_t req = g_array_index(reqs, conns_balance_request_t, i);
		evs_client_format_message(_balance_handler, 0, 0, NULL, d_plain, req.to, message.buffer);
		
		DEBUGF("Balancing: %d to %s", req.count, req.to);
		
		guint cnt = 0;
		while (cnt++ < req.count && g_hash_table_iter_next(&iter, (gpointer)&client, (gpointer)&client)) {
			if (message.buffer->len > 0) {
				client_write(client, &message);
			}
			g_hash_table_iter_remove(&iter);
			conns_client_close(client);
		}
		
		g_free(req.to);
	}
	
	g_string_free(message.buffer, TRUE);
	g_array_free(reqs, TRUE);
}

void conns_balance(guint count, gchar *to) {
	conns_balance_request_t req;
	req.count = count;
	req.to = g_strdup(to);
	
	g_mutex_lock(&_balances_lock);
	g_array_append_val(_balances, req);
	g_mutex_unlock(&_balances_lock);
}

void conns_client_new(client_t *client) {
	// Basic information about this client
	client->state = cstate_initing;
	client->timer = -1;
	
	g_hash_table_insert(_clients, client, client);
	client_ref(client);
	
	apps_client_connect(client);
	conns_client_timeout_set(client);
}

void conns_client_close(client_t *client) {
	DEBUGF("A client closed: %d", client->socket);
	
	client->state = cstate_dead;
	conns_client_timeout_clear(client);
	conns_message_free(client);
	apps_client_close(client);
	evs_client_client_close(client);
	qsys_close(client);
	
	// Remove our last reference, this MUST be done last so that any free operation
	// doesn't interfere with the setting
	g_hash_table_remove(_clients, client);
	client_unref(client);
}

void conns_client_hup(client_t *client) {
	UTILS_STATS_INC(conns_hups);
		
	DEBUGF("Client HUP: %d", client->socket);
	
	// The underlying socket was closed
	conns_client_close(client);
}

void conns_client_data(client_t *client) {
	// Where data from the client's socket will be stored
	gchar buffer[option_max_message_size()];
	
	// Clients typically aren't sending messages
	_conns_message_new(client);
	
	// Read the message the client sent, unless it's too large,
	// then kill the client
	gssize len;
	while ((len = qsys_read(client, buffer, sizeof(buffer))) > 0) {
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
		if (client->state == cstate_initing) {
			DEBUG("Client handshake");
			status = client_handshake(client);
			
			// Headers are sent without encoding, don't use the client wrapper
			if (status & CLIENT_WRITE) {
				status = client_write_frame(client, client->message->buffer->str, client->message->buffer->len);
				
				// The handshake is complete, we're done here.
				client->state = cstate_running;
				
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
			conns_client_timeout_clear(client);
			
			// Clean up the message buffer, since we just finished processing him
			conns_message_clean(client, FALSE, TRUE);
		
		// The client gets 1 timer to make itself behave. If it doesn't in this
		// time, then we summarily kill it.
		} else if (status == CLIENT_WAIT) {
			UTILS_STATS_INC(conns_client_wait);
			
			conns_client_timeout_set(client);
			
			// The buffer will still be set, we're waiting, so just exit
			break;
			
		// The client is misbehaving. Close him.
		} else if (status & CLIENT_BAD) {
			UTILS_STATS_INC(conns_bad_clients);
			
			DEBUGF("Bad client, closing: status=%d", status);
			conns_client_close(client);
			client = NULL;
			
			break;
		}
	}
	
	if (client != NULL && client->message->socket_buffer->len == 0 && client->message->remaining_length == 0) {
		conns_message_free(client);
	}
}

gboolean conns_init() {
	// Just hashing clients directly, as a set
	_client_timeouts = g_hash_table_new(NULL, NULL);
	_client_timeout_ticks = option_timeout();
	_clients = g_hash_table_new(NULL, NULL);
	_balance_handler = evs_server_on("/qio/move", NULL, NULL, NULL, FALSE);
	
	_new_balances();
	
	return TRUE;
}

void conns_client_timeout_clear(client_t *client) {
	if (client->timer != -1) {
		client->timer = -1;
		g_hash_table_remove(_client_timeouts, client);
	}
}

void conns_client_timeout_set(client_t *client) {
	// Don't ever update a timer on a client
	if (client->timer >= 0) {
		return;
	}
	
	client->timer = _client_timeout_ticks;
	g_hash_table_insert(_client_timeouts, client, client);
}

void conns_maintenance_tick() {
	static guint ticks = 0;
	
	// Send out messages
	evs_client_send_messages();
	
	// Check if there are any outstanding rebalance requests
	_conns_balance();
	
	if (ticks++ % CONNS_MAINTENANCE_CLEANUP == 0) {
		// Clean up any client subscriptions
		evs_client_cleanup();
		
		// Run through all the clients in CLIENT_WAIT and clean up the bad ones
		_conns_client_timeout_clean();
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
		return;
	}
	
	message->remaining_length = 0;
	message->type = 0;
	message->mask = 0;
	
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
