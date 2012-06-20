#include <glib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "debug.h"
#include "websocket.h"

/**
 * Get ready to accept incoming connections
 */
gboolean accepter_init(void);

/**
 * Clean up the accepter for shutdown
 */
void accepter_finish(void);