#pragma once
#include <glib.h>

#include "socket.h"

#define UNSUBSCRIBED 0

/**
 * Setup everything the pubsub needs to run.
 */
gboolean pubsub_init(void);

/**
 * Puts the client into the UNSUBSCRIBED room, waiting for any futher commands.
 */
void sub_client_ready(client_t*);