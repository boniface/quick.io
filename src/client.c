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
