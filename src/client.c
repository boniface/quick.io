/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static qev_stats_counter_t *_stat_subs_total;
static qev_stats_counter_t *_stat_subs_added;
static qev_stats_counter_t *_stat_subs_removed;

static qev_stats_counter_t *_stat_callbacks_total;
static qev_stats_counter_t *_stat_callbacks_created;
static qev_stats_counter_t *_stat_callbacks_fired;
static qev_stats_counter_t *_stat_callbacks_evicted;

/**
 * Assumes lock on client is held
 */
static struct client_sub* _sub_alloc(struct client *client)
{
	guint pressure;
	guint64 used;
	struct client_sub *csub = NULL;
	gboolean allocate = FALSE;

	used = qev_stats_counter_get(_stat_subs_total);

	if (used >= cfg_clients_max_subs) {
		; // No allocations allowed
	} else if (cfg_clients_subs_fairness == 0) {
		allocate = TRUE;
	} else {
		pressure = ((100 - cfg_clients_subs_fairness) / 100.0) * cfg_clients_max_subs;

		if (used >= pressure) {
			guint client_used = client->subs == NULL ? 0 : g_hash_table_size(client->subs);
			guint max_per = MAX(1, cfg_clients_max_subs / qev_cfg_get_max_clients()) *
							((20 / (.05 * cfg_clients_subs_fairness)) - 3);
			allocate = max_per > client_used;
		} else {
			allocate = TRUE;
		}
	}

	if (allocate) {
		qev_stats_counter_inc(_stat_subs_total);
		qev_stats_counter_inc(_stat_subs_added);

		csub = g_slice_alloc0(sizeof(*csub));
	}

	return csub;
}

static void _sub_free(struct client_sub *csub)
{
	qev_stats_counter_dec(_stat_subs_total);
	qev_stats_counter_inc(_stat_subs_removed);
	g_slice_free1(sizeof(*csub), csub);
}

/**
 * Gets the client's subscription, if there is one. You MUST be holding
 * a lock on the client.
 */
static struct client_sub* _sub_get(
	struct client *client,
	struct subscription *sub)
{
	if (client->subs == NULL) {
		return NULL;
	}
	return g_hash_table_lookup(client->subs, sub);
}

static void _sub_cleanup(
	struct client *client,
	struct subscription *sub,
	struct client_sub *csub)
{
	g_hash_table_remove(client->subs, sub);
	qev_list_remove(sub->subscribers, &csub->idx);
	_sub_free(csub);
	sub_unref(sub);

	if (g_hash_table_size(client->subs) == 0) {
		g_hash_table_unref(client->subs);
		client->subs = NULL;
	}
}

static gboolean _sub_remove(
	struct client *client,
	struct subscription *sub,
	const gboolean rejected)
{
	gboolean removed = FALSE;
	struct client_sub *csub = NULL;

	qev_lock(client);

	csub = _sub_get(client, sub);
	if (csub == NULL) {
		goto out;
	}

	if (!rejected && csub->pending) {
		csub->tombstone = TRUE;
	} else {
		removed = TRUE;
		_sub_cleanup(client, sub, csub);
	}

out:
	qev_unlock(client);

	return removed;
}

/**
 * Assumes lock on client is held
 */
static void _sub_remove_all(struct client *client)
{
	GHashTableIter iter;
	struct subscription *sub;
	struct client_sub *csub;

	if (client->subs != NULL) {
		g_hash_table_iter_init(&iter, client->subs);

		while (g_hash_table_iter_next(&iter, (void**)&sub, (void**)&csub)) {
			g_hash_table_iter_remove(&iter);
			qev_list_remove(sub->subscribers, &csub->idx);
			_sub_free(csub);
			sub_unref(sub);
		}

		g_hash_table_unref(client->subs);
		client->subs = NULL;
	}
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
	qev_stats_counter_dec(_stat_callbacks_total);
}

static void _cb_free(struct client *client, const guint i)
{
	_cb_data_free(client->cbs[i]->cb_data, client->cbs[i]->free_fn);
	_cb_remove(client, i);
}

