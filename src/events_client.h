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
 * Gets a string for the event type.
 */
#define DATA_TYPE(t) (t == d_plain ? "plain" : "json")

/**
 * The subscription that a user without any subscriptions will always have.
 * This is removed as soon as the user subscribes to an event, and added back
 * when the user no longer has any subscriptions.
 */
#define UNSUBSCRIBED "0"

/**
 * The initial number of subscriptions a client should have.
 *
 * This is a power of 2 because that is GPtrArray always expands
 * itself to the nearest power of 2.
 */
#define EVS_CLIENT_CLIENT_INTIAL_COUNT 4

/**
 * A published message that is waiting to be sent to users. It contains all the data it
 * needs to send a message, regardless of any other memory references.
 */
typedef struct evs_client_message_s {
	/**
	 * The name of the event.
	 * @attention MUST NOT be free()'d
	 */
	gchar *event;
	
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
	guint16 message_len;
} STRUCT_PACKED evs_client_message_t;

/**
 * The value that is stored in the hash table so that we can access
 * the pointer to the key for passing around to events.
 */
typedef struct evs_client_sub_s {
	/**
	 * The name of the event.
	 * This is a duplicated string that is referenced all over the place.
	 * @attention MUST be free()'d when done.
	 */
	gchar *event;
	
	/**
	 * The hash table (used as a set) of clients.
	 *
	 * @see http://developer.gnome.org/glib/2.30/glib-Hash-Tables.html#id915490
	 */
	GHashTable *clients;
} evs_client_sub_t;

/**
 * Setup everything the pubsub needs to run.
 */
gboolean evs_client_init();

/**
 * Add the client to UNSUBSCRIBED, waiting for any futher commands.
 *
 * @param client The client that just became ready.
 */
void evs_client_client_ready(client_t* client);

/**
 * Subscribes a client to an event.
 * 
 * @param event The name of the event the client should be subscribed to.
 * @param client The client to subscribe to event.
 *
 * @return CLIENT_INVALID_SUBSCRIPTION - The event name doesn't exist.
 * @return CLIENT_TOO_MANY_SUBSCRIPTIONS - Already subscribed to the maximum number of
 * subscriptions allowed.
 * @return CLIENT_ALREADY_SUBSCRIBED - Already subscribed to this event.
 */
status_t evs_client_sub_client(gchar *event, client_t *client);

/**
 * The client has been closed and should be cleaned up.
 * Removes client from all his subscriptions and frees any relevant memory.
 *
 * @warning NOT thread safe.
 *
 * @param client The client that should be free'd
 */
void evs_client_client_free(client_t *client);

/**
 * Removes the client from the room.
 *
 * @param event The name of the event to unsubscribe the client from.
 * @param client The client to unsubscribe from event.
 *
 * @return CLIENT_CANNOT_UNSUBSCRIBE - The client was either not subscribed to the event of the event didn't exist.  Either way, the client will not recieve notifications for this event.
 */
status_t evs_client_unsub_client(gchar *event, client_t *client);

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
 * @ingroup ModuleFunctions
 *
 * @param event The event name to publish to.
 * @param type The type of the data (json, plain, etc).
 * @param data The actual data to be sent.
 *
 * @attention IS thread safe.
 */
MODULE_EXPORT status_t evs_client_pub_event(const gchar *event, const enum data_t type, const gchar *data);

/**
 * A cleanup routine for empty events.
 *
 * @warning NOT thread safe.
 */
void evs_client_cleanup();