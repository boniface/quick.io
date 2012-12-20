#include "qio.h"

/**
 * The types of messages that can be sent asynchronously
 */
enum _async_message_types {
	/**
	 * A broadcast message.
	 * Requires: _message_s#event_path, _message_s#message, _message_s#message_opcode
	 */
	_mt_broadcast,
	
	/**
	 * A heartbeat message to send to all clients.
	 * Requires:
	 */
	_mt_heartbeat,
};

/**
 * A message that is waiting to be sent to user(s). It contains all the data it
 * needs to send a message, regardless of any other memory references.
 */
typedef struct {
	/**
	 * The type of message being sent
	 */
	enum _async_message_types type;
	
	/**
	 * The path of the event we're publishing to.
	 * This causes another hash table lookup, but it saves us from the following case:
	 *
	 * 1) Publish event to /test/event
	 * 2) All clients unsubscribe from /test/event
	 * 3) We attempt to publish with an invalid pointer (segfault!)
	 *
	 * @attention MUST be free()'d when done.
	 */
	gchar *event_path;
	
	/**
	 * A message buffer that each message type may abuse as it sees fit.
	 * @attention MUST be free()'d when done.
	 */
	GString *message;
	
	/**
	 * The opcode of the message.
	 * The message's opcode that will be sent to the client in the websocket message's
	 * opcode field.
	 */
	opcode_t message_opcode;
} _async_message_t;

/**
 * All of the subscriptions registered on the server.
 */
static GHashTable* _events;

/**
 * To make sure that nothing is colliding on _events. It's possible to use a RW lock
 * here, but it requires all sorts of complicated logic because you can't atomically
 * upgrade a read lock to a write lock.  So one lock it is!
 *
 * ATTENTION:
 *   Locking in this file __**MUST**__ happen in this order:
 *       Events Lock -> Subscription Lock -> Client Lock
 */
static GMutex _events_lock;

/**
 * The list of messages that we need to send in our next loop.
 */
static GAsyncQueue *_messages;

/**
 * The handler for heartbeat events.
 */
static event_handler_t *_heartbeat;

/**
 * Creates a subscription from a handler and friends.
 */
static evs_client_sub_t* _subscription_create(const gchar *event_path, event_handler_t *handler, path_extra_t *extra) {
	evs_client_sub_t *sub = g_slice_alloc0(sizeof(*sub));
	
	// Keep around a reference to the key for passing around for easier
	// persistence when the event names are freed from a message
	sub->event_path = g_strdup(event_path);
	
	// Use the default hashing functions, only using memory locations anyway
	// (*client->*client is what is being stored)
	sub->clients = g_hash_table_new(NULL, NULL);
	
	// Because we'll need this for some future references, let's keep the
	// handler around: this will also save us on future lookups for the same
	// event
	sub->handler = handler;
	
	// Save the extra elements on the path, we'll need those again later
	g_ptr_array_ref(extra);
	sub->extra = extra;
	
	g_mutex_init(&sub->lock);
	
	return sub;
}

/** 
 * Shortcut for getting a subscription, or creating it if it doesn't exist.
 *
 * @note This function ALWAYS gives the caller a lock on the subscription
 * to ensure that it won't be deleted once returned.
 */
static evs_client_sub_t* _subscription_get(const gchar *event_path, const gboolean and_create) {
	g_mutex_lock(&_events_lock);
	
	evs_client_sub_t *sub = g_hash_table_lookup(_events, event_path);
	
	if (sub == NULL && and_create) {
		// To validate this event, check the server events to make sure there
		// is a handler registered
		path_extra_t *extra;
		event_handler_t *handler = evs_server_get_handler(event_path, &extra);
		
		// If no handler was found, don't create anything
		if (handler == NULL) {
			g_mutex_unlock(&_events_lock);
			return NULL;
		}
		
		sub = _subscription_create(event_path, handler, extra);
		
		// The hash table relies on the key existing for its
		// lifetime: duplicated in _subscription_create
		g_hash_table_insert(_events, sub->event_path, sub);
	}
	
	if (sub != NULL) {
		g_mutex_lock(&sub->lock);
	}
	
	g_mutex_unlock(&_events_lock);
	
	return sub;
}

/**
 * Check and cleanup a subscription if it has no more subscribers.
 */
