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
 * A global on subscribe callback: the type of function called whena user subscribes
 * or unsubscribes from an event.
 *
 * @param client The client that subscribed to the event.
 * @param event_path The path of the event that was subscribed to.
 * @param extra Any extra parameters that came in with the subscription
 */
typedef void (*on_subscribe_cb)(const client_t *client, const char *event_path, const path_extra_t *extra);

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
typedef event_handler_t* (*app_on)(const gchar *event_path, handler_fn handler, on_subscribe_handler_cb on_subscribe, on_unsubscribe_handler_cb on_unsubscribe, gboolean handle_children);

/**
 * The callback function for adding stats to be sent to graphite.
 *
 * @param key The graphite key to be added
 * @param val The number to be added
 */
typedef void (*apps_stats_cb)(gchar *key, double val);

/**
 * The function in the app that takes the apps_stats_cb.
 *
 * @param cb The function to be called to add stats to send to graphite.
 */
typedef void (*apps_stats_app_cb)(apps_stats_cb cb);

/**
 * A module callback that takes an "on" function.
 *
 * @param on The function to call to register event handlers.
 */
typedef gboolean (*app_on_cb)(app_on on);

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
	app_on_cb run;
	
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
	apps_stats_app_cb stats_flush;
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
 * Ask the app if the given subscription is okay.
 *
 * @param client The client that subscribed
 * @param handler The handler being subscribed to
 * @param extra The extra path segments for the event
 * @param client_callback The callback to be notified when verification is complete. Only to be used
 * when returning CLIENT_ASYNC.
 *
 * @return CLIENT_GOOD Everything is good, send the callback as normal.
 * @return CLIENT_ASYNC Doing async verification, will send the callback internally.
 * @return CLIENT_INVALID_SUBSCRIPTION if the subscription should be rejected
 */
status_t apps_evs_client_check_subscribe(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback);

/**
 * Inform all the apps when a client has added a subscription.
 * 
 * @param client The client that subscribed.
 * @param event_path The path to the event that was subscribed to.
 * @param extra The extra segments on the event.
 *
 * @return True if the client subscribed to a valid event, false otherwise,
 * indicating the subscription should be canceled.
 */
gboolean apps_evs_client_subscribe(client_t *client, const gchar *event_path, path_extra_t *extra);

/**
 * Inform all the apps when a client has dropped a subscription.
 * 
 * @param client The client that unsubscribed.
 * @param handler The event handler the client dropped.
 * @param event_path The event path the client dropped.
 * @param extra The extra path segments.
 */
void apps_evs_client_unsubscribe(client_t *client, const event_handler_t *handler, const char *event_path, path_extra_t *extra);

/**
 * Triggers the stats callback in all the apps.
 *
 * @param app_append The callback function for adding stats to the current set.
 */
void apps_stats_gather(stats_app_append_cb app_append);

#ifdef TESTING
#include "../test/test_apps.h"
#endif