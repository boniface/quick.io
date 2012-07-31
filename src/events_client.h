#pragma once
#include "qio.h"

#define UNSUBSCRIBED "0"

/**
 * The initial number of subscriptions a client should have.
 *
 * This is a power of 2 because that is GPtrArray always expands
 * itself to the nearest power of 2.
 */
#define EVS_CLIENT_CLIENT_INTIAL_COUNT 4

/**
 * A publishable message. It contains all the data it needs to send a message,
 * regardless of any other memory references.  All of this should be free()'d
 * when done.
 */
struct evs_client_message_s {
	// The name of the event
	gchar *event;
	
	// The opcode of the message
	opcode_t type;
	
	// The un-prepared data to send to the client
	gchar *message;
	
	// The length of the data to send
	guint16 message_len;
} __attribute__((__packed__));

typedef struct evs_client_message_s evs_client_message_t;

/**
 * The value that is stored in the hash table so that we can access
 * the pointer to the key for passing around to events.
 */
typedef struct evs_client_sub_s {
	// The name of the event
	gchar *event;
	
	// The hash table (used as a set) of clients
	GHashTable *clients;
} evs_client_sub_t;

/**
 * Setup everything the pubsub needs to run.
 */
gboolean evs_client_init(void);

/**
 * Add the client to UNSUBSCRIBED, waiting for any futher commands.
 */
void evs_client_client_ready(client_t*);

/**
 * Subscribes a client to an event.
 */
status_t evs_client_sub_client(gchar*, client_t*);

/**
 * The client has been closed. Remove him from all his subscriptions and free
 * any relevant memory.
 *
 * THIS SHOULD NEVER BE CALLED FROM ANYTHING BUT THE MAIN THREAD. IT IS NOT
 * THREAD SAFE.
 */
void evs_client_client_free(client_t*);

/**
 * Removes the client from the room.
 */
status_t evs_client_unsub_client(gchar*, client_t*);

/**
 * Publish the entire message queue immediately.
 *
 * This MUST NEVER be called from anything but the main thread.
 */
void evs_client_pub_messages(void);

/**
 * Send a message to everyone subscribed to the event. This just adds to the list of
 * messages to send, and the message is actually sent in the next loop of the main
 * thread.
 *
 * This function IS thread safe.
 */
status_t evs_client_pub_message(gchar*, message_t*);

/**
 * A cleanup routine for dead rooms and the like.
 *
 * This function IS NOT threadsafe.
 */
void evs_client_cleanup(void);