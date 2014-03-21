/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static gchar *_json_hostname = NULL;

static enum evs_status _callback(
	struct client *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb,
	gchar *json)
{
	evs_cb_t server_cb = EVS_NO_CALLBACK;

	if (*ev_extra == '/') {
		server_cb = g_ascii_strtoull(ev_extra + 1, NULL, 10);
	}

	if (server_cb == EVS_NO_CALLBACK) {
		evs_err_cb(client, client_cb, CODE_BAD,
						"invalid server callback id", NULL);
		return EVS_STATUS_HANDLED;
	}

	return client_cb_fire(client, server_cb, client_cb, json);
}

static enum evs_status _hostname(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	evs_cb(client, client_cb, _json_hostname);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _on(
	struct client *client,
	const gchar *_ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *ev_path;
	gchar *ev_extra;
	struct event *ev;

	if (*json != '"') {
		evs_err_cb(client, client_cb, CODE_BAD, "invalid json ev_path", NULL);
		return EVS_STATUS_HANDLED;
	}

	ev_path = json + 1;
	evs_clean_path(ev_path);

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		evs_err_cb(client, client_cb, CODE_NOT_FOUND, NULL, NULL);
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

	if (*json != '"') {
		evs_err_cb(client, client_cb, CODE_BAD, "invalid json ev_path", NULL);
		return EVS_STATUS_HANDLED;
	}

	ev_path = json + 1;
	evs_clean_path(ev_path);

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		evs_err_cb(client, client_cb, CODE_NOT_FOUND, NULL, NULL);
		return EVS_STATUS_HANDLED;
	}

	evs_off(client, ev, ev_extra);

	return EVS_STATUS_OK;
}

static enum evs_status _ping(
	struct client *client G_GNUC_UNUSED,
	const gchar *_ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	return EVS_STATUS_OK;
}

void evs_qio_init()
{
	if (cfg_public_address != NULL) {
		GString *buff = qev_buffer_get();

		ASSERT(qev_json_pack(buff, "%s", cfg_public_address) == QEV_JSON_OK,
			"Failed to create JSON for public address.");
		_json_hostname = g_strdup(buff->str);

		qev_buffer_put(buff);
	}

	evs_add_handler("/qio", "/callback", _callback, evs_no_on, NULL, TRUE);
	evs_add_handler("/qio", "/hostname", _hostname, evs_no_on, NULL, FALSE);
	evs_add_handler("/qio", "/off", _off, evs_no_on, NULL, FALSE);
	evs_add_handler("/qio", "/on", _on, evs_no_on, NULL, FALSE);
	evs_add_handler("/qio", "/ping", _ping, evs_no_on, NULL, FALSE);
}
