/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

#define HEADERS \
	"GET / HTTP/1.1\r\n" \
	"Host: localhost\r\n" \
	"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

#define HEADERS_RESPONSE \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Cache-Control: private, max-age=0\r\n" \
	"Expires: -1\r\n" \
	"Sec-WebSocket-Accept: Nf+/kB4wxkn+6EPeanngB3VZNwU=\r\n\r\n"

#define QIO_HANDSHAKE "\x81\x89""abcd""N""\x13""\n""\x0b""N""\r""\x0b""\x05""\x08"
#define HANDSHAKE_RESPONSE "\x81\x09/qio/ohai"

static const gchar *_ping = "\x81\x90""abcd""N""\x13""\n""\x0b""N""\x12""\n""\n"
							"\x06""X""R""Y""\x0f""\x17""\xf""\x08";
static const gchar *_ping_response = "\x81\x2a""/qio/callback/1:0="
									"{\"code\":200,\"data\":null}";

static qev_fd_t _client()
{
	gint err;
	gchar buff[1024];

	qev_fd_t ts = test_socket();
	ck_assert_int_eq(send(ts, HEADERS, sizeof(HEADERS) - 1, 0),
					sizeof(HEADERS) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HEADERS_RESPONSE, buff);

	ck_assert_int_eq(send(ts, QIO_HANDSHAKE, sizeof(QIO_HANDSHAKE) - 1, 0),
					sizeof(QIO_HANDSHAKE) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HANDSHAKE_RESPONSE, buff);

	return ts;
}

static void _test_ping_response(const qev_fd_t tc)
{
	gint err;
	gchar buff[strlen(_ping_response) + 1];

	err = recv(tc, buff, strlen(_ping_response), 0);
	ck_assert_int_eq(err, strlen(_ping_response));

	buff[err] = '\0';
	ck_assert_str_eq(buff, _ping_response);
}

static void _test_ping(const qev_fd_t tc)
{
	gint err;

	err = send(tc, _ping, strlen(_ping), 0);
	ck_assert_int_eq(err, strlen(_ping));

	_test_ping_response(tc);
}

START_TEST(test_rfc6455_sane)
{
	qev_fd_t tc = _client();

	_test_ping(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_partial_write)
{
	gint err;
	gchar buff[128];
	qev_fd_t tc = _client();
	guint64 wlen = strlen(_ping) - 2;

	err = send(tc, _ping, wlen, MSG_NOSIGNAL);
	ck_assert(err == (gint64)wlen);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x10\x03\xf0""client timeout", err) == 0);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_partial_write_finish)
{
	gint64 err;
	qev_fd_t tc = _client();
	gint64 wlen = strlen(_ping) - 2;

	gint on = 1;
	err = setsockopt(tc, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on));
	ck_assert(err == 0);

	err = send(tc, _ping, wlen, MSG_NOSIGNAL);
	ck_assert(err == wlen);

	err = send(tc, _ping + wlen, 2, MSG_NOSIGNAL);
	ck_assert(err == 2);

	_test_ping_response(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_handshake_http_partial)
{
	const gchar *headers = HEADERS;
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, 3, 0), 3);
	test_wait_for_buff(3);
	ck_assert_int_eq(send(ts, headers + 3, sizeof(HEADERS) - 4, 0),
					sizeof(HEADERS) - 4);

	close(ts);
}
END_TEST

START_TEST(test_rfc6455_handshake_http_no_upgrade_header)
{
	const gchar *headers =
		"GET / HTTP/1.1\r\n"
		"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";

	gint err;
	gchar buff[1024];
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

	err = recv(ts, buff, sizeof(buff), 0);
	buff[err] = '\0';

	ck_assert_str_eq(buff,
		"HTTP/1.0 426 Upgrade Required\r\n"
		"Cache-Control: private, max-age=0\r\n"
		"Expires: -1\r\n"
		"Connection: Keep-Alive\r\n"
		"Keep-Alive: timeout=60\r\n"
		"Content-Length: 0\r\n"
		"Content-Type: text/plain\r\n\r\n");

	close(ts);
}
END_TEST

