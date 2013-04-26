/**
 * @file events_client.h
 * Handles managing which clients are subscribed to which events.
 * When a client calls QuickIo#on, they are subscribed on the server side,
 * and all of the subscriptions are managed here.
 */
#ifndef QIO_EVENTS_CLIENT_H
#define QIO_EVENTS_CLIENT_H

#include "qio.h"

/**
 * The message format for events being sent to a client.
 */
#define F_EVENT "%s" EVENT_DELIMITER "%" G_GUINT32_FORMAT EVENT_DELIMITER "%s" EVENT_DATA_DELIMITER "%s"

/**
 * The message format for a callback.
 */
#define F_CALLBACK_PATH "/qio/callback/%" G_GUINT32_FORMAT

/**
 * Gets a string for the event type.
 */
#define DATA_TYPE(t) (t == d_plain ? "plain" : "json")

/**
 * The initial number of subscriptions a client should have.
 *
 * This is a power of 2 because that is GPtrArray always expands
 * itself to the nearest power of 2.
 */
#define EVS_CLIENT_CLIENT_INITIAL_COUNT 4

/**
 * The time between heartbeat ticks.
 */
#define HEARTBEAT_TICK 10

/**
 * The amount of time between heartbeats to a client
 */
#define HEARTBEAT_INTERVAL 60

/**
 * The amount of time between challenge heartbeats for inactive clients.
 */
#define HEARTBEAT_INACTIVE (60*15)

/**
 * The amount of time a client is given to respond to the heartbeat challenge before being killed
 */
#define HEARTBEAT_INACTIVE_DEAD (HEARTBEAT_INACTIVE + (60*3))

/**
 * The value that is stored in the hash table so that we can access
 * the pointer to the key for passing around to events.
 */
struct evs_client_sub_s {
	/**
	 * The name of the event.
	 * This is a duplicated string that is referenced all over the place.
	 * @attention This is free'd when g_hash_table_remove is called.
	 */
	gchar *event_path;

	/**
	 * The extra path segments that are on the end of the path.
	 *
	 * @attention MUST be g_ptr_array_free()'d when done.
	 */
	path_extra_t *extra;

	/**
	 * The hash table (used as a set) of clients.
	 *
	 * @see http://developer.gnome.org/glib/2.30/glib-Hash-Tables.html#id915490
	 */
	GHashTable *clients;

	/**
	 * The handler for this event. Keep this around to save us on lookups to
	 * evs_server_get_handler().
	 */
	event_handler_t *handler;

	/**
	 * For protecting writes to the client list in this subscription
	 */
	GRWLock lock;

	/**
	 * The number of existing references to the subscription
	 */
	int ref_count;
};

/**
 * Setup everything the pubsub needs to run.
 */
gboolean evs_client_init();

/**
 * Subscribes a client to an event.
 *
 * @param event_path The name of the event the client should be subscribed to.
 * @param client The client to subscribe to event.
 * @param client_callback The callback to be issued if anything goes async.
 *
 * @return CLIENT_GOOD Everything went well.
 * @return CLIENT_ASYNC The app has gone async with the request.
 * @return CLIENT_ERROR Subscribe miserably failed.
 */
status_t evs_client_sub_client(const gchar *event_path, client_t *client, const callback_t client_callback);

/**
 * Set up internal structures for the client.
 *
 * @param client The client that just became ready.
 */
void evs_client_client_ready(client_t* client);

/**
 * The client has been closed and should be cleaned up.
 * Removes client from all his subscriptions and frees any relevant memory.
 *
 * @warning NOT thread safe.
 *
 * @param client The client that should be free'd
 */
void evs_client_client_close(client_t *client);

/**
 * Removes the client from the room.
 *
 * @param event_path The name of the event to unsubscribe the client from.
 * @param client The client to unsubscribe from event.
 *
 * @return CLIENT_CANNOT_UNSUBSCRIBE - The client was either not subscribed to the event of
 * the event didn't exist.  Either way, the client will not recieve notifications for this event.
 */
status_t evs_client_unsub_client(const gchar *event_path, client_t *client);

