/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static enum evs_status _on(
	struct client *client,
	const gchar *_ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *ev_path;
	gchar *ev_extra;
	struct event *ev;
	enum qev_json_status jstatus;

	jstatus = qev_json_unpack(json, NULL, "%s", &ev_path);
	if (jstatus != qev_json_ok) {
		qio_evs_err_cb(client, client_cb, CODE_BAD, "invalid json ev_path", NULL);
		goto out;
	}

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		qio_evs_err_cb(client, client_cb, CODE_NOT_FOUND, NULL, NULL);
		goto out;
	}

	evs_on(client, ev, ev_extra, client_cb);

out:
	return EVS_STATUS_HANDLED;
}

static enum evs_status _off(
	struct client *client,
	const gchar *_ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *ev_path;
	gchar *ev_extra;
	struct event *ev;
	enum qev_json_status jstatus;

	jstatus = qev_json_unpack(json, NULL, "%s", &ev_path);
	if (jstatus != qev_json_ok) {
		qio_evs_err_cb(client, client_cb, CODE_BAD, "invalid json ev_path", NULL);
		return EVS_STATUS_HANDLED;
	}

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		qio_evs_err_cb(client, client_cb, CODE_NOT_FOUND, NULL, NULL);
		return EVS_STATUS_HANDLED;
	}

	evs_off(client, ev, ev_extra);

	return EVS_STATUS_OK;
}

static enum evs_status _callback(
	struct client *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb,
	gchar *json)
{
	evs_cb_t server_cb = 0;

	if (*ev_extra == '/') {
		server_cb = g_ascii_strtoull(ev_extra + 1, NULL, 10);
	}

	if (server_cb == EVS_NO_CALLBACK) {
		qio_evs_err_cb(client, client_cb, CODE_BAD,
						"invalid server callback id", NULL);
		return EVS_STATUS_HANDLED;
	}

	return client_cb_fire(client, server_cb, client_cb, json);
}

void evs_qio_init()
{
	evs_add_handler("/qio/on", _on, qio_evs_no_on, NULL, FALSE);
	evs_add_handler("/qio/off", _off, qio_evs_no_on, NULL, FALSE);
	evs_add_handler("/qio/callback", _callback, qio_evs_no_on, NULL, TRUE);
}
