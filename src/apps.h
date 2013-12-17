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
 * Callback type expected from the apps
 */
typedef gboolean (*qio_app_cb)();

/**
 * What we know about an app.
 */
struct app {
	/**
	 * A prefix that should be forced on everything the app does
	 */
	gchar *prefix;

	/**
	 * The actual module. Closed when QIO is shutting down.
	 */
	GModule *mod;

	/**
	 * Default setup function
	 */
	gboolean (*init)(void *app);

	/**
	 * The app's thread fn
	 */
	qio_app_cb run;

	/**
	 * Makes the app exit
	 */
	qio_app_cb exit;

	/**
	 * Runs the app's test cases
	 */
	qio_app_cb test;
};

/**
 * Listens for new apps in configuration and runs them as they're discovered.
 */
void apps_init();
