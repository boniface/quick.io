/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_protocol_util_parse_headers_0)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"   	key: 						value \n\r"
		"     \r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 1);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "key"), "value ");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_1)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"   	key: 						value \n\r"
		"   	key: 						value \n\r"
		"   	key: 						value \n\r"
		"   	key: 						value \n\r"
		"   	key: 						value \n\r"
		"   	key: 						value \n\r"
		"   	key2: 						me         		 \n\r"
		"     \r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 2);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "key"), "value ");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "key2"), "me         		 ");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_2)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"     \r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_3)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff, "lol \n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_4)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff, "lol");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_5)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r\n"
		"null: \0localhost\r\n\r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_6)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r\n"
		"one: \1localhost\r\n\r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 1);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "one"), "\1localhost");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_7)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\n"
		"Host: localhost\n"
		"Sec-WebSocket-Key: key\n"
		"Sec-WebSocket-Protocol: quickio\n"
		"Upgrade: websocket\n"
		"Connection: Upgrade\n"
		"Sec-WebSocket-Version: 13\n\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 6);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "connection"), "Upgrade");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "host"), "localhost");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-key"), "key");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_8)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r"
		"Host: localhost\r"
		"Sec-WebSocket-Key: key\r"
		"Sec-WebSocket-Protocol: quickio\r"
		"Upgrade: websocket\r"
		"Connection: Upgrade\r"
		"Sec-WebSocket-Version: 13\r\r");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 6);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "connection"), "Upgrade");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "host"), "localhost");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-key"), "key");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_parse_headers_9)
{
	GHashTable *tbl;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: key\r\n"
		"Origin: http://someplace.com\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Cookie: \r\n"
		"User-Agent: Mozilla/5.0 and other stuff\r\n"
		"Sec-WebSocket-Protocol: quickio\r\n");

	tbl = protocol_util_parse_headers(buff);

	ck_assert_uint_eq(g_hash_table_size(tbl), 9);
	ck_assert_str_eq(g_hash_table_lookup(tbl, "connection"), "Upgrade");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "host"), "localhost");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "origin"), "http://someplace.com");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-key"), "key");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "sec-websocket-version"), "13");
	ck_assert_str_eq(g_hash_table_lookup(tbl, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_util", &sr, &s);

	tcase = tcase_create("Parse Headers");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_protocol_util_parse_headers_0);
	tcase_add_test(tcase, test_protocol_util_parse_headers_1);
	tcase_add_test(tcase, test_protocol_util_parse_headers_2);
	tcase_add_test(tcase, test_protocol_util_parse_headers_3);
	tcase_add_test(tcase, test_protocol_util_parse_headers_4);
	tcase_add_test(tcase, test_protocol_util_parse_headers_5);
	tcase_add_test(tcase, test_protocol_util_parse_headers_6);
	tcase_add_test(tcase, test_protocol_util_parse_headers_7);
	tcase_add_test(tcase, test_protocol_util_parse_headers_8);
	tcase_add_test(tcase, test_protocol_util_parse_headers_9);

	return test_do(sr);
}
