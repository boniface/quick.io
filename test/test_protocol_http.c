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

#define CONT_HEADERS \
	"POST /?sid=" SID " HTTP/1.1\r\n" \
	"Content-Length: %lu\r\n\r\n" \
	"%s"

#define POLL_HEADERS \
	"POST /poll?sid=" SID " HTTP/1.1\r\n" \
	"Content-Length: 0\r\n\r\n"

#define _poll_hc() _poll(&_hc)
#define _send_hc(...) _send(&_hc, ##__VA_ARGS__)
#define _recv_hc(...) _recv(&_hc, ##__VA_ARGS__)

struct httpc {
	// Control socket
	qev_fd_t cont;

	// Polling socket
	qev_fd_t poll;

	// If the polling socket has already been set to poll
	gboolean is_polling;
};

static struct httpc _hc;

static void _httpc_close(struct httpc *hc)
{
	close(hc->cont);
	hc->cont = -1;

	close(hc->poll);
	hc->poll = -1;

	hc->is_polling = FALSE;
}

static void _httpc_init(struct httpc *hc)
{
	_httpc_close(hc);
	hc->cont = test_socket();
	hc->poll = test_socket();
	hc->is_polling = FALSE;
}

static void _setup()
{
	test_setup();
	_httpc_init(&_hc);
}

static void _teardown()
{
	_httpc_close(&_hc);
	test_teardown();
}

static void _poll(struct httpc *hc)
{
	gint err;

	if (!hc->is_polling) {
		err = send(hc->poll, POLL_HEADERS, strlen(POLL_HEADERS), 0);
		ck_assert_uint_eq(err, strlen(POLL_HEADERS));
	}
}

static void _drain(qev_fd_t sock, GString *into)
{
	gint err;

	qev_buffer_ensure(into, 2048);

	while (TRUE) {
		gssize to_read = into->allocated_len - into->len;
		err = recv(sock, into->str + into->len, to_read, 0);

		if (err > 0) {
			g_string_set_size(into, into->len + err);
		}

		if (err != to_read) {
			return;
		}
	}
}

static void _send(struct httpc *hc, gchar *msg, const gchar *code)
{
	gint err;
	GString *buff = qev_buffer_get();
	GString *resp = qev_buffer_get();

	g_string_printf(buff, CONT_HEADERS, strlen(msg), msg);

	err = send(hc->cont, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	qev_buffer_clear(buff);
	_drain(hc->cont, buff);

	g_string_printf(resp, "HTTP/1.1 %s", code);
	ck_assert(g_str_has_prefix(buff->str, resp->str));

	qev_buffer_put(buff);
	qev_buffer_put(resp);
}

static void _recv(struct httpc *hc, gchar *msg)
{
	gsize len;
	gchar *cl;
	gchar *body;
	struct protocol_headers headers;
	GString *buff = qev_buffer_get();

	_poll(hc);
	_drain(hc->poll, buff);

	body = strstr(buff->str, "\r\n\r\n");
	ck_assert(body != NULL);
	body[3] = '\0';
	body += 4;

	protocol_util_headers(buff->str, &headers);
	cl = protocol_util_headers_get(&headers, "Content-Length");
	len = g_ascii_strtoull(cl, NULL, 10);

	ck_assert_uint_eq(strlen(msg), len);
	ck_assert_str_eq(body, msg);
}

START_TEST(test_http_sane)
{
	_send_hc("/qio/on:1=\"/test\"\n", "200");
	_recv_hc("/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}\n");

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
	tcase_add_checked_fixture(tcase, _setup, _teardown);
	tcase_add_test(tcase, test_http_sane);

	return test_do(sr);
}
