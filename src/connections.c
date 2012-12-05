#include "qio.h"

/**
 * All of the clients currently connected to the server.
 */
static GPtrArray *_clients;

/**
 * The next available position in _clients.
 * Though there is a len in the array, incrementing this before adding the client
 * to the array leaves a race condition that could result in a segfault. So we
 * increment outside of the array, then add to the array, then increment the
 * array length.
 */
static guint _clients_len = 0;

/**
 * In order to make make looping through clients possible while still accepting
 * clients into the list, we're going to ABUSE (indeed, almost to death) a RW lock.
 * Rather than representing RW, we're going to represent two things:
 *   R = Safe to append to
 *   W = Complete lock on the data structure
 *
 * Why is nothing le thread safe? :(
 */
static qev_lock_t _clients_lock;

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
		if (--client->timer == 0) {
			TEST_STATS_INC(conns_timeouts);
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
	qev_lock_write_lock(&_clients_lock);
	
	// Clients are only ever removed from 1 thread, so we don't have to be atomic
	guint pos = client->clients_pos;
	
	if (pos > 0) {
		g_ptr_array_remove_index_fast(_clients, pos - 1);
		__sync_fetch_and_sub(&_clients_len, 1);
		
		#if defined(TESTING) || defined(DEBUG)
			// Cause segfaults while testing if we get stuff wrong
			_clients->pdata[_clients->len] = NULL;
		#endif
		
		// If the array isn't empty and we're not the last element (neither case
		// is safe to write back to)
		if (_clients->len >= pos) {
			((client_t*)g_ptr_array_index(_clients, pos - 1))->clients_pos = pos;
		}
		
		client->clients_pos = 0;
	}
	
	qev_lock_write_unlock(&_clients_lock);
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
			qev_close(client);
			
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
	qev_lock_read_lock(&_clients_lock);
	guint pos = __sync_add_and_fetch(&_clients_len, 1);
	
	if (pos > option_max_clients()) {
		WARN("More clients than `max-clients` trying to connect");
		
		qev_close(client);
		__sync_sub_and_fetch(&_clients_len, 1);
		
		qev_lock_read_unlock(&_clients_lock);
		return;
	}
	
	client->clients_pos = pos;
	_clients->pdata[client->clients_pos - 1] = client;
	g_atomic_int_inc(&_clients->len);
	qev_lock_read_unlock(&_clients_lock);
	
	client->state = cstate_initing;
	client->timer = -1;
	client_ref(client);
	
	apps_client_connect(client);
	conns_client_timeout_set(client);
	
	STATS_INC(clients_new);
	STATS_INC(clients);
}

void conns_client_killed(client_t *client) {
	client->state = cstate_dead;
}

void conns_client_close(client_t *client) {
	DEBUG("A client closed: %p", &client->qevclient);
	
	_conns_clients_remove(client);
	
	conns_client_timeout_clear(client);
	
	// The message can be safely freed here: it's only used here,
	// and we're definitely not reading/writing to it asynchronously
	conns_message_free(client);
	
	client_unref(client);
	
	STATS_INC(clients_closed);
	STATS_DEC(clients);
}

void conns_client_free(client_t *client) {
	apps_client_close(client);
	evs_client_client_close(client);
	evs_server_client_close(client);
	g_slice_free1(sizeof(*client), client);
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
		g_string_append_len(client->message->socket_buffer, buffer, len);
		
		// If the client needs to enhance his calm, kill the connection.
		if (client->message->socket_buffer->len > (option_max_message_size() * MAX_BUFFER_SIZE_MULTIPLIER)) {
			TEST_STATS_INC(conns_bad_clients);
			STATS_INC(clients_ratelimited);
			
			DEBUG("Client needs to enhance his calm");
			return FALSE;
		}
	}
	
	// While there is still something on the socket buffer to process
	while (client->message->socket_buffer->len > 0) {
		status_t status;
		
		if (client->state == cstate_initing) {
			TEST_STATS_INC(conns_handshakes);
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
			TEST_STATS_INC(conns_messages);
			
			DEBUG("Message from client");
			status = client_message(client);
			
			if (status == CLIENT_WRITE && (status = client_write(client, NULL)) != CLIENT_GOOD) {
				status = CLIENT_FATAL;
			}
		}
		
		if (status == CLIENT_GOOD) {
			conns_client_timeout_clear(client);
			
			// Clean up the message buffer, since we just finished processing him
			conns_message_clean(client, FALSE, TRUE);
		

		} else if (status == CLIENT_WAIT) {
			TEST_STATS_INC(conns_client_wait);
			
			conns_client_timeout_set(client);
			
			// The buffer will still be set, we're waiting, so just exit
			break;
			
		} else if (status == CLIENT_FATAL) {
			TEST_STATS_INC(conns_bad_clients);
			
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
	_clients = g_ptr_array_sized_new(option_max_clients());
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
	_conns_client_timeout_clean();
	_conns_balance();
}

void conns_clients_foreach(gboolean(*_callback)(client_t*)) {
	guint i = g_atomic_int_get(&_clients->len);
	while (i > 0) {
		qev_lock_read_lock(&_clients_lock);
		
		guint len = g_atomic_int_get(&_clients->len);
		
		if (len == 0) {
			qev_lock_read_unlock(&_clients_lock);
			break;
		}
		
		if (--i > len) {
			i = len - 1;
		}
		
		client_t *client = g_ptr_array_index(_clients, i);
		
		qev_lock_read_unlock(&_clients_lock);
		
		if (client->state == cstate_running && !_callback(client)) {
			break;
		}
	}
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
