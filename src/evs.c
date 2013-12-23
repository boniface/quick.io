/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * A generic event with a path and some data
 */
#define EV_FORMAT "%s%s:%" G_GUINT64_FORMAT "=%s"

/**
 * For STATUS_OK callbacks
 */
#define CB_FORMAT \
	"/qio/callback/%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT "=" \
	"{\"code\":%d,\"data\":%s}"

/**
 * For !STATUS_OK callbacks. Notice the missing %s}: qev_json_pack is used
 * to pack the JSON string, so the trailing } must be appended.
 */
#define CB_ERR_FORMAT \
	"/qio/callback/%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT "=" \
	"{\"code\":%d,\"data\":%s,\"err_msg\":"

#define NO_CALLBACK (0l)

struct broadcast {
	struct subscription *sub;
	gchar *json;
};

/**
 * Essentially: [^_\-/a-zA-Z0-9]
 */
static const gchar _allowed_chars[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,
	0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  0,  0,  0,  0,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,
};

/**
 * All pending broadcasts
 */
static GAsyncQueue *_broadcasts = NULL;

static void _clean_ev_path(gchar *path)
{
	gchar *curr = path;
	gchar *writing = path;

	while (*curr != '\0') {
		if (_allowed_chars[(guchar)*curr] && writing != curr) {
			if (!(*writing == '/' && *curr == '/')) {
				writing++;
			}

			*writing = *curr;
		}

		curr++;
	}

	// Remove any trailing slash
	if (*writing != '/') {
		writing++;
	}

	*writing = '\0';
}

static void _broadcast(struct client *client, void *frames_)
{
	GString **frames = frames_;
	protocols_bcast_write(client, frames);
}

static void _broadcast_free(void *bc_)
{
	struct broadcast *bc = bc_;

	g_free(bc->json);
	sub_unref(bc->sub);
	g_slice_free1(sizeof(*bc), bc);
}

void event_init(
	struct event *ev,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children)
{
	ev->ev_path = g_strdup(ev_path);
	ev->handler_fn = handler_fn;
	ev->on_fn = on_fn;
	ev->off_fn = off_fn;
	ev->handle_children = handle_children;
	ev->subs = g_hash_table_new_full(g_str_hash, g_str_equal,
										NULL, (qev_free_fn)sub_unref);
	g_rw_lock_init(&ev->subs_lock);
}

void event_clear(struct event *ev)
{
	g_free(ev->ev_path);
	ev->ev_path = NULL;

	g_hash_table_unref(ev->subs);
	ev->subs = NULL;

	g_rw_lock_clear(&ev->subs_lock);
}

struct event* evs_add_handler(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children)
{
	struct event *ev;
	gchar *ep = g_strdup(ev_path);

	_clean_ev_path(ep);

	ev = evs_query_insert(ep, handler_fn, on_fn, off_fn, handle_children);
	if (ev == NULL) {
		WARN("Failed to create event %s: event already exists.", ep);
	}

	g_free(ep);

	return ev;
}

enum evs_status qio_evs_no_on(
	client_t *client G_GNUC_UNUSED,
	subscription_t *sub G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED)
{
	return EVS_STATUS_ERR;
}

void evs_route(
	struct client *client,
	gchar *ev_path,
	const evs_cb_t client_cb,
	gchar *json)
{
	struct event *ev;
	gchar *ev_extra = NULL;
	enum evs_status status = EVS_STATUS_ERR;

	_clean_ev_path(ev_path);

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		DEBUG("Could not find handler for event: %s", ev_path);
		goto out;
	}

	DEBUG("Got event: client=%p, ev_path=%s, ev_extra=%s, callback=%lu, json=%s",
			client, ev_path, ev_extra, client_cb, json);

	if (ev->handler_fn != NULL) {
		status = ev->handler_fn(client, ev_extra, client_cb, json);
	}

out:
	switch (status) {
		case EVS_STATUS_OK:
			qio_evs_cb(client, client_cb, NULL);
			break;

		case EVS_STATUS_ERR:
			qio_evs_err_cb(client, client_cb, CODE_UNKNOWN, NULL, NULL);
			break;

		case EVS_STATUS_HANDLED:
			break;
	}
}

void evs_on(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb)
{
	struct subscription *sub;
	enum evs_status status = EVS_STATUS_OK;

	DEBUG("Subscribing to: ev_path=%s, ev_extra=%s", ev->ev_path, ev_extra);

	sub = sub_get(ev, ev_extra);

	qev_lock(client);

	if (client_sub_has(client, sub)) {
		qio_evs_cb(client, client_cb, NULL);
		sub_unref(sub);
		goto out;
	}

	if (ev->on_fn != NULL) {
		status = ev->on_fn(client, sub, ev_extra, client_cb);
	}

	if (status != EVS_STATUS_HANDLED) {
		qio_evs_on_cb(status == EVS_STATUS_OK, client, sub, client_cb);
	}

out:
	qev_unlock(client);
}

void qio_evs_send(
	client_t *client,
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json)
{
	qio_evs_send_full(client, ev, ev_extra, json, NULL, NULL, NULL);
}

void qio_evs_send_full(
	client_t *client,
	event_t *ev,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn)
{
	evs_cb_t server_cb = 0;
	struct subscription *sub = sub_get(ev, ev_extra);
	GString *buff = qev_buffer_get();
	json = json ? : "null";

	if (!ev->handle_children && (ev_extra != NULL || *ev_extra != '\0')) {
		WARN("Sending event %s%s to client, but %s doesn't handle_children, "
				"so no subscription was possible.",
				ev->ev_path, sub->ev_extra, ev->ev_path);
	}

	qev_lock(client);

	if (!g_hash_table_contains(client->subs, sub)) {
		CRITICAL("Client is not subscribed to %s%s. Sending it an event "
				"is wrong.", ev->ev_path, sub->ev_extra);
	}

	g_string_printf(buff, EV_FORMAT, ev->ev_path, sub->ev_extra,
									server_cb, json);

	protocols_write(client, buff->str, buff->len);

	qev_unlock(client);

	qev_buffer_put(buff);
}

void qio_evs_on_cb(
	const gboolean success,
	struct client *client,
	struct subscription *sub,
	const evs_cb_t client_cb)
{
	gboolean good;
	enum evs_code code = CODE_OK;

	if (!success) {
		code = CODE_UNAUTH;
		goto error;
	}

	good = client_sub_add(client, sub);
	if (!good) {
		code = CODE_ENHANCE_CALM;
		goto error;
	}

out:
	qio_evs_err_cb(client, client_cb, code, NULL, NULL);
	return;

error:
	sub_unref(sub);
	goto out;
}

void evs_off(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra)
{
	struct subscription *sub = sub_get(ev, ev_extra);
	gboolean removed = client_sub_remove(client, sub);

	if (removed && sub->ev->off_fn != NULL) {
		sub->ev->off_fn(client, ev_extra);
	}

	sub_unref(sub);
}

void evs_client_close(struct client *client)
{
	client_sub_remove_all(client);
}

void qio_evs_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *json)
{
	qio_evs_cb_full(client, client_cb, CODE_OK, NULL, json,
					NULL, NULL, NULL);
}

