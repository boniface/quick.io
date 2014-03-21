/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_protocol_util_headers_0)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"   	key: 						value \n\r"
		"     \r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 1);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "key"), "value ");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_1)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"   	key: 						value1 \n\r"
		"   	key: 						value2 \n\r"
		"   	key: 						value3 \n\r"
		"   	key: 						value4 \n\r"
		"   	key: 						value5 \n\r"
		"   	key: 						value6 \n\r"
		"   	key2: 						me         		 \n\r"
		"     \r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 7);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "key"), "value1 ");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "key2"), "me         		 ");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_2)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"lol \n"
		"     \r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_3)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff, "lol \n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_4)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff, "lol");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_5)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r\n"
		"null: \0localhost\r\n\r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 0);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_6)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r\n"
		"one: \1localhost\r\n\r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 1);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "one"), "\1localhost");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_7)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\n"
		"Host: localhost\n"
		"Sec-WebSocket-Key: key\n"
		"Sec-WebSocket-Protocol: quickio\n"
		"Upgrade: websocket\n"
		"Connection: Upgrade\n"
		"Sec-WebSocket-Version: 13\n\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 6);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "connection"), "Upgrade");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "host"), "localhost");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-key"), "key");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "13");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "13");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_8)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET ws://localhost HTTP/1.1\r"
		"Host: localhost\r"
		"Sec-WebSocket-Key: key\r"
		"Sec-WebSocket-Protocol: quickio\r"
		"Upgrade: websocket\r"
		"Connection: Upgrade\r"
		"Sec-WebSocket-Version: 13\r\r");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 6);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "connection"), "Upgrade");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "host"), "localhost");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-key"), "key");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "13");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "13");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_9)
{
	struct protocol_headers headers;
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

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 9);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "connection"), "Upgrade");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "host"), "localhost");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "origin"), "http://someplace.com");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-key"), "key");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "13");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "upgrade"), "websocket");

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_protocol_util_headers_10)
{
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	g_string_assign(buff,
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: key\r\n"
		"Origin: 	\r\n"
		"Sec-WebSocket-Version:\r\n"
		"Cookie:\r\n"
		"User-Agent: Mozilla/5.0 and other stuff\r\n"
		"Sec-WebSocket-Protocol: quickio\r\n");

	protocol_util_headers(buff->str, &headers);

	ck_assert_uint_eq(headers.used, 9);
	ck_assert_str_eq(protocol_util_headers_get(&headers, "connection"), "Upgrade");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "host"), "localhost");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "origin"), "");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-key"), "key");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-protocol"), "quickio");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "sec-websocket-version"), "");
	ck_assert_str_eq(protocol_util_headers_get(&headers, "upgrade"), "websocket");

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
	tcase_add_test(tcase, test_protocol_util_headers_0);
	tcase_add_test(tcase, test_protocol_util_headers_1);
	tcase_add_test(tcase, test_protocol_util_headers_2);
	tcase_add_test(tcase, test_protocol_util_headers_3);
	tcase_add_test(tcase, test_protocol_util_headers_4);
	tcase_add_test(tcase, test_protocol_util_headers_5);
	tcase_add_test(tcase, test_protocol_util_headers_6);
	tcase_add_test(tcase, test_protocol_util_headers_7);
	tcase_add_test(tcase, test_protocol_util_headers_8);
	tcase_add_test(tcase, test_protocol_util_headers_9);
	tcase_add_test(tcase, test_protocol_util_headers_10);

	return test_do(sr);
}
