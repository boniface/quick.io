/**
 * @file apps.h
 *
 * Deals with apps in bulk.
 * The general design pattern behind the applications is that they are entirely independent
 * from the server. Anything the apps do should have no impact on the rest of the server
 * (segfaults not withstanding). As such, the apps should run with their own references to
 * everything, such that any changes to the internal structures do not affect the apps,
 * and vis-versa. That being said, the only reference the apps share with the main server
 * is `client`, but that id ref-counted and void* in the apps (in other words, it may not be
 * modified for any reason).
 *
 * Generally speaking, each app runs in its own thread, independent of the server, and it
 * recieves callbacks asynchronously. If the application defines an `app_run()` function,
 * then it is responsible for allocating time to fire any events it recieves.
 */

#pragma once
#include "qio.h"

/**
 * The characters that can begin a path.
 */
#define PATH_STARTERS "./\\~"

/**
 * How to reference the current directory.
 */
#define PATH_CURR_DIR "./"

/**
 * Where the applications are stored, by default
 */
#define APP_PATH "apps/"

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
 * A generic app callback
 */
typedef void (*app_cb_str)();

/**
 * A callback that does work on a client.
 *
 * @param client The client to do work on.
 */
typedef void (*app_cb_client)(client_t *client);

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
typedef event_handler_t* (*app_on)(const gchar *event_path, handler_fn handler, on_subscribe_handler_cb on_subscribe, on_subscribe_handler_cb on_unsubscribe, gboolean handle_children);

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
	 * The app's internal ID.
	 */
	guint16 id;
	
	/**
	 * The name of the app.
	 *
	 * @attention This should NEVER be free'd
	 */
	gchar *name;
	
	/**
	 * The prefix that all events the app registers are forced to.
	 *
	 * @attention This should NEVER be free'd
	 */
	gchar *event_prefix;
	
	/**
	 * The thread the app is running in, if it has an `app_run` function.
	 */
	GThread *thread;
	
	/**
	 * The lock indicating that the application is done initing and ready to start serving
	 * requests.
	 */
	GMutex ready;
	
	/**
	 * A pointer to the module reference for accessing with GModule functions.
	 *
	 * @see http://developer.gnome.org/glib/2.32/glib-Dynamic-Loading-of-Modules.html
	 */
	GModule *module;
	
	/**
	 * The callback for setting application-specific options (name, path, etc).
	 */
	app_cb_str _set_app_opts;
	
	/**
	 * A reference to an application's `app_init` function.
	 * This function runs the application inside its own thread.
	 * 
	 * This allows the app to register events and setup anything it needs
	 * before the server starts serving requests.
	 */
	app_on_cb init;
	
	/**
	 * A reference to an application's `app_run` function.
	 * This function runs the application inside its own thread.
	 *
	 * In order to run an application loop, or anything,
	 * this function must be implemented.
	 */
	app_cb run;
	
	/**
	 * A reference to an application's `app_client_connect` function.
	 * A notification that a new client has connected and been accepted.
	 */
	app_cb_client client_connect;
	
	/**
	 * A reference to an application's `app_client_close` function.
	 * A notification that a client has closed.
	 */
	app_cb_client client_close;
	
	/**
	 * A reference to an application's `app_evs_client_subscribe` function.
	 * A notification that a client has subscribed to an event.
	 */
	on_subscribe_cb subscribe;
	
	/**
	 * A reference to an application's `app_evs_client_unsubscribe` function.
	 * A notification that a client has unsubscribed from an event.
	 */
	on_subscribe_cb unsubscribe;
	
	/**
	 * A reference to an application's `app_register_events` function.
	 * Tells the application to register its events with events_server.
	 */
	app_cb register_events;
} app_t;

/**
 * Initialize and run the applications.  This function goes through all the apps, and gives
 * them three calls, in order: app_init, app_register_events, app_run.
 */
gboolean apps_run();

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
void apps_evs_client_subscribe(client_t *client, const evs_client_sub_t *sub);

/**
 * Inform all the apps when a client has dropped a subscription.
 * 
 * @param client The client that unsubscribed.
 * @param sub The subscription the client dropped.
 */
void apps_evs_client_unsubscribe(client_t *client, const evs_client_sub_t *sub);

/**
 * Routes all queued events to the callbacks inside the apps.
 */
void apps_route_events(app_t app);

#ifdef TESTING
#include "../test/test_apps.h"
#endif