static void _subscription_cleanup(const gchar *event_path) {
	g_mutex_lock(&_events_lock);
	
	evs_client_sub_t *sub = g_hash_table_lookup(_events, event_path);
	
	if (sub == NULL) {
		g_mutex_unlock(&_events_lock);
		return;
	}
	
	g_mutex_lock(&sub->lock);
	
	if (g_hash_table_size(sub->clients) == 0) {
		DEBUG("Removing empty subscription: %s", sub->event_path);
		
		g_hash_table_remove(_events, event_path);
		g_mutex_unlock(&_events_lock);
		
		g_ptr_array_unref(sub->extra);
		g_hash_table_unref(sub->clients);
		
		// It's removed from the events table at this point, so it's safe
		// to do unlocked operations on it
		g_mutex_unlock(&sub->lock);
		g_mutex_clear(&sub->lock);
		
		g_slice_free1(sizeof(*sub), sub);
	} else {
		g_mutex_unlock(&_events_lock);
		g_mutex_unlock(&sub->lock);
	}
}

/**
 * Subscribes a client to an event.
 *
 * @param from_app Determines if the event is checked with the handling application
 * before being pushed through.
 *
 * @see evs_client_sub_client
 */
static status_t _evs_client_sub_client(const gchar *event_path, client_t *client, const callback_t client_callback, const gboolean from_app) {
	// Attempt to get the subscription to check if it exists
	evs_client_sub_t *sub = _subscription_get(event_path, TRUE);
	
	if (sub == NULL) {
		return CLIENT_ERROR;
	}
	
	qev_client_lock(client);
	
	if (client->subs->len >= option_max_subscriptions() || g_hash_table_contains(sub->clients, client)) {
		g_mutex_unlock(&sub->lock);
		qev_client_unlock(client);
		return CLIENT_ERROR;
	}
	
	if (!from_app) {
		status_t status = apps_evs_client_check_subscribe(client, sub->handler, sub->extra, client_callback);
		
		if (status == CLIENT_ASYNC || status != CLIENT_GOOD) {
			g_mutex_unlock(&sub->lock);
			qev_client_unlock(client);
			return status == CLIENT_ASYNC ? CLIENT_ASYNC : CLIENT_ERROR;
		}
	}
	
	DEBUG("Subscribing client to: %s", event_path);
	
	// Subscribe the client
	g_hash_table_add(sub->clients, client);
	
	g_mutex_unlock(&sub->lock);
	
	// Give the client a reference to the key of the event he is subscribed to
	g_ptr_array_add(client->subs, sub);
	
	qev_client_unlock(client);
	
	// And now, once the subscribe is complete, send the general subscribe event
	apps_evs_client_subscribe(client, sub->event_path, sub->extra);
	
	return CLIENT_GOOD;
}

/**
 * @see evs_client_format_message()
 *
 * @param path If this is not NULL, then it MUST be free()'d
 */
static status_t _evs_client_format_message(const event_handler_t *handler, const callback_t client_callback, const guint32 server_callback, path_extra_t *extra, const enum data_t type, const gchar *data, GString *buffer, gchar **path) {
	// A way of getting the path back to the caller
	gchar *final_path;
	
	// A callback takes precedence over an event handler, always
	if (client_callback != 0) {
		final_path = g_strdup_printf(F_CALLBACK_PATH, client_callback);
	} else {
		// Seriously, no handler = no message
		if (handler == NULL) {
			return CLIENT_ERROR;
		}
		
		// First, start by constructing the name of the event we're publishing to, complete
		// with all extra path elements
		gchar *handler_path = evs_server_path_from_handler(handler);
		
		if (handler_path == NULL) {
			return CLIENT_ERROR;
		}
		
		GString *ep = g_string_new(handler_path);
		
		// Skip all the loopy logic unless there are extra path segments
		if (extra != NULL) {
			for (guint i = 0; i < extra->len; i++) {
				g_string_append_printf(ep, "/%s", (gchar*)g_ptr_array_index(extra, i));
			}
		}
		
		final_path = ep->str;
		g_string_free(ep, FALSE);
	}
	
	g_string_printf(buffer, F_EVENT, final_path, server_callback, DATA_TYPE(type), data);
	
	if (path != NULL) {
		*path = final_path;
	} else {
		g_free(final_path);
	}
	
	return CLIENT_GOOD;
}

/**
 * Publish a single message to multiple clients.
 *
 * @param message The message to send to all the subscribers.
 * @param iter A function that loops through all the clients and calls the passed in
 * function on them.
 * @param heartbeat Rather than have tons of logic and anonymous functions in the handler,
 * we just zip it up here.
 */
