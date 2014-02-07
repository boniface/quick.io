/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Path used for all callbacks
 */
#define CB_PATH_FORMAT "/qio/callback/%" G_GUINT32_FORMAT


/**
 * For STATUS_OK callbacks, this is the JSON data to send
 */
#define CB_OK_JSON_FORMAT "{\"code\":%d,\"data\":%s}"

/**
 * For !STATUS_OK callbacks. Notice the missing %s}: qev_json_pack is used
 * to pack the err_msg, so the trailing } must be appended.
 */
#define CB_ERR_JSON_FORMAT "{\"code\":%d,\"data\":%s,\"err_msg\":"

#define JSON_OR_NULL(json) \
	json = (json == NULL ? "null" : \
				(*json == '\0' ? "null" : json))

struct _broadcast {
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

static guint _clean_path(gchar *ev_path)
{
	gchar *curr = ev_path;
	gchar *writing = ev_path;

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
	if (*writing != '/' && writing != ev_path) {
		writing++;
	}

	*writing = '\0';

	return writing - ev_path;
}

static void _broadcast(void *client_, void *frames_)
{
	struct client *client = client_;
	GString **frames = frames_;
	protocols_bcast_write(client, frames);
}

static void _broadcast_free(void *bc_)
{
	struct _broadcast *bc = bc_;

	g_free(bc->json);
	sub_unref(bc->sub);
	g_slice_free1(sizeof(*bc), bc);
}

static void _off(
	struct client *client,
	struct subscription *sub)
{
	gboolean removed;

	qev_lock(client);

	removed = client_sub_remove(client, sub);
	if (removed && sub->ev->off_fn != NULL) {
		sub->ev->off_fn(client, sub->ev_extra);
	}

	qev_unlock(client);
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
	ev->subs = g_hash_table_new(g_str_hash, g_str_equal);
	g_rw_lock_init(&ev->subs_lock);
}

void event_clear(struct event *ev)
{
	g_hash_table_unref(ev->subs);
	ev->subs = NULL;

	g_free(ev->ev_path);
	ev->ev_path = NULL;

	g_rw_lock_clear(&ev->subs_lock);
}

struct event* evs_add_handler(
	const gchar *prefix,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children)
{
	struct event *ev;
	GString *ep = evs_clean_path(prefix, ev_path, NULL);

	ev = evs_query_insert(ep->str, handler_fn, on_fn, off_fn, handle_children);
	if (ev == NULL) {
		WARN("Failed to create event \"%s\": event already exists.", ep->str);
	}

	qev_buffer_put(ep);

	return ev;
}

GString* evs_clean_path(
	const gchar *ev_prefix,
	const gchar *ev_path,
	const gchar *ev_extra)
{

	GString *p = qev_buffer_get();

	g_string_printf(p, "%s/%s/%s",
		ev_prefix == NULL ? "" : ev_prefix,
		ev_path == NULL ? "" : ev_path,
		ev_extra == NULL ? "" : ev_extra);

	if (p->str[0] != '/') {
		g_string_prepend_c(p, '/');
	}

	g_string_set_size(p, _clean_path(p->str));

	return p;
}

enum evs_status evs_no_on(const struct evs_on_info *info G_GNUC_UNUSED)
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
	enum evs_code code = CODE_UNKNOWN;

	_clean_path(ev_path);
	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		DEBUG("Could not find handler for event: %s", ev_path);
		code = CODE_NOT_FOUND;
		goto out;
	}

	DEBUG("Got event: client=%p, ev_path=%s, ev_extra=%s, "
			"callback=%" G_GUINT32_FORMAT ", json=%s",
			client, ev_path, ev_extra, client_cb, json);

	if (ev->handler_fn != NULL) {
		status = ev->handler_fn(client, ev_extra, client_cb, json);
	}

