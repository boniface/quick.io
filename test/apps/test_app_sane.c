/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "../../include/quickio_app.h"
#include <stdio.h>

static enum evs_status _test_handler(
	client_t *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb,
	gchar *json)
{
	qio_send_cb(client, client_cb, CODE_OK, NULL, "\"test\"");
	return EVS_STATUS_HANDLED;
}

static gboolean _app_init()
{
	qio_add_handler("/good", _test_handler, NULL, NULL, TRUE);
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
