/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"


static gboolean _get_if_exists(
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

struct subscription* sub_get(struct event *ev, const gchar *ev_extra)
{
	struct subscription *sub;

	if (ev_extra == NULL) {
		ev_extra = "";
	}

	g_rw_lock_reader_lock(&ev->subs_lock);

	_get_if_exists(ev, ev_extra, &sub);

	g_rw_lock_reader_unlock(&ev->subs_lock);

	if (sub == NULL) {
		g_rw_lock_writer_lock(&ev->subs_lock);

		if (!_get_if_exists(ev, ev_extra, &sub)) {
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
		g_hash_table_steal(ev->subs, sub->ev_extra);
	}

	g_rw_lock_writer_unlock(&ev->subs_lock);

	g_free(sub->ev_extra);
	qev_list_free(sub->subscribers);
	g_slice_free1(sizeof(*sub), sub);
}
