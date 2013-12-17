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

static GPtrArray *_apps = NULL;

static void _add_app(
	const gchar *path,
	const gchar *prefix)
{
	qio_app_cb cb;
	gchar *full_path;
	struct app app;
	struct app *papp = NULL;
	guint *magic_num = NULL;
	gboolean good = FALSE;

	CRITICAL("path=%s, prefix=%s", path, prefix);

	if (strspn(path, PATH_STARTERS) == 0) {
		full_path = g_strdup_printf("%s/%s", PATH_LIB_DIR, path);
	} else {
		full_path = g_strdup(path);
	}

	app.mod = g_module_open(full_path, G_MODULE_BIND_LOCAL);
	if (app.mod == NULL) {
		CRITICAL("Could not open app (%s): %s", path, g_module_error());
		goto out;
	}

	good = g_module_symbol(app.mod, "__qio_is_app", (void*)&magic_num) &&
			*magic_num == QIO_MAGIC_NUM;
	if (!good) {
		CRITICAL("Loaded module is not a QuickIO app: %s", full_path);
		goto out;
	}

	#define X(fn) \
		if (g_module_symbol(app.mod, "__qio_app_" G_STRINGIFY(fn), (void*)&cb)) { \
			qio_app_cb *acb = (qio_app_cb*)&app.fn; \
			*acb = cb; }

		CALLBACKS
	#undef X

	app.prefix = g_strdup(prefix);
	papp = g_slice_copy(sizeof(app), &app);

	if (!app.init(papp)) {
		CRITICAL("Could not initialize app: %s", full_path);
		goto out;
	}

	// @todo start app here

	g_ptr_array_add(_apps, papp);
	good = TRUE;

out:
	g_free(full_path);

	if (!good) {
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
			FATAL("App failed to exit: %s", app->prefix);
		}

		g_free(app->prefix);
		g_module_close(app->mod);
		g_slice_free1(sizeof(*app), app);
	}

	g_ptr_array_free(_apps, TRUE);
}

void apps_init()
{
	_apps = g_ptr_array_new();
	qev_cfg_listen("quick.io-apps", _add_app);

	qev_cleanup_fn(_cleanup);
}
