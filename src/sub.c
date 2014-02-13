/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static qev_stats_counter_t *_stat_total;
static qev_stats_counter_t *_stat_added;
static qev_stats_counter_t *_stat_removed;

static gboolean _get_if_exists(
	struct event *ev,
	const gchar *ev_extra,
	struct subscription **sub_)
{
	struct subscription *sub;

	sub = g_hash_table_lookup(ev->subs, ev_extra);
	if (sub != NULL && __sync_add_and_fetch(&sub->refs, 1) <= 1) {
		/*
		 * This is safe to set without atomics: since a lock is held,
		 * we can be sure that the memory isn't going to be freed, and since
		 * the ref was 0, we don't need to be atomic when setting it back to
		 * 0 since no one has a reference to it anymore.
		 */
		sub->refs = 0;
		sub = NULL;
	}

	*sub_ = sub;

	return sub != NULL;
}

struct subscription* sub_get(struct event *ev, const gchar *ev_extra)
{
	struct subscription *sub;

	if (ev_extra == NULL) {
		ev_extra = "";
	}

	// @todo should this use qev_lock? bench it out
	g_rw_lock_reader_lock(&ev->subs_lock);

	_get_if_exists(ev, ev_extra, &sub);

	g_rw_lock_reader_unlock(&ev->subs_lock);

	if (sub == NULL) {
		g_rw_lock_writer_lock(&ev->subs_lock);

		if (!_get_if_exists(ev, ev_extra, &sub)) {
			sub = g_slice_alloc0(sizeof(*sub));
			sub->ev = ev;
			sub->ev_extra = g_strdup(ev_extra);
			sub->subscribers = qev_list_new_resizable(cfg_sub_min_size,
											qev_cfg_get_max_clients(), NULL);
			sub->refs = 1;

			g_hash_table_replace(ev->subs, sub->ev_extra, sub);

			qev_stats_counter_inc(_stat_total);
			qev_stats_counter_inc(_stat_added);
		}

		g_rw_lock_writer_unlock(&ev->subs_lock);
	}

	return sub;
}

struct subscription* sub_ref(struct subscription *sub)
{
	__sync_add_and_fetch(&sub->refs, 1);
	return sub;
}

void sub_unref(struct subscription *sub)
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
		g_hash_table_remove(ev->subs, sub->ev_extra);
	}

	g_rw_lock_writer_unlock(&ev->subs_lock);

	g_free(sub->ev_extra);
	qev_list_free(sub->subscribers);
	g_slice_free1(sizeof(*sub), sub);

	qev_stats_counter_dec(_stat_total);
	qev_stats_counter_inc(_stat_removed);
}

void sub_init()
{
	_stat_total = qev_stats_counter("subs", "total", FALSE);
	_stat_added = qev_stats_counter("subs", "added", TRUE);
	_stat_removed = qev_stats_counter("subs", "removed", TRUE);
}
