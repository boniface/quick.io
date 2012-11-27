#include "qio.h"

/**
 * All of the clients currently connected to the server.
 */
static GPtrArray *_clients;

/**
 * Hash tables no le thread safe :(
 */
static GMutex _clients_lock;

/**
 * All of the clients that have timeouts set on them.
 */
static GHashTable *_client_timeouts;

/**
 * We can only touch the timeouts from 1 thread
 */
static GMutex _client_timeouts_lock;

/**
 * A list of all the balance requests.
 */
static GAsyncQueue *_balances;

/**
 * The handler for balance events.
 */
static event_handler_t *_balance_handler;

/**
 * Create a new message struct in the clien
 */
static void _conns_message_new(client_t *client) {
	message_t *message = client->message;
	
	// Replace any slots in the message that might have been freed
	if (message == NULL) {
		message = g_slice_alloc0(sizeof(*message));
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
	client_t *client;
	GHashTableIter iter;
	
	g_mutex_lock(&_client_timeouts_lock);
	
	g_hash_table_iter_init(&iter, _client_timeouts);
	while (g_hash_table_iter_next(&iter, (void*)&client, NULL)) {
		client->timer--;
		if (client->timer == -1) {
			UTILS_STATS_INC(conns_timeouts);
			STATS_INC(client_timeouts);
			
			DEBUG("Timer on client expired: %p", &client->qevclient);
			qev_close(client);
			g_hash_table_iter_remove(&iter);
		}
	}
	
	g_mutex_unlock(&_client_timeouts_lock);
}

/**
 * Assumes you have a lock on _clients: removes the given client and updates
 * the client that replaces him
 */
static void _conns_clients_remove(client_t *client) {
	if (client->clients_pos > 0) {
		g_ptr_array_remove_index_fast(_clients, client->clients_pos - 1);
		
		if (_clients->len > 0) {
			((client_t*)g_ptr_array_index(_clients, client->clients_pos - 1))->clients_pos = client->clients_pos;
		}
		
		client->clients_pos = 0;
	}
}

/**
 * Go through any balance requests and fulfill them.
 */
static void _conns_balance() {
	// Stop any unnecessary allocation of memory
	if (g_async_queue_length(_balances) == 0) {
		return;
	}
	
	message_t message;
	message.type = op_text;
	message.buffer = g_string_sized_new(100);
	
	conns_balance_request_t *req;
	while ((req = g_async_queue_try_pop(_balances)) != NULL) {
		evs_client_format_message(_balance_handler, 0, 0, NULL, d_plain, req->to, message.buffer);
		
		DEBUG("Balancing: %d to %s", req->count, req->to);
		
		guint cnt = 0;
		
		gboolean _callback(client_t *client) {
			if (cnt++ >= req->count) {
				return FALSE;
			}
			
			if (message.buffer->len > 0) {
				client_write(client, &message);
			}
			
			STATS_INC(clients_balanced);
			_conns_clients_remove(client);
			
			return TRUE;
		}
		
		conns_clients_foreach(_callback);
		g_free(req->to);
		g_slice_free1(sizeof(*req), req);
	}
	
	g_string_free(message.buffer, TRUE);
}

void conns_balance(guint count, gchar *to) {
	conns_balance_request_t *req = g_slice_alloc0(sizeof(*req));
	req->count = count;
	req->to = g_strdup(to);
	
	g_async_queue_push(_balances, req);
}

void conns_client_new(client_t *client) {
	// Basic information about this client
	client->state = cstate_initing;
	client->timer = -1;
	
	g_mutex_lock(&_clients_lock);
	g_ptr_array_add(_clients, client);
	client->clients_pos = _clients->len;
	g_mutex_unlock(&_clients_lock);
	
	client_ref(client);
	
	apps_client_connect(client);
	conns_client_timeout_set(client);
	
	STATS_INC(clients_new);
	STATS_INC(clients);
}

void conns_client_close(client_t *client) {
	DEBUG("A client closed: %p", &client->qevclient);
	
	client->state = cstate_dead;
	conns_client_timeout_clear(client);
	conns_message_free(client);
	apps_client_close(client);
	evs_client_client_close(client);
	evs_server_client_close(client);
	
	g_mutex_lock(&_clients_lock);
	_conns_clients_remove(client);
	g_mutex_unlock(&_clients_lock);
	
	client_unref(client);
	
	STATS_INC(clients_closed);
	STATS_DEC(clients);
}

gboolean conns_client_data(client_t *client) {
	// Where data from the client's socket will be stored
	gchar buffer[option_max_message_size()];
	
	// Clients typically aren't sending messages
	_conns_message_new(client);
	
	// Read the message the client sent, unless it's too large,
	// then kill the client
	gssize len;
	while ((len = qev_read(client, buffer, sizeof(buffer))) > 0) {
		// Put the buffer into our string
		g_string_append_len(client->message->socket_buffer, buffer, len);
		
		// If the client needs to enhance his calm, kill the connection.
		if (client->message->socket_buffer->len > (option_max_message_size() * MAX_BUFFER_SIZE_MULTIPLIER)) {
			UTILS_STATS_INC(conns_bad_clients);
			STATS_INC(clients_ratelimited);
			
			DEBUG("Client needs to enhance his calm");
			return FALSE;
		}
	}
	
	// While there is still something on the socket buffer to process
	while (client->message->socket_buffer->len > 0) {
		status_t status;
		
		if (client->state == cstate_initing) {
			UTILS_STATS_INC(conns_handshakes);
			DEBUG("Client handshake");
			status = client_handshake(client);
			
			// Headers are sent without encoding, don't use the client wrapper
			if (status & CLIENT_WRITE) {
				STATS_INC(client_handshakes);
				
				status = client_write_frame(client, client->message->buffer->str, client->message->buffer->len);
				
				// The handshake is complete, we're done here.
				client->state = cstate_running;
				evs_client_client_ready(client);
			}
		} else {
			UTILS_STATS_INC(conns_messages);
			
			DEBUG("Message from client");
			status = client_message(client);
			
			// Send back a framed response for the websocket
			if (status == CLIENT_WRITE && (status = client_write(client, NULL)) != CLIENT_GOOD) {
				status = CLIENT_FATAL;
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
		} else if (status == CLIENT_FATAL) {
			UTILS_STATS_INC(conns_bad_clients);
			
			DEBUG("Bad client, closing: status=%d", status);
			qev_close(client);
			client = NULL;
			
			break;
		}
	}
	
	if (client != NULL && client->message->socket_buffer->len == 0 && client->message->remaining_length == 0) {
		conns_message_free(client);
	}
	
	return TRUE;
}

gboolean conns_init() {
	_clients = g_ptr_array_new();
	_client_timeouts = g_hash_table_new(NULL, NULL);
	_balance_handler = evs_server_on("/qio/move", NULL, NULL, NULL, FALSE);
	_balances = g_async_queue_new();
	
	return TRUE;
}

void conns_client_timeout_clear(client_t *client) {
	if (client->timer != -1) {
		client->timer = -1;
		
		g_mutex_lock(&_client_timeouts_lock);
		g_hash_table_remove(_client_timeouts, client);
		g_mutex_unlock(&_client_timeouts_lock);
	}
}

void conns_client_timeout_set(client_t *client) {
	// Don't ever update a timer on a client
	if (client->timer >= 0) {
		return;
	}
	
	client->timer = option_timeout();
	
	g_mutex_lock(&_client_timeouts_lock);
	g_hash_table_add(_client_timeouts, client);
	g_mutex_unlock(&_client_timeouts_lock);
}

void conns_maintenance_tick() {
	static guint ticks = 0;
	
	// Send out everything that couldn't be done synchronously
	evs_client_send_async_messages();
	
	// Check if there are any outstanding rebalance requests
	_conns_balance();
	
	if (ticks++ % CONNS_MAINTENANCE_TIMEOUTS == 0) {
		_conns_client_timeout_clean();
	}
}

void conns_clients_foreach(gboolean(*_callback)(client_t*)) {
	// For keeping track of the number of clients balanced so that we can yield appropriately
	guint total = 0;
	
	g_mutex_lock(&_clients_lock);
	guint i = _clients->len;
	while (i > 0) {
		if (!_callback(g_ptr_array_index(_clients, i - 1))) {
			break;
		}
		
		if (++total == CONNS_YIELD) {
			total = 0;
			g_mutex_unlock(&_clients_lock);
			g_thread_yield();
			g_mutex_lock(&_clients_lock);
		}
		
		if (--i > _clients->len) {
			i = _clients->len;
		}
	}
	
	g_mutex_unlock(&_clients_lock);
}

void conns_message_free(client_t *client) {
	// If the client doesn't have a message, don't be stupid
	if (client->message == NULL) {
		return;
	}
	
	g_string_free(client->message->buffer, TRUE);
	g_string_free(client->message->socket_buffer, TRUE);
	g_slice_free1(sizeof(*(client->message)), client->message);
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
	
	if (truncate_socket_buffer) {
		g_string_truncate(message->socket_buffer, 0);
	}
	
	if (truncate_buffer) {
		g_string_truncate(message->buffer, 0);
	}
}

#ifdef TESTING
#include "../test/test_connections.c"
#endif