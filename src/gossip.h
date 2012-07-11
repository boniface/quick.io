#pragma once
#include <glib.h>

// The name of our shared memory
#define SHM_NAME "csocket.io"

// Simple way of getting SHM open
#define SHM_FLAGS (O_RDWR | O_CREAT)

// Only we can read-write this memory, but everyone else can read
#define SHM_MASK 0644

// The data that we use for gossiping
typedef struct gossip_s {
	guint32 clients;
	guint32 connects;
} gossip_t;

/**
 * Sets up the shared memory.
 */
gboolean gossip_init(void);

/**
 * The file descriptor for doing gossip as a client.
 */
gboolean gossip_client(void);

/**
 * Setup ourselves on a port for gossip.  We'll pass commands down the
 * write pipes to our children.
 */
gboolean gossip_server(void);

/**
 * Notification of a new client.
 *
 * This is synchronized and safe to call from anywhere.
 */
void gossip_client_connect(void);

/**
 * Notification of a new disconnecting.
 *
 * This is synchronized and safe to call from anywhere.
 */
void gossip_client_disconnect(void);