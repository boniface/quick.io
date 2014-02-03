/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

static gboolean _decode(void *buff)
{
	struct client client = {
		.qev_client = {
			.rbuff = buff
		},
	};

	g_string_assign(buff, "\x81\x8c""abcd""N""\x16""\x06""\x17""\x15""X""S""Y""\x0f""\x17""\x0f""\x08");

	protocol_rfc6455_route(&client);
	return TRUE;
}

int main()
{
	qev_bench_t *bench;
	GString *buff = qev_buffer_get();
	bench = qev_bench_new("protocol_rfc6455", "bench_protocol_rfc6455.xml");

	test_config();
	test_setup();

	qev_bench_fn(bench, "decode", _decode, buff, 1000000);

	qev_bench_free(bench);

	qev_buffer_put(buff);

	test_teardown();

	return 0;
}