START_TEST(test_rfc6455_handshake_qio_partial)
{
	gint err;
	gchar buff[512];
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, HEADERS, sizeof(HEADERS) - 1, 0),
					sizeof(HEADERS) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HEADERS_RESPONSE, buff);

	ck_assert_int_eq(send(ts, QIO_HANDSHAKE, 3, 0), 3);
	test_wait_for_buff(3);
	ck_assert_int_eq(send(ts, QIO_HANDSHAKE + 3, sizeof(QIO_HANDSHAKE)-4, 0),
					sizeof(QIO_HANDSHAKE)-4);

	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HANDSHAKE_RESPONSE, buff);

	close(ts);
}
END_TEST

START_TEST(test_rfc6455_handshake_message_before_ohai)
{
	const gchar *inval = "\x81\x82""abcd""tu";

	gint err;
	gchar buff[512];
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, HEADERS, sizeof(HEADERS) - 1, 0),
					sizeof(HEADERS) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HEADERS_RESPONSE, buff);

	ck_assert_int_eq(send(ts, inval, strlen(inval), 0), strlen(inval));

	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x13\x03\xea""invalid handshake", err) == 0);

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_rfc6455_handshake_invalid_prefix)
{
	const gchar *headers = "POST ws://localhost/qio HTTP/1.1\r\n\r\n";
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_rfc6455_heartbeats)
{
	gint err;
	gchar buff[128];
	qev_fd_t tc = _client();
	struct client *client = test_get_client();

	test_heartbeat();

	// Not expecting a heartbeat on open
	_test_ping(tc);

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(51) - QEV_MS_TO_USEC(1000);;
	test_heartbeat();
	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';
	ck_assert_str_eq(buff, "\x81\x15""/qio/heartbeat:0=null");

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(70);
	test_heartbeat();
	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';
	ck_assert_str_eq(buff, "\x81\x15""/qio/heartbeat:0=null");

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	test_heartbeat();
	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';
	ck_assert_str_eq(buff, "\x81\x15""/qio/heartbeat:1=null");

	client->last_recv = qev_monotonic - QEV_SEC_TO_USEC(60 * 17);
	test_heartbeat();
	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\00", err) == 0);
	test_client_dead(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_heartbeat_before_handshake)
{
	gint err;
	gchar buff[1024];
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, HEADERS, sizeof(HEADERS) - 1, 0),
					sizeof(HEADERS) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HEADERS_RESPONSE, buff);

	// Ensure the client doesn't see this
	test_heartbeat();

	ck_assert_int_eq(send(ts, QIO_HANDSHAKE, sizeof(QIO_HANDSHAKE) - 1, 0),
					sizeof(QIO_HANDSHAKE) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HANDSHAKE_RESPONSE, buff);

	close(ts);
}
END_TEST

