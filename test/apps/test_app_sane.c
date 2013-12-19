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
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *val;
	gboolean good;

	good = qio_json_unpack(json, "{%s}", "key", &val);
	if (good && g_strcmp0(val, "value") == 0) {
		qio_send_cb(client, client_cb, CODE_OK, NULL, "\"test\"");
	} else {
		qio_send_cb(client, client_cb, CODE_BAD, "Error with \"key\" in json.", NULL);
	}

	return EVS_STATUS_HANDLED;
}

static gboolean _app_init()
{
	qio_add_handler("/good", _test_handler, NULL, NULL, TRUE);
	qio_add_handler("/good2", _test_handler, NULL, NULL, TRUE);
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
