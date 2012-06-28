#pragma once
#include <glib.h>

#define LISTEN_BACKLOG 100

/**
 * The file descriptor for doing gossip as a client.
 */
void gossip_client(int);

/**
 * Setup ourselves on a port for gossip.  We'll pass commands down the
 * write pipes to our children.
 */
gboolean gossip_server(int*);