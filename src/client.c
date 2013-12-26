/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static gint32* _sub_get(struct client *client G_GNUC_UNUSED)
{
	// @todo actually implement that heuristic :-P
	// @todo with expandable qev_lists + max subscribe stats/heuristic
	return g_slice_alloc(sizeof(gint32));
}

static void _sub_put(struct client *client G_GNUC_UNUSED, gint32 *idx)
{
	g_slice_free1(sizeof(*idx), idx);
}

static void _cb_data_free(void *cb_data, const qev_free_fn free_fn)
{
	if (cb_data != NULL && free_fn != NULL) {
		free_fn(cb_data);
	}
}

static void _cb_remove(struct client *client, const guint i)
{
	g_slice_free1(sizeof(**client->cbs), client->cbs[i]);
	client->cbs[i] = NULL;
}

static void _cb_free(struct client *client, const guint i)
{
	_cb_data_free(client->cbs[i]->cb_data, client->cbs[i]->free_fn);
	_cb_remove(client, i);
}

static void _cb_remove_all(struct client *client)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS(client->cbs); i++) {
		if (client->cbs[i] != NULL) {
			_cb_free(client, i);
		}
	}
}

evs_cb_t client_cb_new(
	struct client *client,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	guint i;
	struct client_cb cb;

	if (cb_fn == NULL) {
		_cb_data_free(cb_data, free_fn);
		return EVS_NO_CALLBACK;
	}

	qev_lock(client);

	for (i = 0; i < G_N_ELEMENTS(client->cbs); i++) {
		if (client->cbs[i] == NULL) {
			break;
		}
	}

	if (i == G_N_ELEMENTS(client->cbs)) {
		i = (guint)g_random_int_range(0, G_N_ELEMENTS(client->cbs));
		_cb_free(client, i);
	}

	cb = (struct client_cb){
		.id = ++client->cbs_id,
		.cb_fn = cb_fn,
		.cb_data = cb_data,
		.free_fn = free_fn,
	};

	client->cbs[i] = g_slice_copy(sizeof(cb), &cb);

	qev_unlock(client);

	// @todo test limits of callbacks
	return (i << 16) | cb.id;
}

enum evs_status client_cb_fire(
	struct client *client,
	const evs_cb_t server_cb,
	const evs_cb_t client_cb,
	gchar *json)
{
	struct client_cb cb = { .cb_fn = NULL };
	enum evs_status status;
	guint16 slot = server_cb >> 16;
	guint16 id = server_cb & 0xffff;

	qev_lock(client);

	if (client->cbs[slot]->id == id) {
		cb = *client->cbs[slot];
		_cb_remove(client, slot);
	}

	qev_unlock(client);

	if (cb.cb_fn == NULL) {
		qio_evs_err_cb(client, client_cb, CODE_NOT_FOUND,
							"callback doesn't exist", NULL);
		return EVS_STATUS_HANDLED;
	}

	status = cb.cb_fn(client, client_cb, json);
	_cb_data_free(cb.cb_data, cb.free_fn);

	return status;
}

gboolean client_sub_has(struct client *client, struct subscription *sub)
{
	gboolean has = FALSE;

	qev_lock(client);

	if (client->subs == NULL) {
		goto out;
	}

	has = g_hash_table_contains(client->subs, sub);

out:
	qev_unlock(client);
	return has;
}

gboolean client_sub_add(
	struct client *client,
	struct subscription *sub)
{
	gint32 *idx;
	gboolean good = TRUE;

	qev_lock(client);

	if (client_sub_has(client, sub)) {
		goto out;
	}

	idx = _sub_get(client);
	if (idx == NULL) {
		good = FALSE;
		goto out;
	}

	if (client->subs == NULL) {
		client->subs = g_hash_table_new(NULL, NULL);
	}

	qev_list_add(sub->subscribers, client, idx);
	g_hash_table_insert(client->subs, sub, idx);

out:
	qev_unlock(client);
	return good;
}

gboolean client_sub_remove(struct client *client, struct subscription *sub)
{
	gint32 *idx;
	gboolean removed = FALSE;

	qev_lock(client);

	if (client->subs == NULL) {
		goto out;
	}

	idx = g_hash_table_lookup(client->subs, sub);
	if (idx != NULL) {
		g_hash_table_remove(client->subs, sub);
		qev_list_remove(sub->subscribers, idx);
		_sub_put(client, idx);
		sub_unref(sub);

		removed = TRUE;

		if (g_hash_table_size(client->subs) == 0) {
			g_hash_table_unref(client->subs);
			client->subs = NULL;
		}
	}

out:
	qev_unlock(client);
	return removed;
}

void client_sub_remove_all(struct client *client)
{
	GHashTableIter iter;
	struct subscription *sub;
	gint32 *idx;

	qev_lock(client);

	if (client->subs != NULL) {
		g_hash_table_iter_init(&iter, client->subs);

		while (g_hash_table_iter_next(&iter, (void**)&sub, (void**)&idx)) {
			g_hash_table_iter_remove(&iter);
			qev_list_remove(sub->subscribers, idx);
			_sub_put(client, idx);
			sub_unref(sub);
		}

		g_hash_table_unref(client->subs);
		client->subs = NULL;
	}

	qev_unlock(client);
}

void client_close(struct client *client)
{
	_cb_remove_all(client);
	client_sub_remove_all(client);
}
