/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#define G_LOG_DOMAIN "test_app_failures"
#include "quickio.h"

static gboolean _app_exit()
{
	return FALSE;
}

QUICKIO_APP(
	qio_app_noop,
	_app_exit);
