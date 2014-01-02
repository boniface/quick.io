/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

// #define APP_ENABLE_DEBUG
#define G_LOG_DOMAIN "test_app_failures"

#include "../../include/quickio_app.h"
#include <stdio.h>

static gboolean _app_exit()
{
	return FALSE;
}

QUICKIO_APP(
	qio_app_noop,
	qio_app_noop,
	_app_exit,
	qio_app_noop);
