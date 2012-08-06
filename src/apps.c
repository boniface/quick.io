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
	// The offset of the callback position in the app_t struct
	int offset;
	
	// The name of the function in the module
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
};

/**
 * The information about the apps that are currently running.
 */
static GPtrArray *_apps;

gboolean apps_init() {
	gchar **app_names = option_apps();
	gchar **app_prefixes = option_apps_prefixes();
	
	_apps = g_ptr_array_new();
	
	if (_apps == NULL) {
		ERROR("Could not allocate space for the apps.");
		return FALSE;
	}
	
	gint32 app_count = option_apps_count();
	for (int i = 0; i < app_count; i++) {
		gchar *app_path = *(app_names + i);
		
		// Check to see if an absolute path to a module was given
		// If it was not, assuming looking in ./, so add that to the
		// name, and open that module
		gchar *path;
		if (strspn(app_path, PATH_STARTERS) == 0) {
			size_t len = strlen(app_path) + sizeof(PATH_CURR_DIR);
			path = malloc(len * sizeof(*path));
			snprintf(path, len, "%s%s", PATH_CURR_DIR, app_path);
		} else {
			path = g_strdup(app_path);
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
		app_t *app = malloc(sizeof(*app));
		
		if (app == NULL) {
			ERRORF("Could not allocate memory for app: %s", path);
			return FALSE;
		}
		
		// We allocated, so save it
		g_ptr_array_add(_apps, app);
		
		// Make sure the app is all blank
		memset(app, 0, sizeof(*app));
		
		// Save the name of the app
		gchar* (*app_name)(void);
		if (g_module_symbol(module, "app_name", (void**)&app_name)) {
			app->name = app_name();
		} else {
			ERRORF("App doesn't have a name: %s", path);
			return FALSE;
		}
		
		// We might need this elsewhere
		app->module = module;
		
		if (option_apps_prefixes_count() > 0) {
			app->event_prefix = *(app_prefixes + i);
		}
		
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
	}

	return TRUE;
}

app_t* apps_get_app(gchar *app_name) {
	APP_FOREACH(
		if (strcmp(app_name, app->name) == 0) {
			return app;
		}
	)
	
	return NULL;
}

gboolean apps_run() {
	for (gsize i = 0; i < _apps->len; i++) {
		// Get the app
		app_t *app = g_ptr_array_index(_apps, i);
		
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
	}
	
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
			void on(gchar *event, handler_fn handler, on_subscribe_cb on_subscribe, on_subscribe_cb on_unsubscribe, gboolean handle_children) {
				if (app->event_prefix != NULL) {
					GString *e = g_string_new(app->event_prefix);
					g_string_append(e, event);
					evs_server_on(e->str, handler, on_subscribe, on_unsubscribe, handle_children);
					g_string_free(e, TRUE);
				} else {
					evs_server_on(event, handler, on_subscribe, on_unsubscribe, handle_children);
				}
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

void apps_client_close(client_t *client) {
	APP_FOREACH(
		app_client_cb cb = app->client_close;
		
		if (cb != NULL) {
			cb(client);
		}
	)
}

void apps_client_connect(client_t *client) {
	APP_FOREACH(
		app_client_cb cb = app->client_connect;
		
		if (cb != NULL) {
			cb(client);
		}
	)
}

void apps_evs_client_subscribe(gchar *event, client_t *client) {
	// Notify the individual event that it recieved a subscription
	gchar *e = g_strdup(event);
	GList *extra = NULL;
	guint extra_len = 0;
	event_info_t *einfo = evs_server_get_einfo(e, &extra, &extra_len);
	g_free(e);
	
	if (einfo != NULL && einfo->on_subscribe != NULL) {
		einfo->on_subscribe(client, extra, extra_len);
	}
	
	// Notify all the general listeners that a subscription happened.
	APP_FOREACH(
		app_evs_client_cb cb = app->subscribe;
		
		if (cb != NULL) {
			cb(event, client);
		}
	)
}

void apps_evs_client_unsubscribe(gchar *event, client_t *client) {
	// Notify the individual event that it lost a subscription
	gchar *e = g_strdup(event);
	GList *extra = NULL;
	guint extra_len = 0;
	event_info_t *einfo = evs_server_get_einfo(e, &extra, &extra_len);
	g_free(e);
	
	if (einfo != NULL && einfo->on_unsubscribe != NULL) {
		einfo->on_unsubscribe(client, extra, extra_len);
	}
	
	// Notify all the general listeners that an unsubscription happened.
	APP_FOREACH(
		app_evs_client_cb cb = app->unsubscribe;
		
		if (cb != NULL) {
			cb(event, client);
		}
	)
}