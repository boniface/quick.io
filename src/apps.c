#include "qio.h"

#define APP_FOREACH(body) \
	for (gsize i = 0; i < _apps->len; i++) {\
		struct app *app = g_ptr_array_index(_apps, i); \
		body \
	}

/**
 * The callbacks that we're going to be using
 */
struct _app_callbacks {
	/**
	 * The offset of the callback position in the struct app
	 */
	int offset;

	/**
	 * The name of the function in the module
	 */
	gchar *name;
};

static struct _app_callbacks callbacks[] = {
	// Called to setup basic constants in the app
	{offsetof(struct app, _set_app_opts), "qio_set_app_opts"},

	// For running the app in its own thread
	{offsetof(struct app, init), "app_init"},
	{offsetof(struct app, run), "app_run"},
	{offsetof(struct app, test), "app_test"},

	// Called in the main thread when the app is running
	{offsetof(struct app, client_connect), "app_client_connect"},
	{offsetof(struct app, client_close), "app_client_close"},
	{offsetof(struct app, subscribe), "app_evs_client_subscribe"},
	{offsetof(struct app, unsubscribe), "app_evs_client_unsubscribe"},
	{offsetof(struct app, stats_flush), "app_stats_flush"},
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
static gpointer _app_run(gpointer void_app)
{
	struct app *app = void_app;
	struct event_handler* on(
		const gchar *event_path,
		const handler_fn fn,
		const on_subscribe_handler_cb on_subscribe,
		const on_unsubscribe_handler_cb on_unsubscribe,
		const gboolean handle_children)
	{
		struct event_handler* handler;

		if (app->event_prefix != NULL) {
			gchar *e = g_strdup_printf("%s%s", app->event_prefix, event_path);
			handler = evs_server_on(e, fn, on_subscribe,
								on_unsubscribe, handle_children);
			g_free(e);
		} else {
			handler = evs_server_on(event_path, fn, on_subscribe,
								on_unsubscribe, handle_children);
		}

		return handler;
	}

	if (app->init != NULL) {
		_apps_init_success &= app->init(on);
	}

	g_atomic_int_inc(&app->ready);

	if (app->run != NULL && !app->run(on)) {
		FATAL("App \"%s\" exited with bad status", app->name);
	}

	return NULL;
}

gboolean apps_run()
{
	const struct opt_app **apps = option_apps();
	_apps = g_ptr_array_new();

	guint16 app_count = option_apps_count();
	for (guint16 i = 0; i < app_count; i++) {
		const struct opt_app *o_app = *(apps + i);

		// Check to see if an absolute path to a module was given
		// If it was not, assuming looking in ./, so add that to the
		// name, and open that module
		gchar *path;
		if (strspn(o_app->path, PATH_STARTERS) == 0) {
			size_t len = strlen(o_app->path) + sizeof(PATH_LIB_DIR) + 1;
			path = g_malloc0((len * sizeof(*path)));
			snprintf(path, len, "%s/%s", PATH_LIB_DIR, o_app->path);
		} else {
			path = g_strdup(o_app->path);
		}

		// No flags: expose all the symbols so that any dynamically-loaded
		// libraries (Python, JS, etc) have access to the necessary symbols +
		// resolve them on load
		GModule *module = g_module_open(path, 0);
		if (module == NULL) {
			CRITICAL("Could not open app (%s): %s", path, g_module_error());
			return FALSE;
		}

		struct app *app = g_malloc0(sizeof(*app));
		g_ptr_array_add(_apps, app);
		app->module = module;
		app->id = i;
		app->name = o_app->config_group;
		app->event_prefix = evs_server_format_path(o_app->prefix, NULL);

		// Find the absolute path to the directory holding the module
		// There is no error checking here: after g_module_open, the path must exist
		// otherwise, there would have been nothing to open, so there's really not a
		// case this can fail in
		dirname(path);
		gchar abspath[PATH_MAX];
		// Silence compiler warnings
		gchar *unused __attribute__((unused));
		unused = realpath(path, abspath);

		g_free(path);

		// Register all the callbacks the app has
		for (gsize j = 0; j < G_N_ELEMENTS(callbacks); j++) {
			// For use with the current callback being registered
			gpointer curr_cb;

			if (g_module_symbol(module, callbacks[j].name, &curr_cb)) {
				// Just side-step type check altogether. They're all function
				// pointers
				*((app_cb*)(((gchar*)app) + callbacks[j].offset)) = curr_cb;
			}
		}

		// If the app didn't register any of the boilerplate stuff
		if (app->_set_app_opts == NULL) {
			CRITICAL("App \"%s\" missing qio_set_app_opts()", app->name);
			return FALSE;
		}

		app->_set_app_opts(i, app->name, abspath);
		app->thread = g_thread_new(app->name, _app_run, app);
	}

	// Wait for the applications to be ready before allowing the server to run
	APP_FOREACH(
		while (!g_atomic_int_compare_and_exchange(&app->ready, 1, 0)) {
			g_thread_yield();
		}
	)

	return _apps_init_success;
}

void apps_client_connect(struct client *client)
{
	APP_FOREACH(
		app_cb_client cb = app->client_connect;

		if (cb != NULL) {
			cb(client);
		}
	)
}

void apps_client_close(struct client *client)
{
	APP_FOREACH(
		app_cb_client cb = app->client_close;

		if (cb != NULL) {
			cb(client);
		}
	)
}

enum status apps_evs_client_check_subscribe(
	struct client *client,
	const struct event_handler *handler,
	path_extra_t *extra,
	const callback_t client_callback)
{
	if (handler->on_subscribe) {
		return handler->on_subscribe(client, handler,
						extra, client_callback);
	}

	return CLIENT_GOOD;
}

gboolean apps_evs_client_subscribe(
	struct client *client,
	const gchar *event_path,
	path_extra_t *extra)
{
	APP_FOREACH(
		on_subscribe_cb cb = app->subscribe;

		if (cb != NULL) {
			cb(client, event_path, extra);
		}
	)

	return TRUE;
}

void apps_evs_client_unsubscribe(
	struct client *client,
	const struct event_handler *handler,
	const gchar *event_path,
	path_extra_t *extra)
{
	if (handler->on_unsubscribe != NULL) {
		handler->on_unsubscribe(client, handler, extra);
	}

	APP_FOREACH(
		on_subscribe_cb cb = app->unsubscribe;

		if (cb != NULL) {
			cb(client, event_path, extra);
		}
	)
}

void apps_stats_gather(stats_app_append_cb app_append)
{
	APP_FOREACH(
		apps_stats_app_cb cb = app->stats_flush;

		if (cb != NULL) {
			void _append(gchar *key, double val) {
				app_append(app->event_prefix, key, val);
			}

			cb(_append);
		}
	)
}

gboolean apps_test()
{
	if (_apps->len != 1) {
		FATAL("Can only test 1 app at a time, not %u", _apps->len);
	}

	struct app *app = g_ptr_array_index(_apps, 0);

	if (app->test == NULL) {
		FATAL("App \"%s\" did not define `app_test()`", app->name);
	}

	return app->test(option_bind_port());
}

#ifdef TESTING
#include "../test/test_apps.c"
#endif
