/**
 * @file apps.h
 *
 * @brief Deals with apps in bulk.
 * @details Each function calls the respective function on each app.
 */

#pragma once
#include <stddef.h>

#include "qio.h"

/**
 * A macro to ease boiler-plate in the apps.
 */
#define APP_NAME(name) \
	gchar* app_name() { \
		return g_strdup(name); \
	}

/**
 * The characters that can begin a path.
 */
#define PATH_STARTERS "./\\~"

/**
 * How to reference the current directory.
 */
#define PATH_CURR_DIR "./"

/**
 * A completely void app callback.
 */
typedef void (*app_cb)();

/**
 * An app callback that returns a boolean.
 * @return A yes or no, depending on the function.
 */
typedef gboolean (*app_bool_cb)();

/**
 * A callback that does work on a client.
 *
 * @param client The client to do work on.
 */
typedef void (*app_client_cb)(const client_t *client);

/**
 * A callback for when a client subscribes/unsubscribes from an event.
 *
 * @param event The event that was triggered.
 * @param client The client that sent the event.
 */
typedef void (*app_evs_client_cb)(const client_t *client, const path_extra_t extra, const guint extra_len);

/**
 * For closured "on" functions.
 *
 * @param event The name of the event to be notified for.
 * @param handler The function that should be called when the event comes in.
 * @param on_subscribe The function that should be notified when a client subscribes to this event.
 * @param handle_children If this handler handles children events with handlers.
 *
 * @return The object representing the event, able to be called.
 *
 * @see event_info_s
 */
typedef event_handler_t* (*app_on)(const gchar *event_path, handler_fn handler, on_subscribe_cb on_subscribe, on_subscribe_cb on_unsubscribe, gboolean handle_children);

/**
 * A module callback that takes an "on" function.
 *
 * @param on The function to call to register event handlers.
 */
typedef void (*app_on_cb)(app_on on);

/**
 * The callbacks that the apps will recieve for different events.
 */
typedef struct app_s {
	/**
	 * The name of the app.
	 *
	 * @attention This should NEVER be free'd
	 */
	gchar *name;
	
	/**
	 * The prefix that all events the app registers are forced to.
	 */
	gchar *event_prefix;
	
	/**
	 * The thread the app is running in, if it has an `app_run` function.
	 */
	GThread *thread;
	
	/**
	 * A pointer to the module reference for accessing with GModule functions.
	 *
	 * @see http://developer.gnome.org/glib/2.32/glib-Dynamic-Loading-of-Modules.html
	 */
	GModule *module;
	
	/**
	 * A reference to an application's `app_run` function.
	 * This function runs the application inside its own thread.
	 */
	GThreadFunc run;
	
	/**
	 * A reference to an application's `app_prefork` function.
	 * A notification to the application that the server is about to fork itself. This is
	 * called from the main thread of the main server process.
	 */
	app_bool_cb prefork;
	
	/**
	 * A reference to an application's `app_postfork` function.
	 * A notification to the application that the server has forked itself. This is
	 * called from the child process.
	 */
	app_bool_cb postfork;
	
	/**
	 * A reference to an application's `app_client_connect` function.
	 * A notification that a new client has connected and been accepted.
	 */
	app_client_cb client_connect;
	
	/**
	 * A reference to an application's `app_client_close` function.
	 * A notification that a client has closed.
	 */
	app_client_cb client_close;
	
	/**
	 * A reference to an application's `app_evs_client_subscribe` function.
	 * A notification that a client has subscribed to an event.
	 */
	app_evs_client_cb subscribe;
	
	/**
	 * A reference to an application's `app_evs_client_unsubscribe` function.
	 * A notification that a client has unsubscribed from an event.
	 */
	app_evs_client_cb unsubscribe;
	
	/**
	 * A reference to an application's `app_register_events` function.
	 * Tells the application to register its events with events_server.
	 */
	app_cb register_events;
} app_t;

/**
 * Init the list of apps and load their modules.
 */
gboolean apps_init();

/**
 * Run through the apps preforking procedures.
 */
gboolean apps_prefork();

/**
 * Run the apps in their own, separate threads. This should be run after forking
 * to ensure that all sockets and files are process-specific and not shared.
 */
gboolean apps_run();

/**
 * Gets the app named.
 *
 * @param name The name of the app to get the module for.
 */
app_t* apps_get_app(gchar *name);

/**
 * Tell the apps it's their turns to register the evnets they listen for.  This runs in
 * the main thread before forking.
 */
void apps_register_events();

/**
 * Callback for the apps, post-fork.
 */
gboolean apps_postfork();

/**
 * Inform all the apps that a new client has been accepted.
 *
 * @param client The client that was accepted.
 */
void apps_client_connect(client_t *client);

/**
 * Inform all the apps that a client has closed so that he can be removed from 
 * any internal tracking.
 *
 * @param client The client that closed.
 */
void apps_client_close(client_t *client);

/**
 * Inform all the apps when a client has added a subscription.
 * 
 * @param client The client that subscribed.
 * @param sub The subscription the client added.
 */
void apps_evs_client_subscribe(const client_t *client, const evs_client_sub_t *sub);

/**
 * Inform all the apps when a client has dropped a subscription.
 * 
 * @param client The client that unsubscribed.
 * @param sub The subscription the client dropped.
 */
void apps_evs_client_unsubscribe(const client_t *client, const evs_client_sub_t *sub);