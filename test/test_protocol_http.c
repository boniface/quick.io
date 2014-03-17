/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

#define SID "550e8400-e29b-41d4-a716-446655440000"

// START_TEST(test_rfc6455_handshake_http_invalid_headers)
// {
// 	const gchar *headers =
// 		"GET / HTTP/1.1\r\n"
// 		"herp derp\r\n\r\n";

// 	gint err;
// 	gchar buff[1024];
// 	qev_fd_t ts = test_socket();

// 	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

// 	err = recv(ts, buff, sizeof(buff), 0);
// 	buff[err] = '\0';

// 	ck_assert_str_eq(buff,
// 		"HTTP/1.1 426 Upgrade Required\r\n"
// 		"Connection: keep-alive\r\n"
// 		"Keep-Alive: timeout=60\r\n"
// 		"Cache-Control: no-cache, no-store, must-revalidate\r\n"
// 		"Pragma: no-cache\r\n"
// 		"Expires: 0\r\n\r\n");

// 	test_client_dead(ts);
// 	close(ts);
// }
// END_TEST

START_TEST(test_http_sane)
{
	qev_fd_t ts = test_socket();

	gchar buff[128];
	const gchar *headers =
		"POST /?sid=" SID " HTTP/1.1\r\n"
		"Content-Length: 18\r\n\r\n"
		"/qio/on:1=\"/test\"\n";

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));
	recv(ts, buff, sizeof(buff), 0);

	close(ts);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_http", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_http_sane);

	return test_do(sr);
}
