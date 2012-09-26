/**
 * @file events_client.h
 * Handles managing which clients are subscribed to which events.
 * When a client calls QuickIo#on, they are subscribed on the server side,
 * and all of the subscriptions are managed here.
 */

#pragma once
#include "qio.h"

/**
 * The message format for events being sent to a client.
 */
#define F_EVENT "%s" EVENT_DELIMITER "%" G_GUINT32_FORMAT EVENT_DELIMITER "%s=%s"

/**
 * The message format for a callback.
 */
#define F_CALLBACK_PATH "/callback/%" G_GUINT32_FORMAT

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
#define EVS_CLIENT_CLIENT_INTIAL_COUNT 4

/**
 * The value that is stored in the hash table so that we can access
 * the pointer to the key for passing around to events.
 */
struct evs_client_sub_s {
	/**
	 * The name of the event.
	 * This is a duplicated string that is referenced all over the place.
	 * @attention MUST be free()'d when done.
	 */
	gchar *event_path;
	
	/**
	 * The extra path segments that are on the end of the path.
	 *
	 * @attention MUST be g_list_free()'d when done.
	 */
	path_extra_t extra;
	
	/**
	 * The length of the extra path segments.
	 */
	guint extra_len;
	
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
};

/**
 * A published message that is waiting to be sent to users. It contains all the data it
 * needs to send a message, regardless of any other memory references.
 *
 * @attention This is an INTERNAL data structure that SHOULD NOT be used outside of
 * events_client
 */
typedef struct evs_client_message_s {
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
	 * The opcode of the message.
	 * The message's opcode that will be sent to the client in the websocket message's
	 * opcode field.
	 */
	opcode_t type;
	
	/**
	 * The un-prepared data to send to the client.
	 * @attention MUST be free()'d when done.
	 */
	gchar *message;
	
	/**
	 * The length of #message
	 */
	guint64 message_len;
} STRUCT_PACKED evs_client_message_t;

/**
 * Setup everything the pubsub needs to run.
 */
gboolean evs_client_init();

/**
 * Set up internal structures for the client.
 *
 * @param client The client that just became ready.
 */
void evs_client_client_ready(client_t* client);

/**
 * Subscribes a client to an event.
 * 
 * @param event_path The name of the event the client should be subscribed to.
 * @param client The client to subscribe to event.
 *
 * @return CLIENT_INVALID_SUBSCRIPTION - The event name doesn't exist.
 * @return CLIENT_TOO_MANY_SUBSCRIPTIONS - Already subscribed to the maximum number of
 * subscriptions allowed.
 * @return CLIENT_ALREADY_SUBSCRIBED - Already subscribed to this event.
 */
status_t evs_client_sub_client(const gchar *event_path, client_t *client);

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
 * @return CLIENT_CANNOT_UNSUBSCRIBE - The client was either not subscribed to the event of the event didn't exist.  Either way, the client will not recieve notifications for this event.
 */
status_t evs_client_unsub_client(const gchar *event_path, client_t *client);

/**
 * Publish the entire message queue immediately.
 *
 * @warning NOT thread safe.
 */
void evs_client_pub_messages();

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
 * @param data The actual data to be sent. NULL if none.
 *
 * @attention IS thread safe.
 */
APP_EXPORT status_t evs_client_pub_event(const event_handler_t *handler, const path_extra_t extra, const enum data_t type, const gchar *data);

/**
 * Prepare a message to be sent to 1 individual user.
 *
 * @param handler The event handler; if NULL, will use the callback.
 * @param callback The callback to send to the user; 0 if no callback.
 * @param server_callback What should be sent to the client, indicating the server wants a callback. 0 if no callback is wanted.
 * @param extra Any extra path parameters to put onto the path of the event. NULL if none.
 * @param type The type of the data (json, plain, etc).  d_plain if none.
 * @param data The actual data to be sent. NULL if none.
 * @param buffer Where the formatted message will be written. MUST NOT be NULL.
 *
 * @return CLIENT_GOOD - If the message was prepared and formatted successfully.
 * @return CLIENT_UNKNOWN_EVENT - If the event could not be found, or no callback was given.
 */
APP_EXPORT status_t evs_client_format_message(const event_handler_t *handler, const guint32 callback, const guint32 server_callback, const path_extra_t extra, const enum data_t type, const gchar *data, GString *buffer);

/**
 * A cleanup routine for empty events.
 *
 * @warning NOT thread safe.
 */
void evs_client_cleanup();


#ifdef TESTING
#include "../test/test_events_client.h"
#endif
