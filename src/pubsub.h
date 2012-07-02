#pragma once
#include <glib.h>

#include "socket.h"

#define UNSUBSCRIBED "0"

/**
 * The initial number of subscriptions a client should have.
 *
 * This is a power of 2 because that is GPtrArray always expands
 * itself to the nearest power of 2.
 */
#define PUBSUB_CLIENT_INTIAL_COUNT 4

/**
 * Setup everything the pubsub needs to run.
 */
gboolean pubsub_init(void);

/**
 * Add the client to UNSUBSCRIBED, waiting for any futher commands.
 */
void sub_client_ready(client_t*);

/**
 * Subscribes a client to an event.
 */
status_t sub_client(gchar*, client_t*);

/**
 * The client has been closed. Remove him from all his subscriptions and free
 * any relevant memory.
 *
 * THIS SHOULD NEVER BE CALLED FROM ANYTHING BUT THE MAIN THREAD. IT IS NOT
 * THREAD SAFE.
 */
void sub_client_free(client_t*);

/**
 * Removes the client from the room.
 */
void sub_unsub_client(gchar*, client_t*);

/**
 * Send a message to the the room.
 */
status_t pub_message(gchar*, message_t*);
