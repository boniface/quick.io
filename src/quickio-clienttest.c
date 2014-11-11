/**
 * The partner app to quickio-clienttest
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#define G_LOG_DOMAIN "quickio-clienttest"
#define EV_PREFIX "/clienttest"
#include "quickio.h"

G_DEFINE_QUARK(heartbeat-challenge-cb, heartbeat_challenge_cb);
G_DEFINE_QUARK(in-progress-callback, in_progress_callback);

struct event *_ev_broadcast = NULL;
struct event *_ev_in_progress = NULL;

static enum evs_status _close_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	qev_close(client, 0);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _drop_callback_handler(
	struct client *client G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	return EVS_STATUS_HANDLED;
}

static enum evs_status _echo_cb(
	struct client *client,
	const void *data G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	evs_cb_with_cb(client, client_cb, json, _echo_cb, NULL, NULL);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _echo_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	return _echo_cb(client, NULL, client_cb, json);
}

static enum evs_status _error_handler(
	struct client *client,
	const gchar *ev_extra,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	gint64 code = 0;
	GString *msg = qev_buffer_get();

	if (*ev_extra == '/') {
		code = g_ascii_strtoll(ev_extra + 1, NULL, 10);
		g_string_printf(msg, "%lds are fun!", code);
	}

	evs_cb_full(client, client_cb, code, msg->str,
		"\"you requested an error?\"", NULL, NULL, NULL);

	qev_buffer_put(msg);

	return EVS_STATUS_HANDLED;
}

static enum evs_status _heartbeat_challenge_cb(
	struct client *client,
	const void *data G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	GVariant *v = client_get(client, heartbeat_challenge_cb_quark());

	if (v != NULL) {
		evs_cb(client, g_variant_get_uint64(v), NULL);
		client_del(client, heartbeat_challenge_cb_quark());
	}

	return EVS_STATUS_OK;
}

static enum evs_status _heartbeat_challenge_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	client_set(client,
		heartbeat_challenge_cb_quark(),
		g_variant_new_uint64(client_cb));

	evs_send_bruteforce(client, "/qio", "/heartbeat", NULL, NULL,
						_heartbeat_challenge_cb, NULL, NULL);

	return EVS_STATUS_HANDLED;
}

static enum evs_status _in_progress_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	GVariant *cb = client_get(client, in_progress_callback_quark());

	if (cb != NULL) {
		struct subscription *sub = sub_get(_ev_in_progress, NULL, TRUE);
		struct evs_on_info sub_info = {
			.client = client,
			.sub = sub,
			.client_cb = g_variant_get_uint64(cb),
		};

		evs_on_cb(TRUE, &sub_info);
		sub_unref(sub);
		g_variant_unref(cb);
		client_del(client, in_progress_callback_quark());

		return EVS_STATUS_OK;
	} else {
		return EVS_STATUS_ERR;
	}
}

static enum evs_status _in_progress_on(const struct evs_on_info *info)
{
	client_set(info->client,
		in_progress_callback_quark(),
		g_variant_new_uint64(info->client_cb));
	return EVS_STATUS_HANDLED;
}

static enum evs_status _on_data_on(const struct evs_on_info *info)
{
	guint64 i;
	enum qev_json_status status;

	status = qev_json_unpack(info->json, NULL, "%d", &i);
	if (status == QEV_JSON_OK && i == 123987) {
		evs_on_cb(TRUE, info);
		evs_send_info(info, "true");
		return EVS_STATUS_HANDLED;
	} else {
		return EVS_STATUS_ERR;
	}
}

static enum evs_status _move_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json)
{
	evs_send_bruteforce(client, "/qio", "/move", NULL, json, NULL, NULL, NULL);
	qev_close(client, QIO_CLOSE_MOVED);
	return EVS_STATUS_OK;
}

static enum evs_status _send_invalid_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	protocols_send(client, "unicorns, ponies, princesses, and magic!:good times", NULL, 0, "");
	return EVS_STATUS_HANDLED;
}

static enum evs_status _send_unsubscribed_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	evs_send_bruteforce(client, "/you/didnt/subscribe/to/this", NULL, NULL, NULL, NULL, NULL, NULL);
	return EVS_STATUS_HANDLED;
}

static void _broadcast(void *nothing G_GNUC_UNUSED)
{
	evs_broadcast(_ev_broadcast, NULL, "\"broadcast event\"");
}

static gboolean _app_init()
{
	_ev_broadcast = evs_add_handler(
		EV_PREFIX, "/broadcast",
		NULL, NULL, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/close",
		_close_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/drop-callback",
		_drop_callback_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/echo",
		_echo_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/error",
		_error_handler, evs_no_on, NULL, TRUE);
	evs_add_handler(
		EV_PREFIX, "/heartbeat-challenge",
		_heartbeat_challenge_handler, evs_no_on, NULL, FALSE);
	_ev_in_progress = evs_add_handler(
		EV_PREFIX, "/in-progress",
		_in_progress_handler, _in_progress_on, NULL, TRUE);
	evs_add_handler(
		EV_PREFIX, "/move",
		_move_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/send-invalid",
		_send_invalid_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/send-unsubscribed",
		_send_unsubscribed_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(
		EV_PREFIX, "/on-data",
		NULL, _on_data_on, NULL, FALSE);

	qev_timer(10, _broadcast, NULL);

	return TRUE;
}

static gboolean _app_exit()
{
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	_app_exit);
