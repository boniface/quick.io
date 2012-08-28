#include "qio.h"

#define APP_FOREACH(body) \
	for (gsize i = 0; i < _apps->len; i++) {\
		app_t *app = g_ptr_array_index(_apps, i); \
		body \
	}

/**
 * The callbacks that we're going to be using
 */
typedef struct app_callbacks_s {
	/**
	 * The offset of the callback position in the app_t struct
	 */
	int offset;
	
	/**
	 * The name of the function in the module
	 */
	char *name;
} app_callbacks_t;

static app_callbacks_t callbacks[] = {
	{offsetof(app_t, prefork), "app_prefork"},
	{offsetof(app_t, postfork), "app_postfork"},
	{offsetof(app_t, run), "app_run"},
	{offsetof(app_t, client_connect), "app_client_connect"},
	{offsetof(app_t, client_close), "app_client_close"},
	{offsetof(app_t, subscribe), "app_evs_client_subscribe"},
	{offsetof(app_t, unsubscribe), "app_evs_client_unsubscribe"},
	{offsetof(app_t, register_events), "app_register_events"},
	
	{offsetof(app_t, _set_app_name), "qio_set_app_name"},
};

/**
 * The information about the apps that are currently running.
 */
static GPtrArray *_apps;

gboolean apps_init() {
	opt_app_t **apps = option_apps();
	
	_apps = g_ptr_array_new();
	if (_apps == NULL) {
		ERROR("Could not allocate space for the apps.");
		return FALSE;
	}
	
	gint32 app_count = option_apps_count();
	for (int i = 0; i < app_count; i++) {
		opt_app_t *o_app = *(apps + i);
		
		// Check to see if an absolute path to a module was given
		// If it was not, assuming looking in ./, so add that to the
		// name, and open that module
		gchar *path;
		if (strspn(o_app->path, PATH_STARTERS) == 0) {
			size_t len = strlen(o_app->path) + sizeof(PATH_CURR_DIR) + sizeof(APP_PATH);
			path = g_try_malloc0((len * sizeof(*path)));
			snprintf(path, len, "%s"APP_PATH"%s", PATH_CURR_DIR, o_app->path);
		} else {
			path = g_strdup(o_app->path);
		}
		
		// G_MODULE_BIND_MASK: Be module-lazy and make sure the module keeps
		// all his functions to himself
		GModule *module = g_module_open(path, G_MODULE_BIND_MASK);
		
		// If we can't open the app, just quit
		if (module == NULL) {
			ERRORF("Could not open app (%s): %s", path, g_module_error());
			return FALSE;
		}
		
		// For storing the app information
		app_t *app = g_try_malloc0(sizeof(*app));
		
		if (app == NULL) {
			ERRORF("Could not allocate memory for app: %s", path);
			return FALSE;
		}
		
		// We allocated, so save it
		g_ptr_array_add(_apps, app);
		
		// Some basics that we'll definitely need
		app->module = module;
		app->name = o_app->config_group;
		app->event_prefix = o_app->prefix;
		
		// Done looking at the path
		free(path);
		
		// Register all the callbacks the app has
		for (gsize i = 0; i < G_N_ELEMENTS(callbacks); i++) {
			// For use with the current callback being registered
			gpointer curr_cb;
			
			// Get the _client_close function for faster referencing later
			if (g_module_symbol(module, callbacks[i].name, &curr_cb)) {
				// Just side-step type check altogether. They're all function
				// pointers
				*((app_cb*)(((char*)app) + callbacks[i].offset)) = curr_cb;
			}
		}
		
		// If the app didn't register any of the boilerplate stuff
		if (app->_set_app_name == NULL) {
			ERRORF("App \"%s\" did not use APP_INIT()", app->name);
			return FALSE;
		}
		
		app->_set_app_name(app->name);
	}

	return TRUE;
}

gboolean apps_run() {
	APP_FOREACH(
		// If there isn't a run function, then just move on
		if (app->run == NULL) {
			continue;
		}
		
		GThread *thread = g_thread_try_new(__FILE__, app->run, NULL, NULL);
		
		if (thread == NULL) {
			ERRORF("Could not init thread for app %s", app->name);
			return FALSE;
		}
		
		app->thread = thread;
	)
	
	return TRUE;
}

gboolean apps_prefork() {
	APP_FOREACH(
		if (app->prefork != NULL && app->prefork() == FALSE) {
			return FALSE;
		}
	)
	
	return TRUE;
}

void apps_register_events() {
	// Go through all the apps and call their register_commands function
	APP_FOREACH(
		app_on_cb cb = app->register_events;
		if (cb != NULL) {
			event_handler_t* on(const gchar *event_path, const handler_fn fn, const on_subscribe_cb on_subscribe, const on_subscribe_cb on_unsubscribe, const gboolean handle_children) {
				event_handler_t* handler;
				if (app->event_prefix != NULL) {
					GString *e = g_string_new(app->event_prefix);
					g_string_append(e, event_path);
					handler = evs_server_on(e->str, fn, on_subscribe, on_unsubscribe, handle_children);
					g_string_free(e, TRUE);
				} else {
					handler = evs_server_on(event_path, fn, on_subscribe, on_unsubscribe, handle_children);
				}
				
				return handler;
			}
			
			cb(on);
		}
	)
}

gboolean apps_postfork() {
	APP_FOREACH(
		app_bool_cb cb = app->postfork;
		
		// If the postfork callback isn't happy
		if (cb != NULL && cb() == FALSE) {
			ERRORF("Application failed in postfork: %s", app->name);
			return FALSE;
		}
	)
	
	return TRUE;
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

void apps_evs_client_subscribe(const client_t *client, const evs_client_sub_t *sub) {
	if (sub->handler->on_subscribe != NULL) {
		sub->handler->on_subscribe(client, sub->extra, sub->extra_len);
	}
	
	// Notify all the general listeners that a subscription happened.
	APP_FOREACH(
		app_cb_evs_client cb = app->subscribe;
		
		if (cb != NULL) {
			cb(client, sub->event_path, sub->extra, sub->extra_len);
		}
	)
}

void apps_evs_client_unsubscribe(const client_t *client, const evs_client_sub_t *sub) {
	if (sub->handler->on_unsubscribe != NULL) {
		sub->handler->on_unsubscribe(client, sub->extra, sub->extra_len);
	}
	
	// Notify all the general listeners that an unsubscription happened.
	APP_FOREACH(
		app_cb_evs_client cb = app->unsubscribe;
		
		if (cb != NULL) {
			cb(client, sub->event_path, sub->extra, sub->extra_len);
		}
	)
}

#ifdef TESTING
#include "../test/test_apps.c"
#endif