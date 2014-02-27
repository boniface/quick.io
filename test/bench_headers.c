/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

#define HEADERS \
	"GET ws://quickio.ihrdev.com/ HTTP/1.1\r\n" \
	"Pragma: no-cache\r\n" \
	"Origin: http://www.iheart.com\r\n" \
	"Host: quickio.ihrdev.com\r\n" \
	"Sec-WebSocket-Key: AEIF10HmVEmUv1oD//EbJw==\r\n" \
	"User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36\r\n" \
	"Upgrade: websocket\r\n" \
	"Sec-WebSocket-Extensions: x-webkit-deflate-frame\r\n" \
	"Cache-Control: no-cache\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

static GString *_buff = NULL;

static gboolean _hash_table(void *nothing G_GNUC_UNUSED)
{
	g_string_assign(_buff, HEADERS);
	GHashTable *headers = protocol_util_parse_headers(_buff);

	g_hash_table_lookup(headers, "Sec-WebSocket-Protocol");
	g_hash_table_lookup(headers, "Sec-WebSocket-Key");
	g_hash_table_lookup(headers, "Sec-WebSocket-Version");

	return TRUE;
}

static gboolean _as_struct(void *nothing G_GNUC_UNUSED)
{
	struct protocol_headers headers;
	g_string_assign(_buff, HEADERS);
	protocol_util_headers(_buff, &headers);

	protocol_util_headers_get(&headers, "Sec-WebSocket-Protocol");
	protocol_util_headers_get(&headers, "Sec-WebSocket-Key");
	protocol_util_headers_get(&headers, "Sec-WebSocket-Version");

	return TRUE;
}

int main()
{
	qev_bench_t *bench;
	bench = qev_bench_new("protocol_util", "bench_protocol_util.xml");

	_buff = qev_buffer_get();
	qev_bench_fn(bench, "hash_table", _hash_table, NULL, 1000000);
	qev_bench_fn(bench, "as_struct", _as_struct, NULL, 1000000);
	qev_buffer_put(_buff);

	qev_bench_free(bench);

	return 0;
}
