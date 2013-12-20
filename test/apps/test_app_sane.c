/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "../../include/quickio_app.h"
#include <stdio.h>

static gint _ons = 0;
static gint _offs = 0;

static enum evs_status _stats_handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	GString *buff = qio_buffer_get();

	qio_json_pack(buff, "[%d,%d]", _ons, _offs);
	qio_evs_cb(client, client_cb, buff->str);

	qio_buffer_put(buff);

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *val;
	gboolean good;

	good = qio_json_unpack(json, "{%s}", "key", &val);
	if (good && g_strcmp0(val, "value") == 0) {
		qio_evs_cb(client, client_cb, "\"test\"");
	} else {
		qio_evs_err_cb(client, client_cb, CODE_BAD, "Error with \"key\" in json.", NULL);
	}

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_on(
	client_t *client G_GNUC_UNUSED,
	subscription_t *sub G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED)
{
	g_atomic_int_inc(&_ons);
	return EVS_STATUS_OK;
}

static void _good_off(
	client_t *client G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED)
{
	g_atomic_int_inc(&_offs);
}

static enum evs_status _delayed_subscribe(
	client_t *client,
	subscription_t *sub,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb)
{
	qio_evs_on_cb(TRUE, client, sub, client_cb);
	return EVS_STATUS_HANDLED;
}

static gboolean _app_init()
{
	qio_evs_add_handler("/stats", _stats_handler, NULL, NULL, TRUE);
	qio_evs_add_handler("/good", _good_handler, _good_on, _good_off, TRUE);
	qio_evs_add_handler("/good2", _good_handler, _good_on, _good_off, TRUE);
	qio_evs_add_handler("/delayed", NULL, _delayed_subscribe, NULL, TRUE);
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
