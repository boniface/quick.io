#include <gmodule.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"
#include "debug.h"
#include "option.h"

#define APP_FOREACH(body) \
	for (gsize i = 0; i < _apps->len; i++) {\
		app_t *app = g_ptr_array_index(_apps, i); \
		body \
	}

/**
 * The callbacks that we're going to be using
 */
struct app_callbacks {
	// The offset of the callback position in the app_t struct
	int offset;
	
	// The name of the function in the module
	char *name;
};

static struct app_callbacks callbacks[] = {
	{offsetof(app_t, prefork), "app_prefork"},
	{offsetof(app_t, postfork), "app_postfork"},
	{offsetof(app_t, run), "app_run"},
	{offsetof(app_t, client_connect), "app_client_connect"},
	{offsetof(app_t, client_close), "app_client_close"},
	{offsetof(app_t, subscribe), "app_pubsub_subscribe"},
	{offsetof(app_t, unsubscribe), "app_pubsub_unsubscribe"},
	{offsetof(app_t, register_events), "app_register_events"},
};

/**
 * The information about the apps that are currently running.
 */
static GPtrArray *_apps;

gboolean apps_init() {
	gchar **app_names = option_apps();
	
	_apps = g_ptr_array_new();
	
	if (_apps == NULL) {
		ERROR("Could not allocate space for the apps.");
		return FALSE;
	}
	
	int app_count = option_apps_count();
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
		#warning Only export specific functions to modules
		#warning See: LDFLAGS (-Wl,-E (use libtool?) to dynamic-list)
		#warning See: linux.die.net/man/1/ld
		#warning See: www.gnu.org/software/libtool/manual/html_node/Building-modules.html
		#warning See: www.gnu.org/software/libtool/manual/html_node/Link-mode.html
		
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

void apps_register_commands() {
	// Go through all the apps and call their register_commands function
	APP_FOREACH(
		if (app->register_events != NULL) {
			app->register_events();
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

void apps_pubsub_subscribe(gchar *event, client_t *client) {
	APP_FOREACH(
		app_pubsub_cb cb = app->subscribe;
		
		if (cb != NULL) {
			cb(event, client);
		}
	)
}

void apps_pubsub_unsubscribe(gchar *event, client_t *client) {
	APP_FOREACH(
		app_pubsub_cb cb = app->unsubscribe;
		
		if (cb != NULL) {
			cb(event, client);
		}
	)
}