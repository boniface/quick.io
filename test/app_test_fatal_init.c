/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#define G_LOG_DOMAIN "test_app_failures"
#include "quickio.h"

static gboolean _app_init()
{
	return FALSE;
}

QUICKIO_APP(
	_app_init,
	qio_app_noop);
