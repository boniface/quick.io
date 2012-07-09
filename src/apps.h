#pragma once
#include <glib.h>

#include "client.h"

#define PATH_STARTERS "./\\~"
#define PATH_CURR_DIR "./"

/**
 * The message reciever implemented in apps for recieving info that a client has closed.
 */
typedef void (*client_close_fn)(client_t*);

/**
 * Init the list of apps and load their modules.
 */
gboolean apps_init(void);

/**
 * Run the apps in their own, separate threads. This should be run after forking
 * to ensure that all sockets and files are process-specific and not shared.
 */
gboolean apps_run(void);

/**
 * Tell the apps it's their turns to register commands.  This runs in the main
 * thread before forking.
 */
void apps_register_commands(void);

/**
 * Inform all the apps that a client has closed so that he can be removed from 
 * any internal tracking.
 */
void apps_client_close(client_t*);