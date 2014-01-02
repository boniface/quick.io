/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

#define HEADERS \
 	"GET ws://localhost/qio HTTP/1.1\r\n" \
	"Host: localhost\r\n" \
	"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

#define HEADERS_RESPONSE \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: Nf+/kB4wxkn+6EPeanngB3VZNwU=\r\n" \
	"Cache-Control: no-cache, no-store, must-revalidate\r\n" \
	"Pragma: no-cache\r\n" \
	"Expires: 0\r\n\r\n"

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

START_TEST(test_sane)
{
	gint err;
	gchar buff[128];
	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, _ping, strlen(_ping), 0), strlen(_ping));
	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, strlen(_ping_response));

	buff[err] = '\0';
	ck_assert_str_eq(_ping_response, buff);

	close(tc);
}
END_TEST

START_TEST(test_handshake_slow)
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

START_TEST(test_handshake_invalid_http_headers)
{
	const gchar *headers =
		"GET ws://localhost/qio HTTP/1.1\r\n"
		"herp derp\r\n\r\n";
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_handshake_no_upgrade_header)
{
	const gchar *headers =
		"GET ws://localhost/qio HTTP/1.1\r\n"
		"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_handshake_no_ohai)
{
	const gchar *inval = "\x81\x82""abcd""tu";

	gint err;
	gchar buff[256];
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

START_TEST(test_handshake_invalid_prefix)
{
	const gchar *headers = "POST ws://localhost/qio HTTP/1.1\r\n\r\n";

	qev_fd_t ts = test_socket();

	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

	test_client_dead(ts);
	close(ts);
}
END_TEST

START_TEST(test_decode)
{
	gint err;
	gchar buff[128];
	qev_fd_t tc = _client();

	ck_assert(send(tc, _ping, 1, 0) == 1);
	test_wait_for_buff(1);
	ck_assert_int_eq(send(tc, _ping + 1, strlen(_ping) - 1, 0),
					strlen(_ping) - 1);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, strlen(_ping_response));

	buff[err] = '\0';
	ck_assert_str_eq(_ping_response, buff);

	close(tc);
}
END_TEST

START_TEST(test_decode_close)
{
	gint err;
	gchar buff[8];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x88\x80", 2, 0) == 2);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, 2);

	ck_assert((buff[0] & 0xff) == 0x88);
	ck_assert((buff[1] & 0xff) == 0x00);

	close(tc);
}
END_TEST

START_TEST(test_decode_continuation_frame)
{
	gint err;
	guint16 ecode;
	gchar buff[8];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x80\x8a""abcd", 6, 0) == 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, 4);

	ck_assert((buff[0] & 0xff) == 0x88);
	ck_assert((buff[1] & 0xff) == 0x02);

	ecode = GUINT16_FROM_BE(*((guint16*)(buff + 2)));

	ck_assert_uint_eq(ecode, 1003);

	close(tc);
}
END_TEST

START_TEST(test_decode_unmasked)
{
	gint err;
	guint16 ecode;
	gchar buff[32];
	qev_fd_t tc = _client();

	ck_assert(send(tc, "\x81\x04""abcd", 6, 0) == 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, 25);
	buff[err] = '\0';

	ck_assert((buff[0] & 0xff) == 0x88);
	ck_assert((buff[1] & 0xff) == 0x17);

	ecode = GUINT16_FROM_BE(*((guint16*)(buff + 2)));
	ck_assert_str_eq(buff + 4, "client must mask data");

	ck_assert_uint_eq(ecode, 1002);

	close(tc);
}
END_TEST

START_TEST(test_decode_medium)
{
	const gchar *ping = "\x81\xfe\x01""\xae""abcd""N""\x13""\n""\x0b""N"
						"\x12""\n""\n""\x06""X""R""Y";
	const guint ping_len = strlen(ping);

	gint err;
	gchar buff[ping_len + 0x1ae - 12];
	qev_fd_t tc = _client();

	memset(buff, 't' ^ 'a', sizeof(buff));
	memcpy(buff, ping, ping_len);

	ck_assert_int_eq(send(tc, buff, 6, 0), 6);
	test_wait_for_buff(6);
	ck_assert_int_eq(send(tc, buff + 6, sizeof(buff) - 6, 0), sizeof(buff) - 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, strlen(_ping_response));

	buff[err] = '\0';
	ck_assert_str_eq(_ping_response, buff);

	close(tc);
}
END_TEST

START_TEST(test_decode_long)
{
	const gchar *ping = "\x81\xff\x00\x00\x00\x00\x00\x01\x12\x34""abcd"
						"N""\x13""\n""\x0b""N""\x12""\n""\n""\x06""X""R""Y";
	const guint ping_len = 26;

	gint err;
	gchar buff[ping_len + 0x11234 - 12];
	qev_fd_t tc = _client();

	memset(buff, 't' ^ 'a', sizeof(buff));
	memcpy(buff, ping, ping_len);

	ck_assert_int_eq(send(tc, buff, 6, 0), 6);
	test_wait_for_buff(6);
	ck_assert_int_eq(send(tc, buff + 6, sizeof(buff) - 6, 0), sizeof(buff) - 6);

	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, strlen(_ping_response));

	buff[err] = '\0';
	ck_assert_str_eq(_ping_response, buff);

	close(tc);
}
END_TEST

START_TEST(test_decode_invalid_utf8)
{
	const gchar *inval = "\x81\x82""abcd""\xe1""\xe2";

	qev_fd_t tc = _client();

	ck_assert_int_eq(send(tc, inval, strlen(inval), 0), strlen(inval));

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_decode_invalid_qio_handshake)
{
	const gchar *inval = "\x81\x82""abcd""\xe1""\xe2";

	gint err;
	gchar buff[256];
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

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_rfc6455", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_sane);
	tcase_add_test(tcase, test_handshake_slow);
	tcase_add_test(tcase, test_handshake_invalid_http_headers);
	tcase_add_test(tcase, test_handshake_no_upgrade_header);
	tcase_add_test(tcase, test_handshake_no_ohai);
	tcase_add_test(tcase, test_handshake_invalid_prefix);

	tcase = tcase_create("Decode");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_decode);
	tcase_add_test(tcase, test_decode_close);
	tcase_add_test(tcase, test_decode_continuation_frame);
	tcase_add_test(tcase, test_decode_unmasked);
	tcase_add_test(tcase, test_decode_medium);
	tcase_add_test(tcase, test_decode_long);
	tcase_add_test(tcase, test_decode_invalid_utf8);
	tcase_add_test(tcase, test_decode_invalid_qio_handshake);

	return test_do(sr);
}
