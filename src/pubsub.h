#pragma once
#include <glib.h>

#include "socket.h"

#define UNSUBSCRIBED "0"

/**
 * Setup everything the pubsub needs to run.
 */
gboolean pubsub_init(void);

/**
 * Puts the client into the UNSUBSCRIBED room, waiting for any futher commands.
 */
void sub_client_ready(client_t*);

/**
 * Subscribes a client to a room.  This will unsubscribe a client from his
 * other rooms if the single-subscribe flag was given.
 */
status_t sub_client(gchar*, client_t*);

/**
 * The client has been closed. Remove him from all the rooms.
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
