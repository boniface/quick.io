/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

#define PATH_STARTERS "./\\~"
#define PATH_LIB_DIR "/usr/lib/quickio"

#define CALLBACKS \
	X(init) \
	X(exit)

/**
 * No need for a lock on this: all qev_cfg callbacks are joined on a single
 * lock, so only 1 callback can fire at any give time.
 */
static GPtrArray *_apps = NULL;

static void _add_app(
	const gchar *name,
	const gchar *path)
{
	guint i;
	gboolean ok;
	qio_app_cb cb;
	GString *full_path = qev_buffer_get();
	guint *magic_num = NULL;
	struct app *app = g_slice_alloc0(sizeof(*app));

	app->name = g_strdup(name);

	if (strspn(path, PATH_STARTERS) == 0) {
		g_string_printf(full_path, "%s/%s", PATH_LIB_DIR, path);
	} else {
		g_string_assign(full_path, path);
	}

	app->mod = g_module_open(full_path->str, G_MODULE_BIND_LOCAL);
	ASSERT(app->mod != NULL,
			"Could not open app %s: %s", name, g_module_error());

	/*
	 * For config: unloading modules causes segfaults on exit because the
	 * modules can be closed an removed from memory. Keep them in memory at
	 * all times: not like they can be removed at runtime anyway.
	 */
	g_module_make_resident(app->mod);

	for (i = 0; i < _apps->len; i++) {
		struct app *oapp = g_ptr_array_index(_apps, i);
		ASSERT(app->mod != oapp->mod,
				"Duplicate app detected %s (%s).",
				name, full_path->str);
	}

	ok = g_module_symbol(app->mod, "__qio_is_app", (void*)&magic_num) &&
			*magic_num == QIO_MAGIC_NUM;
	ASSERT(ok, "Loaded module is not a QuickIO app: %s (%s)",
				name, full_path->str)

	#define X(fn) \
		ASSERT(g_module_symbol(app->mod, "__qio_app_" G_STRINGIFY(fn), (void*)&cb), \
				"App missing default QuickIO function %s: %s (%s)", \
				"__qio_app_" G_STRINGIFY(fn), name, full_path->str); \
			app->fn = cb;

		CALLBACKS
	#undef X

	ASSERT(app->init(),
			"Could not initialize app (%s): init() failed",
			name);

	g_ptr_array_add(_apps, app);
	qev_buffer_put(full_path);
}

static void _cleanup()
{
	guint i;

	for (i = 0; i < _apps->len; i++) {
		struct app *app = g_ptr_array_index(_apps, i);
		ASSERT(app->exit(), "App failed to exit: %s", app->name);
		g_free(app->name);
		g_slice_free1(sizeof(*app), app);
	}

	g_ptr_array_free(_apps, TRUE);
	_apps = NULL;
}

void apps_init()
{
	_apps = g_ptr_array_new();
	qev_cfg_listen("quick.io-apps", _add_app);

	qev_cleanup_fn(_cleanup);
}

void apps_test()
{
	guint i;
	for (i = 0; i < _apps->len; i++) {
		qio_app_cb test;
		struct app *app = g_ptr_array_index(_apps, i);

		ASSERT(g_module_symbol(app->mod, "__qio_app_test", (void*)&test),
				"Could not load test function for app: %s. "
				"Did you use QUICKIO_APP_TEST?",
				app->name);
		ASSERT(test(), "Tests failed in app: %s", app->name);
	}
}
