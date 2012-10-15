#include "qio.h"

#define APP_FOREACH(body) \
	for (gsize i = 0; i < _apps->len; i++) {\
		app_t *app = g_ptr_array_index(_apps, i); \
		body \
	}

/**
 * The callbacks that we're going to be using
 */
typedef struct _app_callbacks_s {
	/**
	 * The offset of the callback position in the app_t struct
	 */
	int offset;
	
	/**
	 * The name of the function in the module
	 */
	char *name;
} _app_callbacks_t;

static _app_callbacks_t callbacks[] = {
	// Called to setup basic constants in the app
	{offsetof(app_t, _set_app_opts), "qio_set_app_opts"},
	
	// For running the app in its own thread
	{offsetof(app_t, init), "app_init"},
	{offsetof(app_t, run), "app_run"},
	
	// Called in the main thread when the app is running
	{offsetof(app_t, client_connect), "app_client_connect"},
	{offsetof(app_t, client_close), "app_client_close"},
	{offsetof(app_t, subscribe), "app_evs_client_subscribe"},
	{offsetof(app_t, unsubscribe), "app_evs_client_unsubscribe"},
};

/**
 * The information about the apps that are currently running.
 */
static GPtrArray *_apps;

/**
 * If the applications inited successfully in their threads
 */
static gboolean _apps_init_success = TRUE;

/**
 * Runs the thread that controls an entire application.
 */
static gpointer _app_run(gpointer void_app) {
	app_t *app = void_app;
	event_handler_t* on(const gchar *event_path, const handler_fn fn, const on_subscribe_handler_cb on_subscribe, const on_unsubscribe_handler_cb on_unsubscribe, const gboolean handle_children) {
		event_handler_t* handler;
		
		if (app->event_prefix != NULL) {
			gchar *e = g_strdup_printf("%s%s", app->event_prefix, event_path);
			handler = evs_server_on(e, fn, on_subscribe, on_unsubscribe, handle_children);
			g_free(e);
		} else {
			handler = evs_server_on(event_path, fn, on_subscribe, on_unsubscribe, handle_children);
		}
		
		return handler;
	}
	
	if (app->init != NULL) {
		_apps_init_success &= app->init(on);
	}
	
	g_mutex_lock(&(app->ready));
	
	if (app->run) {
		if (!app->run(on)) {
			FATALF("App \"%s\" exited with bad status", app->name);
		}
	}
	
	return NULL;
}

gboolean apps_run() {
	opt_app_t **apps = option_apps();
	_apps = g_ptr_array_new();
	
	guint16 app_count = option_apps_count();
	for (guint16 i = 0; i < app_count; i++) {
		opt_app_t *o_app = *(apps + i);
		
		// Check to see if an absolute path to a module was given
		// If it was not, assuming looking in ./, so add that to the
		// name, and open that module
		gchar *path;
		if (strspn(o_app->path, PATH_STARTERS) == 0) {
			size_t len = strlen(o_app->path) + sizeof(PATH_CURR_DIR) + sizeof(APP_PATH);
			path = g_malloc0((len * sizeof(*path)));
			snprintf(path, len, "%s"APP_PATH"%s", PATH_CURR_DIR, o_app->path);
		} else {
			path = g_strdup(o_app->path);
		}
		
		// No flags: expose all the symbols so that any dynamically-loaded
		// libraries (Python, JS, etc) have access to the necessary symbols + 
		// resolve them on load
		GModule *module = g_module_open(path, 0);
		
		// If we can't open the app, just quit
		if (module == NULL) {
			ERRORF("Could not open app (%s): %s", path, g_module_error());
			return FALSE;
		}
		
		// For storing the app information
		app_t *app = g_malloc0(sizeof(*app));
		
		// We allocated, so save it
		g_ptr_array_add(_apps, app);
		
		// Some basics that we'll definitely need
		app->module = module;
		app->id = i;
		app->name = o_app->config_group;
		app->event_prefix = o_app->prefix;
		
		// Find the absolute path to the directory holding the module
		// There is no error checking here: after g_module_open, the path must exist
		// otherwise, there would have been nothing to open, so there's really not a 
		// case this can fail in
		dirname(path);
		gchar abspath[PATH_MAX];
		realpath(path, abspath);
		
		// Done dealing with path
		free(path);
		
		// Register all the callbacks the app has
		for (gsize i = 0; i < G_N_ELEMENTS(callbacks); i++) {
			// For use with the current callback being registered
			gpointer curr_cb;
			
			if (g_module_symbol(module, callbacks[i].name, &curr_cb)) {
				// Just side-step type check altogether. They're all function
				// pointers
				*((app_cb*)(((char*)app) + callbacks[i].offset)) = curr_cb;
			}
		}
		
		// If the app didn't register any of the boilerplate stuff
		if (app->_set_app_opts == NULL) {
			ERRORF("App \"%s\" missing qio_set_app_opts()", app->name);
			return FALSE;
		}
		
		app->_set_app_opts(i, app->name, abspath);
		g_mutex_init(&(app->ready));
		app->thread = g_thread_new(app->name, _app_run, app);
	}
	
	// Wait for the applications to be ready before allowing the server to run
	APP_FOREACH(
		while (g_mutex_trylock(&(app->ready))) {
			g_mutex_unlock(&(app->ready));
			g_thread_yield();
		}
	)
	
	return _apps_init_success;
}

void apps_client_connect(client_t *client) {
	APP_FOREACH(
		app_cb_client cb = app->client_connect;
		
		if (cb != NULL) {
			cb(client);
		}
	)
}

void apps_client_close(client_t *client) {
	APP_FOREACH(
		app_cb_client cb = app->client_close;
		
		if (cb != NULL) {
			cb(client);
		}
	)
}

status_t apps_evs_client_check_subscribe(client_t *client, const event_handler_t *handler, path_extra_t extra, const callback_t client_callback) {
	if (handler->on_subscribe) {
		return handler->on_subscribe(client, handler, extra, client_callback);
	}
	
	return CLIENT_GOOD;
}

gboolean apps_evs_client_subscribe(client_t *client, const gchar *event_path, path_extra_t extra) {
	// Notify all the general listeners that a subscription happened.
	APP_FOREACH(
		on_subscribe_cb cb = app->subscribe;
		
		if (cb != NULL) {
			cb(client, event_path, extra);
		}
	)
	
	return TRUE;
}

void apps_evs_client_unsubscribe(client_t *client, const event_handler_t *handler, const gchar *event_path, path_extra_t extra) {
	if (handler->on_unsubscribe != NULL) {
		handler->on_unsubscribe(client, handler, extra);
	}
	
	// Notify all the general listeners that an unsubscription happened.
	APP_FOREACH(
		on_subscribe_cb cb = app->unsubscribe;
		
		if (cb != NULL) {
			cb(client, event_path, extra);
		}
	)
}

#ifdef TESTING
#include "../test/test_apps.c"
#endif