void qio_evs_err_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json)
{
	qio_evs_cb_full(client, client_cb, code, err_msg, json,
					NULL, NULL, NULL);
}

void qio_evs_cb_full(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	GString *buff;
	evs_cb_t server_cb = 0;

	if (client_cb == 0) {
		// Glad we could have this chat, guys
		// @todo free cb data and move on
		return;
	}

	// @todo check if code != CODE_OK && server_cb != 0 -> that's an error

	json = json ? : "null";
	buff = qev_buffer_get();

	if (code == CODE_OK) {
		g_string_printf(buff, CB_FORMAT, client_cb, server_cb, code, json);
	} else {
		g_string_printf(buff, CB_ERR_FORMAT, client_cb, NO_CALLBACK, code, json);
		qev_json_pack(buff, "%s", err_msg);
		g_string_append_c(buff, '}');
	}

	protocols_write(client, buff->str, buff->len);

	qev_buffer_put(buff);
}

void qio_evs_broadcast(
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json)
{
	struct broadcast bc = {
		.sub = sub_get(ev, ev_extra),
		.json = g_strdup(json),
	};

	g_async_queue_push(_broadcasts, g_slice_copy(sizeof(bc), &bc));
}

void evs_broadcast_path(const gchar *ev_path, const gchar *json)
{
	gchar *ev_extra;
	struct event *ev = evs_query(ev_path, &ev_extra);

	if (ev != NULL) {
		qio_evs_broadcast(ev, ev_extra, json);
	}
}

void evs_broadcast_tick()
{
	GString **frames;
	struct broadcast *bc;
	GString *e = qev_buffer_get();

	while ((bc = g_async_queue_try_pop(_broadcasts)) != NULL) {
		g_string_printf(e, EV_FORMAT, bc->sub->ev->ev_path, bc->sub->ev_extra,
						NO_CALLBACK, bc->json);
		frames = protocols_bcast(e->str, e->len);

		qev_list_foreach(bc->sub->subscribers, _broadcast,
						cfg_broadcast_threads, TRUE, frames);

		protocols_bcast_free(frames);
		_broadcast_free(bc);
	}

	qev_buffer_put(e);
}

void evs_init()
{
	_broadcasts = g_async_queue_new_full(_broadcast_free);
	qev_cleanup_and_null((void**)&_broadcasts,
						(qev_free_fn)g_async_queue_unref);

	evs_query_init();
	evs_qio_init();
}
