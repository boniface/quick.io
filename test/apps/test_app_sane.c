/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "../../include/quickio_app.h"
#include <stdio.h>

static gboolean _app_init()
{
	qio_add_handler("/test", NULL, NULL, NULL, TRUE);
	printf("app init!\n");
	return TRUE;
}

static gboolean _app_test()
{
	printf("app test!\n");
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	qio_app_noop,
	qio_app_noop,
	_app_test);
