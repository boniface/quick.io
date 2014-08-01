/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

static GString *_buff = NULL;

static gboolean _evs_clean_typical(void *nothing G_GNUC_UNUSED)
{
	g_string_append(_buff, "/qio/ping/test");
	evs_clean_path(_buff->str);
	qev_buffer_clear(_buff);

	return TRUE;
}

static gboolean _evs_clean_wild(void *nothing G_GNUC_UNUSED)
{
	g_string_append(_buff, "///////???qio//////**ping***&^^$#@///////test");
	evs_clean_path(_buff->str);
	qev_buffer_clear(_buff);

	return TRUE;
}

int main()
{
	_buff = qev_buffer_get();

	qev_bench_t *bench = qev_bench_new("evs", "bench_evs.xml");
	qev_bench_fn_for(bench, "evs_clean_typical", _evs_clean_typical, NULL, 100);
	qev_bench_fn_for(bench, "evs_clean_wild", _evs_clean_wild, NULL, 100);
	qev_bench_free(bench);

	qev_buffer_put0(&_buff);

	return 0;
}
