#pragma once
#include <glib.h>

#include "socket.h"

#define UNSUBSCRIBED 0

/**
 * Setup everything the pubsub needs to run.
 */
gboolean pubsub_init(void);

/**
 * Accepts a new client and asks it for information about what it wants.
 */
void sub_client_ready(client_t*);