/**
 * Publish the entire message queue immediately.
 *
 * @warning NOT thread safe.
 */
void evs_client_send_async_messages();

/**
 * Gets the number of people currently subscribed to a specific event.
 *
 * @ingroup AppFunctions
 *
 * @param handler The handler to check subscriptions for.
 * @param extra Any extra parameters to add to the path.
 *
 * @return The number of people subscribed; 0 if event does not exist.
 */
APP_EXPORT guint evs_client_number_subscribed(const event_handler_t *handler, path_extra_t *extra);

/**
 * Send a message to everyone subscribed to the event. This just adds to the list of
 * messages to send, and the message is actually sent in the next loop of the main
 * thread.
 *
 * @ingroup AppFunctions
 *
 * @param handler The event handler to publish to.
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param type The type of the data (json, plain, etc). d_plain if none.
 * @param data The actual data to be sent. NULL/"" if none.
 *
 * @attention IS thread safe.
 */
APP_EXPORT status_t evs_client_pub_event(const event_handler_t *handler, path_extra_t *extra, const enum data_t type, const gchar *data);

/**
 * Send a single event to the given client.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to send the event to.
 * @param handler The event handler of the event being sent
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param server_callback The callback we're expecting on the server.
 * @param type The type of the data (json, plain, etc). d_plain if none.
 * @param data The actual data to be sent. NULL if none.
 *
 * @attention IS thread safe.
 */
APP_EXPORT status_t evs_client_send(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t server_callback, const enum data_t type, const gchar *data);

/**
 * Prepare a message to be sent to 1 individual user.
 *
 * @param handler The event handler; if NULL, will use the callback.
 * @param client_callback The callback to send to the user; 0 if no callback.
 * @param server_callback What should be sent to the client, indicating the server
 * wants a callback. 0 if no callback is wanted.
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param type The type of the data (json, plain, etc).  d_plain if none.
 * @param data The actual data to be sent. "" (empty string) or NULL if none.
 * @param buffer Where the formatted message will be written. MUST NOT be NULL. All contents
 * will be cleared to make room for the new data.
 *
 * @return CLIENT_GOOD If the message was prepared and formatted successfully.
 * @return CLIENT_ERROR If anything went wrong.
 */
status_t evs_client_format_message(const event_handler_t *handler, const callback_t client_callback, const callback_t server_callback, path_extra_t *extra, const enum data_t type, const gchar *data, GString *buffer);

/**
 * If and application went CLIENT_ASYNC during an on_subscribe callback, this function MUST
 * be called to fire off the callback and subscribe (or not) the user to the event.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to act on
 * @param handler The handler for the event.
 * @param extra Any extra segments to add to the event path
 * @param client_callback The id of the callback the client sent with the event.
 * @param valid If the susbcription is valid; determines if the client will be subscribed
 * or rejected.
 */
APP_EXPORT void evs_client_app_sub_cb(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback, const gboolean valid);

/**
 * Send a callback to a user. This is mainly used for async events that need to send stuff back.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to send the callback to
 * @param client_callback The id of the callback the client is expecting
 * @param server_callback The callback to be sent back to the server
 * @param type The data type
 * @param data The data to send with the callback
 */
APP_EXPORT void evs_client_send_callback(client_t *client, const callback_t client_callback, const callback_t server_callback, const enum data_t type, const gchar *data);

/**
 * Sends an error callback to the user.
 *
 * @ingroup AppFunctions
 *
 * @param client The client to send the callback to
 * @param client_callback The id of the callback the client is expecting
 */
APP_EXPORT void evs_client_send_error_callback(client_t *client, const callback_t client_callback);

/**
 * Send a heartbeat out to all users subscribed to the heartbeat event.
 */
void evs_client_heartbeat();

/**
 * Runs on every tick of QEV: for sending out async messages.
 */
void evs_client_tick();

/**
 * A cleanup routine for empty events.
 *
 * @warning NOT thread safe.
 */
void evs_client_cleanup();

#ifdef TESTING
#include "../test/test_events_client.h"
#endif

#endif
