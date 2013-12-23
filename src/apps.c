/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

#define PATH_STARTERS "./\\~"
#define PATH_LIB_DIR "/usr/lib/quickio"

#define CALLBACKS \
	X(init) \
	X(run) \
	X(exit) \
	X(test)

/**
 * No need for a lock on this: all qev_cfg callbacks are joined on a single
 * lock, so only 1 callback can fire at any give time.
 */
static GPtrArray *_apps = NULL;

static void* _app_run_th(void *app_)
{
	struct app *app = app_;
	ASSERT(app->run(), "App failed to run: %s", app->prefix);
	return NULL;
}

static void _add_app(
	const gchar *name,
	const gchar *path)
{
	qio_app_cb cb;
	gchar *full_path;
	struct app app;
	struct app *papp = NULL;
	guint *magic_num = NULL;
	gboolean good = FALSE;

	if (strspn(path, PATH_STARTERS) == 0) {
		full_path = g_strdup_printf("%s/%s", PATH_LIB_DIR, path);
	} else {
		full_path = g_strdup(path);
	}

	app.mod = g_module_open(full_path, G_MODULE_BIND_LOCAL);
	if (app.mod == NULL) {
		CRITICAL("Could not open app %s (%s): %s", name, path, g_module_error());
		goto out;
	}

	good = g_module_symbol(app.mod, "__qio_is_app", (void*)&magic_num) &&
			*magic_num == QIO_MAGIC_NUM;
	if (!good) {
		CRITICAL("Loaded module is not a QuickIO app: %s (%s)", name, full_path);
		goto out;
	}

	#define X(fn) \
		if (g_module_symbol(app.mod, "__qio_app_" G_STRINGIFY(fn), (void*)&cb)) { \
			qio_app_cb *acb = (qio_app_cb*)&app.fn; \
			*acb = cb; }

		CALLBACKS
	#undef X

	app.name = g_strdup(name);
	app.prefix = g_strdup(name[0] == '/' ? name : "");
	papp = g_slice_copy(sizeof(app), &app);

	if (!app.init(papp)) {
		CRITICAL("Could not initialize app: %s", full_path);
		goto out;
	}

	papp->th = g_thread_new(papp->name, _app_run_th, papp);

	g_ptr_array_add(_apps, papp);
	good = TRUE;

out:
	g_free(full_path);

	if (!good) {
		g_free(app.name);
		g_free(app.prefix);

		if (papp != NULL) {
			g_slice_free1(sizeof(*papp), papp);
		}

		if (app.mod != NULL) {
			g_module_close(app.mod);
		}
	}
}

static void _cleanup()
{
	guint i;

	for (i = 0; i < _apps->len; i++) {
		struct app *app = g_ptr_array_index(_apps, i);
		if (!app->exit()) {
			FATAL("App failed to exit: %s", app->name);
		}

		g_free(app->name);
		g_free(app->prefix);
		g_thread_join(app->th);
		g_module_close(app->mod);
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
