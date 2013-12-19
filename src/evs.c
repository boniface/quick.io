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

/**
 * Events are what are located at the paths, whereas subscriptions are what
 * are located at ev_path + ev_extra (ev_extra may == ""), and these are
 * what you can actually broadcast to.
 */
struct subscription {
	/**
	 * The event that this sub belongs to
	 */
	struct event *ev;

	/**
	 * To allow a way to remove sub from ev->subs
	 */
	gchar *ev_extra;

	/**
	 * All of the clients currently listening for this event
	 */
	qev_list_t *subscribers;

	/**
	 * Reference count to allow unlocked operations when dealing with
	 * subscriptions. This number technically represents the total number
	 * of clients subscribed and the number of outstanding references
	 * to the subscription. This allows a quick way to determine simultaneously
	 * if the list is empty and no one has a reference anymore.
	 */
	guint refs;
};

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

static gboolean _sub_get_if_exists(
	struct event *ev,
	const gchar *ev_extra,
	struct subscription **sub_)
{
	struct subscription *sub;

	sub = g_hash_table_lookup(ev->subs, ev_extra);
	if (sub != NULL && __sync_add_and_fetch(&sub->refs, 1) <= 1) {
		sub = NULL;
	}

	*sub_ = sub;

	return sub != NULL;
}

static struct subscription* _sub_get(
	struct event *ev,
	const gchar *ev_extra)
{
	struct subscription *sub;

	if (ev_extra == NULL) {
		ev_extra = "";
	}

	g_rw_lock_reader_lock(&ev->subs_lock);

	_sub_get_if_exists(ev, ev_extra, &sub);

	g_rw_lock_reader_unlock(&ev->subs_lock);

	if (sub == NULL) {
		g_rw_lock_writer_lock(&ev->subs_lock);

		if (!_sub_get_if_exists(ev, ev_extra, &sub)) {
			sub = g_slice_alloc0(sizeof(*sub));
			sub->ev = ev;
			sub->ev_extra = g_strdup(ev_extra);
			sub->subscribers = qev_list_new(qev_cfg_get_max_clients(), NULL);
			sub->refs = 1;

			/*
			 * Make sure that the free function isn't called: if the
			 * previous sub is still in the table, then there is a thread
			 * attempting to free it, but it's blocked on the write lock,
			 * so it cannot safely be touched.
			 */
			g_hash_table_steal(ev->subs, sub->ev_extra);
			g_hash_table_insert(ev->subs, sub->ev_extra, sub);
		}

		g_rw_lock_writer_unlock(&ev->subs_lock);
	}

	return sub;
}

static void _sub_unref(struct subscription *sub)
{
	struct event *ev = sub->ev;

	if (!g_atomic_int_dec_and_test(&sub->refs)) {
		return;
	}

	g_rw_lock_writer_lock(&ev->subs_lock);

	/*
	 * If a new subscription has replaced that one being removed here,
	 * play nice and let it live
	 */
	if (g_hash_table_lookup(ev->subs, sub->ev_extra) == sub) {
		g_hash_table_steal(ev->subs, sub->ev_extra);
	}

	g_rw_lock_writer_unlock(&ev->subs_lock);

	g_free(sub->ev_extra);
	qev_list_free(sub->subscribers);
	g_slice_free1(sizeof(*sub), sub);
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
	_sub_unref(bc->sub);
	g_slice_free1(sizeof(*bc), bc);
}

void event_init(
	struct event *ev,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn)
{
	ev->ev_path = g_strdup(ev_path);
	ev->handler_fn = handler_fn;
	ev->subscribe_fn = subscribe_fn;
	ev->unsubscribe_fn = unsubscribe_fn;
	ev->subs = g_hash_table_new_full(g_str_hash, g_str_equal,
										NULL, (qev_free_fn)_sub_unref);
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

void evs_add_handler(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children)
{
	gboolean created;
	gchar *ep = g_strdup(ev_path);

	_clean_ev_path(ep);

	created = evs_query_insert(ep, handler_fn, subscribe_fn,
								unsubscribe_fn, handle_children);
	if (!created) {
		WARN("Failed to create event %s: event already exists.", ep);
	}

	g_free(ep);
}

void evs_route(
	struct client *client,
	gchar *ev_path,
	const evs_cb_t client_cb,
	gchar *json)
{
	struct event *ev;
	gchar *ev_extra = NULL;
	enum evs_status status = EVS_STATUS_ERROR;

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
			evs_send_cb(client, client_cb, CODE_OK, NULL, NULL);
			break;

		case EVS_STATUS_ERROR:
			evs_send_cb(client, client_cb, CODE_UNKNOWN, NULL, NULL);
			break;

		case EVS_STATUS_HANDLED:
			break;
	}
}

gboolean evs_subscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb)
{
	struct subscription *sub;
	enum evs_status status = EVS_STATUS_OK;
	enum evs_code code = CODE_OK;

	DEBUG("Subscribing to: ev_path=%s, ev_extra=%s", ev->ev_path, ev_extra);

	sub = _sub_get(ev, ev_extra);

	qev_lock(client);

	if (g_hash_table_contains(client->subs, sub)) {
		_sub_unref(sub);
		goto out;
	}

	if (ev->subscribe_fn != NULL) {
		status = ev->subscribe_fn(client, ev_extra, client_cb);
	}

	if (status == EVS_STATUS_OK) {
		gint32 *idx = client_sub_get(client);

		if (idx == NULL) {
			code = CODE_ENHANCE_CALM;
			_sub_unref(sub);
		} else {
			qev_list_add(sub->subscribers, client, idx);
			g_hash_table_insert(client->subs, sub, idx);
		}
	}

	switch (status) {
		case EVS_STATUS_OK:
		case EVS_STATUS_ERROR:
			evs_send_cb(client, client_cb, code, NULL, NULL);
			break;

		case EVS_STATUS_HANDLED:
			break;
	}

out:
	qev_unlock(client);

	return code == CODE_OK;
}

void evs_unsubscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra)
{
	gint32 *idx;
	struct subscription *sub = _sub_get(ev, ev_extra);

	qev_lock(client);

	idx = g_hash_table_lookup(client->subs, sub);

	if (idx != NULL) {
		g_hash_table_remove(client->subs, sub);
		qev_list_remove(sub->subscribers, idx);
		client_sub_put(client, idx);
		_sub_unref(sub);
	}

	qev_unlock(client);

	_sub_unref(sub);
}

void evs_client_close(struct client *client)
{
	GHashTableIter iter;
	struct subscription *sub;
	gint32 *idx;

	qev_lock(client);
	g_hash_table_iter_init(&iter, client->subs);

	while (g_hash_table_iter_next(&iter, (void**)&sub, (void**)&idx)) {
		g_hash_table_iter_remove(&iter);
		qev_list_remove(sub->subscribers, idx);
		client_sub_put(client, idx);
		_sub_unref(sub);
	}

	qev_unlock(client);
}

void evs_send_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json)
{
	evs_send_cb_full(client, client_cb, code, err_msg, json,
					NULL, NULL, NULL);
}

void evs_send_cb_full(
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
	guint64 server_cb = 0;

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

void evs_broadcast(
	struct event *ev,
	const gchar *ev_extra,
	const gchar *json)
{
	struct broadcast bc = {
		.sub = _sub_get(ev, ev_extra),
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
	struct broadcast *bc;
	GString *e = qev_buffer_get();

	while ((bc = g_async_queue_try_pop(_broadcasts)) != NULL) {
		g_string_printf(e, EV_FORMAT, bc->sub->ev->ev_path,
						bc->sub->ev_extra, NO_CALLBACK, bc->json);
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
