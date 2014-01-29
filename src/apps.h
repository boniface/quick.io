/**
 * Handles applications running inside the server.
 * @file apps.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Used to verify that modules are QIO apps.
 */
#define QIO_MAGIC_NUM 0xfc63e18a

/**
 * For an app: sets up all the necessary callbacks
 */
#define QUICKIO_APP(init, exit) \
	guint __qio_is_app = QIO_MAGIC_NUM; \
	gboolean __qio_app_init() \
	{ \
		return init(); } \
	gboolean __qio_app_exit() \
	{ \
		return exit(); }

/**
 * For an app: sets up all the necessary callbacks
 */
#define QUICKIO_APP_TEST(test) \
	gboolean __qio_app_test() \
	{ \
		return test(); }

/**
 * Callback type expected from the apps
 */
typedef gboolean (*qio_app_cb)();

/**
 * What we know about an app.
 */
struct app {
	/**
	 * The name of the app, just for logging purposes.
	 */
	gchar *name;

	/**
	 * The actual module. Closed when QIO is shutting down.
	 */
	GModule *mod;

	/**
	 * Default setup and run function.
	 */
	qio_app_cb init;

	/**
	 * Makes the app exit
	 */
	qio_app_cb exit;
};

/**
 * Listens for new apps in configuration and runs them as they're discovered.
 */
void apps_init();

/**
 * Runs app->test() on all the apps.
 */
void apps_test();

/**
 * If the app doesn't want to implement one of the functions, this may be
 * used as a shorcut
 */
static G_GNUC_UNUSED gboolean qio_app_noop() { return TRUE; }
