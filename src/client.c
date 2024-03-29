/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

static qev_fair_t *_fair_subs;

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
	struct client_sub *csub = NULL;
	guint client_used = client->subs == NULL ? 0 : g_hash_table_size(client->subs);

	if (qev_fair_use(_fair_subs, client_used, 1)) {
		qev_stats_counter_inc(_stat_subs_added);

		csub = g_slice_alloc0(sizeof(*csub));
	}

	return csub;
}

static void _sub_free(struct client_sub *csub)
{
	qev_fair_return(_fair_subs, 1);
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
	struct client_sub *csub,
	const gboolean cleanup_hashtable)
{
	qev_list_remove(sub->subscribers, &csub->idx);

	evs_client_off(client, sub);

	_sub_free(csub);
	sub_unref(sub);

	if (cleanup_hashtable) {
		g_hash_table_remove(client->subs, sub);
		if (g_hash_table_size(client->subs) == 0) {
			g_hash_table_unref(client->subs);
			client->subs = NULL;
		}
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
		_sub_cleanup(client, sub, csub, TRUE);
	}

out:
	qev_unlock(client);

	return removed;
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
		.created = qev_monotonic,
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

void client_cb_prune(struct client *client, const gint64 before)
{
	guint i;
	gboolean has_lock = FALSE;

	for (i = 0; i < G_N_ELEMENTS(client->cbs); i++) {
		if (client->cbs[i] != NULL && !has_lock) {
			has_lock = TRUE;
			qev_lock(client);
		}

		if (client->cbs[i] != NULL && client->cbs[i]->created < before) {
			_cb_free(client, i);
		}
	}

	if (has_lock) {
		qev_unlock(client);
	}
}

gint64 client_cb_prune_get_before()
{
	return qev_monotonic - QEV_SEC_TO_USEC(cfg_clients_cb_max_age);
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

	if (qev_is_closing(client)) {
		sstate = CLIENT_SUB_NULL;
		goto out;
	}

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

	ok = QEV_MOCK(gboolean, qev_list_add,
		sub->subscribers, client, &csub->idx);
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
	_sub_cleanup(client, sub, csub, TRUE);
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

void client_update_subs_config(
	const guint64 total,
	const guint64 pressure,
	const guint64 min)
{
	if (_fair_subs != NULL) {
		qev_fair_set_total(_fair_subs, total);
		qev_fair_set_pressure(_fair_subs, pressure);
		qev_fair_set_min(_fair_subs, min);
	}
}

void client_set(struct client *client, const GQuark key, GVariant *val)
{
	if (val == NULL) {
		WARN("Attempted to set NULL value on client for key \"%s\"",
			g_quark_to_string(key));
		return;
	}

	g_variant_ref_sink(val);

	qev_lock(client);

	if (client->data == NULL) {
		client->data = g_hash_table_new_full(NULL, NULL,
									NULL, (GDestroyNotify)g_variant_unref);
	}

	g_hash_table_insert(client->data, GINT_TO_POINTER(key), val);

	qev_unlock(client);
}

GVariant* client_get(struct client *client, const GQuark key)
{
	GVariant *ret = NULL;

	qev_lock(client);

	if (client->data != NULL) {
		ret = g_hash_table_lookup(client->data, GINT_TO_POINTER(key));
	}

	qev_unlock(client);

	if (ret != NULL) {
		g_variant_ref(ret);
	}

	return ret;
}

gboolean client_has(struct client *client, const GQuark key)
{
	gboolean has = FALSE;

	qev_lock(client);

	if (client->data != NULL) {
		has = g_hash_table_contains(client->data, GINT_TO_POINTER(key));
	}

	qev_unlock(client);

	return has;
}

void client_del(struct client *client, const GQuark key)
{
	GHashTable *tbl = NULL;

	qev_lock(client);

	if (client->data != NULL) {
		g_hash_table_remove(client->data, GINT_TO_POINTER(key));

		if (g_hash_table_size(client->data) == 0) {
			tbl = client->data;
			client->data = NULL;
		}
	}

	qev_unlock(client);

	if (tbl != NULL) {
		g_hash_table_unref(tbl);
	}
}

void client_closed(struct client *client)
{
	GHashTableIter iter;
	struct subscription *sub;
	struct client_sub *csub;

	qev_lock(client);

	if (client->subs != NULL) {
		g_hash_table_iter_init(&iter, client->subs);

		while (g_hash_table_iter_next(&iter, (void**)&sub, (void**)&csub)) {
			g_hash_table_iter_remove(&iter);
			_sub_cleanup(client, sub, csub, FALSE);
		}

		g_hash_table_unref(client->subs);
		client->subs = NULL;
	}

	qev_unlock(client);
}

void client_free_all(struct client *client)
{
	/*
	 * At this point, the client is being freed, so no one can have a reference
	 * to him and everything can be done without locks.
	 */
	guint i;

	for (i = 0; i < G_N_ELEMENTS(client->cbs); i++) {
		if (client->cbs[i] != NULL) {
			_cb_free(client, i);
		}
	}

	if (client->data != NULL) {
		g_hash_table_unref(client->data);
		client->data = NULL;
	}
}

void client_init()
{
	_fair_subs = qev_fair_new("subs",
		cfg_clients_subs_total,
		cfg_clients_subs_pressure,
		cfg_clients_subs_min);

	_stat_subs_added = qev_stats_counter(
		"clients.subs", "added", TRUE,
		"How many subscriptions were added to clients");
	_stat_subs_removed = qev_stats_counter(
		"clients.subs", "removed", TRUE,
		"How many subscriptions were removed from clients");

	_stat_callbacks_total = qev_stats_counter(
		"clients.callbacks", "total", FALSE,
		"How many callbacks exist on the server, awaiting a response");
	_stat_callbacks_created = qev_stats_counter(
		"clients.callbacks", "created", TRUE,
		"How many new callbacks were created");
	_stat_callbacks_fired = qev_stats_counter(
		"clients.callbacks", "fired", TRUE,
		"How many callbacks were triggered by clients");
	_stat_callbacks_evicted = qev_stats_counter(
		"clients.callbacks", "evicted", TRUE,
		"How many callbacks were evicted");

	qev_cleanup_and_null_full((void**)&_fair_subs,
					(qev_free_fn)qev_fair_free, TRUE);
}
