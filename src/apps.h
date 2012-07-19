#pragma once
#include <glib.h>
#include <gmodule.h>
#include <libsoup/soup.h>
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
typedef void (*app_pubsub_cb)(gchar*, client_t*);

/**
 * The callbacks that the apps will recieve for different events.
 */
typedef struct app_s {
	// The name of the app
	gchar *name;
	
	// The thread the app is running in
	GThread *thread;
	
	// The module reference
	GModule *module;
	
	// All of the app callback functions, struct app_callbacks for more
	GThreadFunc run;
	app_bool_cb prefork;
	app_bool_cb postfork;
	app_client_cb client_connect;
	app_client_cb client_close;
	app_pubsub_cb subscribe;
	app_pubsub_cb unsubscribe;
	app_cb register_events;
} app_t;

/**
 * Init the list of apps and load their modules.
 */
gboolean apps_init(void);

/**
 * Run through the apps preforking procedures.
 */
gboolean apps_prefork(void);

/**
 * Run the apps in their own, separate threads. This should be run after forking
 * to ensure that all sockets and files are process-specific and not shared.
 */
gboolean apps_run(void);

/**
 * Gets the app named.
 */
app_t* apps_get_app(gchar*);

/**
 * Tell the apps it's their turns to register the evnets they listen for.  This runs in
 * the main thread before forking.
 */
void apps_register_events(void);

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

/**
 * Inform all the apps when a client has subscribed to something.
 */
void apps_pubsub_subscribe(gchar*, client_t*);

/**
 * Inform all the apps when a client has unsubscribed to something.
 */
void apps_pubsub_unsubscribe(gchar*, client_t*);