#include <gmodule.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"
#include "debug.h"
#include "option.h"

// The number of apps that are loaded
static int _app_count = 0;

// The threads that the apps are running in.
static GThread **_threads;

// The apps that are running
static GModule **_apps;

// The client_close functions
static GPtrArray *_client_close;

gboolean apps_init() {
	gchar **app_names = option_apps();
	_app_count = option_apps_count();
	
	_apps = malloc(_app_count * sizeof(*_apps));
	_threads = malloc(_app_count * sizeof(*_threads));
	_client_close = g_ptr_array_new();
	
	if (_apps == NULL || _threads == NULL) {
		ERROR("Could not allocate space for the apps.");
		return FALSE;
	}
	
	for (int i = 0; i < _app_count; i++) {
		gchar *app_path = *(app_names + i);
		
		// Check to see if an absolute path to a module was given
		// If it was not, assuming looking in ./, so add that to the
		// name, and open that module
		gchar *module;
		if (strspn(app_path, PATH_STARTERS) == 0) {
			size_t len = strlen(app_path) + sizeof(PATH_CURR_DIR);
			module = malloc(len * sizeof(*module));
			snprintf(module, len, "%s%s", PATH_CURR_DIR, app_path);
		} else {
			module = g_strdup(app_path);
		}
		
		// G_MODULE_BIND_MASK: Be lazy and make sure the module keeps
		// all his functions to himself
		GModule *app = g_module_open(module, G_MODULE_BIND_MASK);
		
		// Done with the name of the module
		free(module);
		
		// If we can't open the app, just quit
		if (app == NULL) {
			ERRORF("Could not open app: %s", g_module_error());
			return FALSE;
		}
		
		// Save a reference to the app for later use
		*(_apps + i) = app;
		
		// Get the _client_close function for faster referencing later
		client_close_fn client_close;
		if (g_module_symbol(app, "app_client_close", (gpointer*)&client_close)) {
			g_ptr_array_add(_client_close, client_close);
		}
	}

	return TRUE;
}

gboolean apps_run() {
	for (int i = 0; i < _app_count; i++) {
		GModule *app = *(_apps + i);
		
		#warning Only export specific functions to modules
		#warning See: LDFLAGS (-Wl,-E (use libtool?) to dynamic-list)
		#warning See: linux.die.net/man/1/ld
		#warning See: www.gnu.org/software/libtool/manual/html_node/Building-modules.html
		#warning See: www.gnu.org/software/libtool/manual/html_node/Link-mode.html
		
		// Get a pointer to 
		gpointer (*app_run)(gpointer);
		if (!g_module_symbol(app, "app_run", (gpointer*)&app_run)) {
			ERRORF("App init error: %s", g_module_error());
			g_module_close(app);
			return FALSE;
		}
		
		GThread *thread = g_thread_try_new(__FILE__, app_run, NULL, NULL);
		
		if (thread == NULL) {
			ERROR("Could not init thread for app.");
			return FALSE;
		}
	}
	
	return TRUE;
}

void apps_register_commands() {
	// Go through all the apps and call their register_commands function
	for (int i = 0; i < _app_count; i++) {
		GModule *app = *(_apps + i);
		
		void (*app_register_commands)(void);
		if (g_module_symbol(app, "app_register_commands", (gpointer*)&app_register_commands)) {
			app_register_commands();
		}
	}
}

void apps_client_close(client_t *client) {
	#warning REWRITE this to be: apps_client_unsub, apps_client_sub (just events fired back into apps)
	for (gsize i = 0; i < _client_close->len; i++) {
		((client_close_fn)g_ptr_array_index(_client_close, i))(client);
	}
}