START_TEST(test_rfc6455_decode)
{
	qev_fd_t tc = _client();

	ck_assert(send(tc, _ping, 1, 0) == 1);
	test_wait_for_buff(1);
	ck_assert_int_eq(send(tc, _ping + 1, strlen(_ping) - 1, 0),
					strlen(_ping) - 1);

	_test_ping_response(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_multiple)
{
	guint i;
	gint err;
	qev_fd_t tc = _client();
	GString *ping = qev_buffer_get();

	g_string_printf(ping, "%s%s", _ping, _ping);

	err = send(tc, ping->str, ping->len, 0);
	ck_assert_int_eq(err, ping->len);

	for (i = 0; i < 2; i++) {
		_test_ping_response(tc);
	}

	qev_buffer_put(ping);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_close)
{
	gint err;
	gchar buff[8];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x88\x80", 2, 0) == 2);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x00", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_continuation_frame)
{
	gint err;
	gchar buff[8];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x80\x8a""abcd", 6, 0) == 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x02\x03\xeb", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_unmasked)
{
	gint err;
	gchar buff[32];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x81\x04""abcd", 6, 0) == 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x17\x03\xea""client must mask data", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_medium)
{
	const gchar *ping = "\x81\xfe\x01""\xae""abcd""N""\x13""\n""\x0b""N"
						"\x12""\n""\n""\x06""X""R""Y";
	const guint ping_len = strlen(ping);

	gchar buff[ping_len + 0x1ae - 12];
	qev_fd_t tc = _client();

	memset(buff, 't' ^ 'a', sizeof(buff));
	memcpy(buff, ping, ping_len);

	ck_assert_int_eq(send(tc, buff, 6, 0), 6);
	test_wait_for_buff(6);
	ck_assert_int_eq(send(tc, buff + 6, sizeof(buff) - 6, 0), sizeof(buff) - 6);

	_test_ping_response(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_long)
{
	const gchar *ping = "\x81\xff\x00\x00\x00\x00\x00\x01\x12\x34""abcd"
						"N""\x13""\n""\x0b""N""\x12""\n""\n""\x06""X""R""Y";
	const guint ping_len = 26;

	gchar buff[ping_len + 0x11234 - 12];
	qev_fd_t tc = _client();

	memset(buff, 't' ^ 'a', sizeof(buff));
	memcpy(buff, ping, ping_len);

	ck_assert_int_eq(send(tc, buff, 6, 0), 6);
	test_wait_for_buff(6);
	ck_assert_int_eq(send(tc, buff + 6, sizeof(buff) - 6, 0), sizeof(buff) - 6);

	_test_ping_response(tc);

	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_long_overflow)
{
	const gchar *msg = "\x81\xff\xff\xff\xff\xff\xff\xff\xff\xff""abcd";

	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, msg, strlen(msg), 0), strlen(msg));

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_invalid_utf8)
{
	const gchar *inval = "\x81\x82""abcd""\xe1""\xe2";

	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, inval, strlen(inval), 0), strlen(inval));

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_decode_invalid_qio_handshake)
{
	const gchar *inval = "\x81\x82""abcd""\xe1""\xe2";

	gint err;
	gchar buff[512];
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, HEADERS, sizeof(HEADERS) - 1, 0),
					sizeof(HEADERS) - 1);
	err = recv(ts, buff, sizeof(buff), 0);
	ck_assert(err > 0);
	buff[err] = '\0';
	ck_assert_str_eq(HEADERS_RESPONSE, buff);

	ck_assert_int_eq(send(ts, inval, strlen(inval), 0), strlen(inval));

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_rfc6455_encode_medium)
{
	gint err;
	gchar buff[512];
	qev_fd_t tc = _client();
	struct client *client = test_get_client();
	GString *json = qev_buffer_get();

	g_string_set_size(json, 256);
	memset(json->str, 'a', json->len);
	evs_send_bruteforce(client, "/test", "/2", "", json->str, NULL, NULL, NULL);

	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';

	ck_assert(memcmp(buff, "\x81\x7e\x01\x0a", 4) == 0);

	g_string_prepend(json, "/test/2:0=");
	ck_assert_str_eq(buff + 4, json->str);

	qev_buffer_put(json);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_encode_long)
{
	gint err;
	gchar buff[0x10020];
	qev_fd_t tc = _client();
	struct client *client = test_get_client();
	GString *json = qev_buffer_get();

	g_string_set_size(json, 0xffff + 1);
	memset(json->str, 'a', json->len);
	evs_send_bruteforce(client, "/test", "/2", "", json->str, NULL, NULL, NULL);

	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';

	ck_assert(memcmp(buff, "\x81\x7f\x00\x00\x00\x00\x00\x01\x00\x0a", 10) == 0);

	g_string_prepend(json, "/test/2:0=");
	ck_assert(g_strcmp0(buff + 10, json->str) == 0);

	qev_buffer_put(json);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_close_invalid_event_format)
{
	// /test=123
	const gchar *inval = "\x81\x89""abcd""N""\x16""\x06""\x17""\x15""_""R""V""R";

	gint err;
	gchar buff[256];
	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, inval, strlen(inval), 0), strlen(inval));

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x16\x03\xea""invalid event format", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_close_timeout)
{
	const gchar *incomplete = "\x81\x8f""abcd";

	gint err;
	gchar buff[256];
	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, incomplete, strlen(incomplete), 0), strlen(incomplete));

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x10\x03\xf0""client timeout", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_close_read_high)
{
	const gchar *msg = "\x81\xff""\x01\xff\xff\xff\xff\xff\xff\xff""abcd";

	gint err;
	gchar buff[2048];
	union qev_cfg_val val;
	qev_fd_t tc = _client();

	val.ui64 = 100;
	qev_cfg_set("quick-event", "userspace-buff-total", val, NULL);

	ck_assert_int_eq(send(tc, msg, strlen(msg), 0), strlen(msg));

	memset(buff, 0, sizeof(buff));
	do {
		err = send(tc, buff, sizeof(buff), 0);
	} while (err > 0);

	err = recv(tc, buff, sizeof(buff), 0);
	buff[err] = '\0';
	ck_assert(memcmp(buff, "\x88\x02\x03\xf1", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_rfc6455_close_qev_exit)
{
	gint err;
	gchar buff[8];
	qev_fd_t tc = _client();

	qev_exit();

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert(memcmp(buff, "\x88\x02\x03\xe9", err) == 0);

	test_client_dead(tc);
	close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_rfc6455", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_rfc6455_sane);
	tcase_add_test(tcase, test_rfc6455_partial_write);
	tcase_add_test(tcase, test_rfc6455_partial_write_finish);
	tcase_add_test(tcase, test_rfc6455_handshake_http_partial);
	tcase_add_test(tcase, test_rfc6455_handshake_http_no_upgrade_header);
	tcase_add_test(tcase, test_rfc6455_handshake_qio_partial);
	tcase_add_test(tcase, test_rfc6455_handshake_message_before_ohai);
	tcase_add_test(tcase, test_rfc6455_handshake_invalid_prefix);
	tcase_add_test(tcase, test_rfc6455_heartbeats);
	tcase_add_test(tcase, test_rfc6455_heartbeat_before_handshake);

	tcase = tcase_create("Decode");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_rfc6455_decode);
	tcase_add_test(tcase, test_rfc6455_decode_multiple);
	tcase_add_test(tcase, test_rfc6455_decode_close);
	tcase_add_test(tcase, test_rfc6455_decode_continuation_frame);
	tcase_add_test(tcase, test_rfc6455_decode_unmasked);
	tcase_add_test(tcase, test_rfc6455_decode_medium);
	tcase_add_test(tcase, test_rfc6455_decode_long);
	tcase_add_test(tcase, test_rfc6455_decode_long_overflow);
	tcase_add_test(tcase, test_rfc6455_decode_invalid_utf8);
	tcase_add_test(tcase, test_rfc6455_decode_invalid_qio_handshake);

	tcase = tcase_create("Encode");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_rfc6455_encode_medium);
	tcase_add_test(tcase, test_rfc6455_encode_long);

	tcase = tcase_create("Close Reasons");
	suite_add_tcase(s, tcase);
	tcase_set_timeout(tcase, 10);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_rfc6455_close_invalid_event_format);
	tcase_add_test(tcase, test_rfc6455_close_timeout);
	tcase_add_test(tcase, test_rfc6455_close_read_high);
	tcase_add_test(tcase, test_rfc6455_close_qev_exit);

	return test_do(sr);
}