out:
	switch (status) {
		case EVS_STATUS_OK:
			evs_cb(client, client_cb, NULL);
			break;

		case EVS_STATUS_ERR:
			evs_err_cb(client, client_cb, code, NULL, NULL);
			break;

		case EVS_STATUS_HANDLED:
			break;
	}
}

void evs_on(
	struct client *client,
	struct event *ev,
	gchar *ev_extra,
	const evs_cb_t client_cb)
{
	struct subscription *sub = sub_get(ev, ev_extra);
	struct evs_on_info info = {
		.ev_extra = ev_extra,
		.sub = sub,
		.client = client,
		.client_cb = client_cb,
	};

	DEBUG("Subscribing to: ev_path=%s, ev_extra=%s", ev->ev_path, ev_extra);

	qev_lock(client);

	switch (client_sub_add(client, sub)) {
		case CLIENT_SUB_PENDING:
			evs_err_cb(client, client_cb, CODE_ACCEPTED,
						"subscription pending", NULL);
			break;

		case CLIENT_SUB_TOMBSTONED: // shouldn't happen, but keep compiler quiet
		case CLIENT_SUB_NULL:
			evs_err_cb(client, client_cb, CODE_ENHANCE_CALM, NULL, NULL);
			break;

		case CLIENT_SUB_ACTIVE:
			evs_cb(client, client_cb, NULL);
			break;

		case CLIENT_SUB_CREATED: {
			enum evs_status status = EVS_STATUS_OK;
			if (ev->on_fn != NULL) {
				status = ev->on_fn(&info);
			}

			if (status != EVS_STATUS_HANDLED) {
				evs_on_cb(status == EVS_STATUS_OK, &info);
			}

			break;
		}
	}

	qev_unlock(client);
	sub_unref(sub);
}

void evs_send(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json)
{
	evs_send_full(client, ev, ev_extra, json, NULL, NULL, NULL);
}

void evs_send_info(const struct evs_on_info *info, const gchar *json)
{
	evs_send_info_full(info, json, NULL, NULL, NULL);
}

void evs_send_sub(
	struct client *client,
	struct subscription *sub,
	const gchar *json)
{
	evs_send_sub_full(client, sub, json, NULL, NULL, NULL);
}

void evs_send_full(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	struct subscription *sub = sub_get(ev, ev_extra);

	if (!ev->handle_children && ev_extra != NULL && *ev_extra != '\0') {
		WARN("Sending event %s%s to client, but %s doesn't handle_children, "
				"so no subscription was possible. Refusing to send event.",
				ev->ev_path, sub->ev_extra, ev->ev_path);
	} else {
		evs_send_sub_full(client, sub, json, cb_fn, cb_data, free_fn);
	}

	// @todo from that former memory leak: test multiple clients hitting things at random and really hard
	sub_unref(sub);
}

void evs_send_info_full(
	const struct evs_on_info *info,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const GDestroyNotify free_fn)
{
	evs_send_sub_full(info->client, info->sub, json, cb_fn, cb_data, free_fn);
}

void evs_send_sub_full(
	struct client *client,
	struct subscription *sub,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	if (client_sub_active(client, sub)) {
		JSON_OR_NULL(json);
		evs_cb_t server_cb = client_cb_new(client, cb_fn, cb_data, free_fn);
		protocols_send(client, sub->ev->ev_path, sub->ev_extra, server_cb, json);
	}
}

void evs_send_bruteforce(
	struct client *client,
	const gchar *ev_prefix,
	const gchar *ev_path,
	const gchar *ev_extra,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	GString *path;
	evs_cb_t server_cb = client_cb_new(client, cb_fn, cb_data, free_fn);
	JSON_OR_NULL(json);

	path = evs_clean_path(ev_prefix, ev_path, ev_extra);

	protocols_send(client, path->str, "", server_cb, json);

	qev_buffer_put(path);
}