static void _evs_client_pub_message(_async_message_t *amsg, void(*iter)(gboolean(*)(client_t*)), gboolean heartbeat) {
	// For holding all the message types we might send
	gchar *msgs[h_len];
	gsize msglen[h_len];
	
	msgs[h_rfc6455] = h_rfc6455_prepare_frame(
		amsg->message_opcode,
		FALSE,
		amsg->message->str,
		amsg->message->len,
		&msglen[h_rfc6455]
	);
	
	DEBUG("Publishing message: %s", amsg->message->str);
	
	// If any clients need a heartbeat or are going to in the next interval, just
	// give it to them
	gint64 heartbeat_time = qev_time - HEARTBEAT_INTERVAL + HEARTBEAT_TICK;
	
	gboolean _write(client_t *client) {
		enum handlers handler = client->handler;
		
		// This little bit of logic here saves on tons of anonymous functions and
		// confusing code: it feels a bit out of place, but it's far cleaner to have it
		// here than in the heartbeat anonymous functions
		if (heartbeat) {
			if (client->heartbeat >= heartbeat_time) {
				return TRUE;
			} else {
				STATS_INC(heartbeats);
			}
		}
		
		if (handler == h_none || client_write_frame(client, msgs[handler], msglen[handler]) == CLIENT_FATAL) {
			TEST_STATS_INC(evs_client_send_pub_closes);
			qev_close(client);
		}
		
		return TRUE;
	}
	
	// Go through all the clients and give them their messages
	iter(_write);
	
	// Clean up all the allocated frames
	for (int i = 0; i < h_len; i++) {
		free(msgs[i]);
	}
}

status_t evs_client_sub_client(const gchar *event_path, client_t *client, const callback_t client_callback) {
	return _evs_client_sub_client(event_path, client, client_callback, FALSE);
}

status_t evs_client_unsub_client(const gchar *event_path, client_t *client) {
	// Attempt to get the subscription to check if it exists
	evs_client_sub_t *sub = _subscription_get(event_path, FALSE);
	
	if (sub == NULL) {
		return CLIENT_ERROR;
	}
	
	// If the client isn't a member of the subscription, that's an error
	if (!g_hash_table_remove(sub->clients, client)) {
		g_mutex_unlock(&sub->lock);
		return CLIENT_ERROR;
	}
	
	g_mutex_unlock(&sub->lock);
	
	// The client isn't subscribed anymore, remove from his list
	qev_client_lock(client);
	g_ptr_array_remove_fast(client->subs, sub);
	qev_client_unlock(client);
	
	apps_evs_client_unsubscribe(client, sub->handler, sub->event_path, sub->extra);
	
	_subscription_cleanup(event_path);
	
	return CLIENT_GOOD;
}

void evs_client_client_ready(client_t *client) {
	// Initialize our internal management from the client
	client->subs = g_ptr_array_sized_new(MIN(option_max_subscriptions(), EVS_CLIENT_CLIENT_INITIAL_COUNT));
}

void evs_client_client_close(client_t *client) {
	// It's possible the client never behaved and was killed before
	// its subscriptions were setup
	if (client->subs == NULL) {
		return;
	}
	
	while (client->subs->len > 0) {
		evs_client_sub_t *sub = g_ptr_array_index(client->subs, 0);
		evs_client_unsub_client(sub->event_path, client);
	}
	
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;
}

status_t evs_client_pub_event(const event_handler_t *handler, path_extra_t *extra, const enum data_t type, const gchar *data) {
	// Format the message that will be sent to the clients
	GString *message = g_string_sized_new(100);
	gchar *event_path;
	
	// There are no callbacks for broadcast messages
	status_t status = _evs_client_format_message(handler, 0, 0, extra, type, data, message, &event_path);
	
	if (status != CLIENT_GOOD) {
		g_string_free(message, TRUE);
		return status;
	}
	
	// This one is tricky: duplicate the event path so that the following cannot happen:
	//  1) Publish event to /test/event
	//  2) All clients unsubscribe from /test/event
	//  3) We attempt to publish with an invalid pointer (segfault!)
	_async_message_t *amsg = g_slice_alloc0(sizeof(*amsg));
	amsg->type = _mt_broadcast;
	amsg->event_path = event_path;
	amsg->message = message;
	amsg->message_opcode = op_text;
	g_async_queue_push(_messages, amsg);
	
	return CLIENT_GOOD;
}

status_t evs_client_format_message(const event_handler_t *handler, const callback_t client_callback, const guint32 server_callback, path_extra_t *extra, const enum data_t type, const gchar *data, GString *buffer) {
	return _evs_client_format_message(handler, client_callback, server_callback, extra, type, data, buffer, NULL);
}

