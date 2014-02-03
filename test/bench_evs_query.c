/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

/**
 * Essentially: [^_\-/a-zA-Z0-9]
 */
static const gchar _allowed_chars[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	1,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,
	0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  0,  0,  0,  0,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,
};

static GHashTable *_tbl = NULL;
static GPtrArray *_eps = NULL;
static GMutex _lock;

static GString* _get_rand_path()
{
	guint i;
	GString *path = qev_buffer_get();

	g_string_append_c(path, '/');

	for (i = 0; i < 20; i++) {
		gchar c = '*';

		while (!_allowed_chars[(guchar)c]) {
			c = g_random_int_range('-', 'z');
		}

		g_string_append_c(path, c);

		if (i % 6 == 5) {
			g_string_append_c(path, '/');
		}
	}

	return path;
}

static gboolean _ptrie_lookup(void *nothing G_GNUC_UNUSED)
{
	gchar *extra;
	gchar *event_path = g_ptr_array_index(_eps, g_random_int_range(0, _eps->len));
	return evs_query(event_path, &extra) != NULL;
}

static gboolean _hash_lookup(void *nothing G_GNUC_UNUSED)
{
	guint i;
	gboolean found;
	gchar *event_path = g_ptr_array_index(_eps, g_random_int_range(0, _eps->len));

	for (i = 0; i < 2; i++) {
		g_mutex_lock(&_lock);

		found = g_hash_table_lookup(_tbl, event_path) != NULL;

		g_mutex_unlock(&_lock);
	}

	return found;
}

int main()
{
	guint i;
	qev_bench_t *bench;
	bench = qev_bench_new("evs_query", "bench_evs_query.xml");

	qev_init(NULL, 0);
	qev_run();

	_tbl = g_hash_table_new(g_str_hash, g_str_equal);
	_eps = g_ptr_array_new();

	// @todo Size affects how fast the ptrie lookup is
	for (i = 0; i < 10000; i++) {
		GString *path = _get_rand_path();
		gchar *event_path = path->str;
		*(path->str + path->len - 3) = '\0';
		g_string_free(path, FALSE);
		g_ptr_array_add(_eps, event_path);
	}

	for (i = 0; i < _eps->len; i++) {
		gchar *event_path = g_ptr_array_index(_eps, i);
		evs_query_insert(event_path, NULL, NULL, NULL, FALSE);
	}

	for (i = 0; i < _eps->len; i++) {
		gchar *event_path = g_ptr_array_index(_eps, i);
		g_hash_table_insert(_tbl, event_path, event_path);
	}

	qev_bench_fn(bench, "ptrie_lookup", _ptrie_lookup, NULL, 1000000);
	qev_bench_fn(bench, "hash_lookup", _hash_lookup, NULL, 1000000);

	qev_bench_free(bench);
	qev_exit();
	return 0;
}