void evs_on_cb(
	const gboolean success,
	const struct evs_on_info *info)
{
	enum evs_code code = CODE_OK;

	qev_lock(info->client);

	if (success) {
		if (!client_sub_accept(info->client, info->sub)) {
			_off(info->client, info->sub);
		}
	} else {
		if (info->sub->ev->handle_children) {
			code = CODE_NOT_FOUND;
		} else {
			code = CODE_UNAUTH;
		}

		client_sub_reject(info->client, info->sub);
	}

	qev_unlock(info->client);

	evs_err_cb(info->client, info->client_cb, code, NULL, NULL);
}

struct evs_on_info* evs_on_info_copy(
	const struct evs_on_info *info,
	const gboolean with_ev_extra)
{
	struct evs_on_info *ret = g_slice_copy(sizeof(*info), info);

	qev_ref(info->client);
	sub_ref(info->sub);

	if (with_ev_extra) {
		ret->ev_extra = g_strdup(info->ev_extra);
	} else {
		ret->ev_extra = NULL;
	}

	return ret;
}

void evs_on_info_free(struct evs_on_info *info)
{
	qev_unref(info->client);
	sub_unref(info->sub);

	if (info->ev_extra) {
		g_free(info->ev_extra);
	}

	g_slice_free1(sizeof(*info), info);
}

void evs_off(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra)
{
	struct subscription *sub = sub_get(ev, ev_extra);
	_off(client, sub);
	sub_unref(sub);
}

void evs_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *json)
{
	evs_cb_full(client, client_cb, CODE_OK, NULL, json,
					NULL, NULL, NULL);
}

void evs_cb_with_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	evs_cb_full(client, client_cb, CODE_OK, NULL, json,
					cb_fn, cb_data, free_fn);
}

void evs_err_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json)
{
	evs_cb_full(client, client_cb, code, err_msg, json,
					NULL, NULL, NULL);
}

void evs_cb_full(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	GString *path;
	GString *jbuff;
	evs_cb_t server_cb = EVS_NO_CALLBACK;

	if (client_cb == 0) {
		client_cb_new(NULL, NULL, cb_data, free_fn);
		return;
	}

	if (code == CODE_OK) {
		server_cb = client_cb_new(client, cb_fn, cb_data, free_fn);
	} else {
		client_cb_new(NULL, NULL, cb_data, free_fn);
	}

	JSON_OR_NULL(json);
	path = qev_buffer_get();
	jbuff = qev_buffer_get();

	g_string_printf(path, CB_PATH_FORMAT, client_cb);

	if (code == CODE_OK) {
		g_string_printf(jbuff, CB_OK_JSON_FORMAT, code, json);
	} else {
		g_string_printf(jbuff, CB_ERR_JSON_FORMAT, code, json);
		qev_json_pack(jbuff, "%s", err_msg);
		g_string_append_c(jbuff, '}');
	}

	protocols_send(client, path->str, "", server_cb, jbuff->str);

	qev_buffer_put(path);
	qev_buffer_put(jbuff);
}

void evs_broadcast(
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json)
{
	JSON_OR_NULL(json);

	struct _broadcast bc = {
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
		evs_broadcast(ev, ev_extra, json);
	}
}

void evs_broadcast_tick()
{
	GString **frames;
	struct _broadcast *bc;
	GString *path = qev_buffer_get();

	while ((bc = g_async_queue_try_pop(_broadcasts)) != NULL) {
		g_string_printf(path, "%s%s", bc->sub->ev->ev_path, bc->sub->ev_extra);
		frames = protocols_bcast(path->str, bc->json);

		qev_list_foreach(bc->sub->subscribers, _broadcast,
						cfg_broadcast_threads, TRUE, frames);

		protocols_bcast_free(frames);
		_broadcast_free(bc);
	}

	qev_buffer_put(path);
}

void evs_init()
{
	_broadcasts = g_async_queue_new_full(_broadcast_free);
	qev_cleanup_and_null((void**)&_broadcasts,
						(qev_free_fn)g_async_queue_unref);

	evs_query_init();
	evs_qio_init();
}