gboolean evs_client_init() {
	// Keys are copied before they are inserted, so when they're removed,
	// they must be freed
	_events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	_messages = g_async_queue_new();
	
	_heartbeat = evs_server_on("/qio/heartbeat", NULL, evs_no_subscribe, NULL, FALSE);
	
	return TRUE;
}

void evs_client_app_sub_cb(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback, const gboolean valid) {
	gchar *event_path = evs_server_path_from_handler(handler);
	
	if (event_path != NULL && valid) {
		gchar *path = evs_server_format_path(event_path, extra);
		
		if (_evs_client_sub_client(path, client, client_callback, TRUE) == CLIENT_GOOD) {
			evs_client_send_callback(client, client_callback, 0, d_plain, "");
		} else {
			evs_client_send_error_callback(client, client_callback);
		}
		
		g_free(path);
	} else {
		evs_client_send_error_callback(client, client_callback);
	}
}

void evs_client_send_callback(client_t *client, const callback_t client_callback, const callback_t server_callback, const enum data_t type, const gchar *data) {
	// Umm, that would be stupid...
	if (client_callback == 0) {
		// But they created a callback for the client...wat?
		if (server_callback != 0) {
			evs_server_callback_free(client, server_callback);
		}
		
		return;
	}
	
	GString *message = g_string_sized_new(100);
	
	// No error condition when sending a callback, and since we do the error check here,
	// there's no need to do any other error checks
	if (evs_client_format_message(NULL, client_callback, server_callback, NULL, type, data, message) == CLIENT_GOOD) {
		message_t m;
		m.type = op_text;
		m.buffer = message;
		
		TEST_STATS_INC(evs_client_async_messages);
		if (client_write(client, &m) != CLIENT_GOOD) {
			TEST_STATS_INC(evs_client_send_single_closes);
			qev_close(client);
		}
	}
	
	g_string_free(message, TRUE);
}
void evs_client_send_error_callback(client_t *client, const callback_t client_callback) {
	evs_client_send_callback(client, client_callback, 0, d_plain, QIO_ERROR);
}

void evs_client_heartbeat() {
	_async_message_t *amsg = g_slice_alloc0(sizeof(*amsg));
	amsg->type = _mt_heartbeat;
	g_async_queue_push(_messages, amsg);
}

void evs_client_tick() {
	_async_message_t *amsg;
	
	void broadcast() {
		evs_client_sub_t *sub;
		
		void iter(gboolean(*_write)(client_t*)) {
			sub = _subscription_get(amsg->event_path, FALSE);
			
			if (sub == NULL) {
				return;
			}
			
			TEST_STATS_INC(evs_client_pubd_messages);
			
			GHashTableIter iter;
			client_t *client;
			g_hash_table_iter_init(&iter, sub->clients);
			while (g_hash_table_iter_next(&iter, (gpointer)&client, NULL)) {
				STATS_INC(messages_broadcast);
				_write(client);
			}
			
			g_mutex_unlock(&sub->lock);
		}
		
		_evs_client_pub_message(amsg, iter, FALSE);
	}
	
	void heartbeat() {
		amsg->message = g_string_sized_new(100);
		amsg->message_opcode = op_text;
		_evs_client_format_message(_heartbeat, 0, 0, NULL, d_plain, "", amsg->message, NULL);
		_evs_client_pub_message(amsg, conns_clients_foreach, TRUE);
	}
	
	while ((amsg = g_async_queue_try_pop(_messages)) != NULL) {
		gint64 start = g_get_monotonic_time();
		
		switch (amsg->type) {
			case _mt_broadcast:
				broadcast();
				break;
			
			case _mt_heartbeat:
				heartbeat();
				break;
		}
		
		INFO("Run time for %s: %" G_GINT64_FORMAT, amsg->type == _mt_broadcast ? "broadcast" : "heartbeat", g_get_monotonic_time() - start);
		
		if (amsg->event_path != NULL) {
			g_free(amsg->event_path);
		}
		
		if (amsg->message != NULL) {
			g_string_free(amsg->message, TRUE);
		}
		
		g_slice_free1(sizeof(*amsg), amsg);
	}
}

guint evs_client_number_subscribed(const event_handler_t *handler, path_extra_t *extra) {
	guint cnt = 0;
	
	gchar *event_path = evs_server_path_from_handler(handler);
	gchar *ep = evs_server_format_path(event_path, extra);
	evs_client_sub_t *sub = _subscription_get(ep, FALSE);
	g_free(ep);
	
	if (sub != NULL) {
		cnt = g_hash_table_size(sub->clients);
		g_mutex_unlock(&sub->lock);
	}
	
	return cnt;
}

#ifdef TESTING
#include "../test/test_events_client.c"
#endif
