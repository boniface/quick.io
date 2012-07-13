#pragma once
#include <glib.h>
#include <stddef.h>

#include "client.h"

#define PATH_STARTERS "./\\~"
#define PATH_CURR_DIR "./"

/**
 * The message reciever implemented in apps for recieving info that a client has closed.
 */
typedef void (*app_cb)(void);
typedef gboolean (*app_bool_cb)(void);
typedef void (*app_client_cb)(client_t*);

/**
 * The callbacks that the apps will recieve for different events.
 */
typedef struct app_s {
	// The name of the app
	gchar *name;
	
	// The thread the app is running in
	GThread *thread;
	
	// All of the app callback functions, struct app_callbacks for more
	GThreadFunc run;
	app_bool_cb prefork;
	app_bool_cb postfork;
	app_client_cb client_connect;
	app_client_cb client_close;
	app_cb register_commands;
} app_t;

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
 * Callback for the apps, post-fork.
 */
gboolean apps_postfork(void);

/**
 * Inform all the apps that a new client has been accepted.
 */
void apps_client_connect(client_t*);

/**
 * Inform all the apps that a client has closed so that he can be removed from 
 * any internal tracking.
 */
void apps_client_close(client_t*);