/**
 * Assumes lock on client is held
 */
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
	guint16 i;
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
		qev_stats_counter_inc(_stat_callbacks_evicted);
	}

	client->cbs_id++;
	client->cbs_id = MAX(1, client->cbs_id);

	cb = (struct client_cb){
		.id = client->cbs_id,
		.cb_fn = cb_fn,
		.cb_data = cb_data,
		.free_fn = free_fn,
	};

	client->cbs[i] = g_slice_copy(sizeof(cb), &cb);

	qev_unlock(client);

	qev_stats_counter_inc(_stat_callbacks_total);
	qev_stats_counter_inc(_stat_callbacks_created);

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

	if (slot >= G_N_ELEMENTS(client->cbs)) {
		goto error;
	}

	qev_lock(client);

	if (client->cbs[slot] != NULL && client->cbs[slot]->id == id) {
		cb = *client->cbs[slot];
		_cb_remove(client, slot);
	}

	qev_unlock(client);

	if (cb.cb_fn == NULL) {
		goto error;
	}

	status = cb.cb_fn(client, cb.cb_data, client_cb, json);
	_cb_data_free(cb.cb_data, cb.free_fn);

	qev_stats_counter_inc(_stat_callbacks_fired);

	return status;

error:
	evs_err_cb(client, client_cb, CODE_NOT_FOUND,
				"callback doesn't exist", NULL);
	return EVS_STATUS_HANDLED;
}

gboolean client_sub_active(struct client *client, struct subscription *sub)
{
	gboolean active;
	struct client_sub *csub;

	qev_lock(client);

	csub = _sub_get(client, sub);
	active = csub != NULL && !csub->pending && !csub->tombstone;

	qev_unlock(client);

	return active;
}

enum client_sub_state client_sub_add(
	struct client *client,
	struct subscription *sub)
{
	enum client_sub_state sstate;
	struct client_sub *csub = NULL;

	qev_lock(client);

	csub = _sub_get(client, sub);
	if (csub != NULL) {
		csub->tombstone = FALSE;
		if (csub->pending) {
			sstate = CLIENT_SUB_PENDING;
		} else {
			sstate = CLIENT_SUB_ACTIVE;
		}
		goto out;
	}

	csub = _sub_alloc(client);
	if (csub == NULL) {
		sstate = CLIENT_SUB_NULL;
		goto out;
	}

	if (client->subs == NULL) {
		client->subs = g_hash_table_new(NULL, NULL);
	}

	csub->pending = TRUE;
	sub_ref(sub);
	g_hash_table_insert(client->subs, sub, csub);
	sstate = CLIENT_SUB_CREATED;

out:
	qev_unlock(client);
	return sstate;
}

enum client_sub_state client_sub_accept(
	struct client *client,
	struct subscription *sub)
{
	gboolean ok;
	enum client_sub_state sstate = CLIENT_SUB_NULL;
	struct client_sub *csub = NULL;

	qev_lock(client);

	csub = _sub_get(client, sub);

	// Possible if the client closed before the callback came through
	if (csub == NULL) {
		goto out;
	}

	ok = QEV_MOCK(gboolean, qev_list_try_add, sub->subscribers,
							client, &csub->idx);
	if (!ok) {
		goto cleanup;
	} else {
		csub->pending = FALSE;
		ok = csub->tombstone == FALSE;
		if (ok) {
			sstate = CLIENT_SUB_ACTIVE;
		} else {
			sstate = CLIENT_SUB_TOMBSTONED;
			goto cleanup;
		}
	}

out:
	qev_unlock(client);
	return sstate;

cleanup:
	_sub_cleanup(client, sub, csub);
	goto out;
}

void client_sub_reject(
	struct client *client,
	struct subscription *sub)
{
	_sub_remove(client, sub, TRUE);
}

gboolean client_sub_remove(
	struct client *client,
	struct subscription *sub)
{
	return _sub_remove(client, sub, FALSE);
}

void client_close(struct client *client)
{
	qev_lock(client);

	_cb_remove_all(client);
	_sub_remove_all(client);

	qev_unlock(client);
}

void client_init()
{
	_stat_subs_total = qev_stats_counter("clients.subs", "total", FALSE);
	_stat_subs_added = qev_stats_counter("clients.subs", "added", TRUE);
	_stat_subs_removed = qev_stats_counter("clients.subs", "removed", TRUE);

	_stat_callbacks_total = qev_stats_counter("clients.callbacks",
									"total", FALSE);
	_stat_callbacks_created = qev_stats_counter("clients.callbacks",
									"created", TRUE);
	_stat_callbacks_fired = qev_stats_counter("clients.callbacks",
									"fired", TRUE);
	_stat_callbacks_evicted = qev_stats_counter("clients.callbacks",
									"evicted", TRUE);
}
