/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static enum evs_status _sub(
	struct client *client,
	const gchar *_ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *ev_path;
	gchar *ev_extra;
	enum qev_json_status jstatus;
	enum evs_status status = EVS_ERROR;
	struct event *ev;

	jstatus = qev_json_unpack(json, NULL, "%s", &ev_path);
	if (jstatus != qev_json_ok) {
		goto out;
	}

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		goto out;
	}

	if (ev->subscribe_fn == NULL) {
		status = EVS_OK;
	} else {
		status = ev->subscribe_fn(client, ev_extra, client_cb);
	}

	if (status == EVS_OK) {
		evs_subscribe(client, ev, ev_extra, 0);
	}

out:
	return status;
}

void evs_qio_init()
{
	evs_on(NULL, "/qio/sub", _sub, NULL, NULL, TRUE);
}
