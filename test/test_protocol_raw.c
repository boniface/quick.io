/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_raw_handshake_slow)
{
	gchar buff[10];
	qev_fd_t ts = test_socket();

	ck_assert(send(ts, "/qio", 4, MSG_NOSIGNAL) == 4);
	test_wait_for_buff(4);
	ck_assert(send(ts, "/ohai", 5, MSG_NOSIGNAL) == 5);

	ck_assert(recv(ts, buff, sizeof(buff), 0) == 9);
	buff[9] = '\0';
	ck_assert_str_eq(buff, "/qio/ohai");

	test_ping(ts);

	close(ts);
}
END_TEST

START_TEST(test_raw_route_slow)
{
	qev_fd_t tc = test_client();

	ck_assert(send(tc, "\x00\x00", 2, 0) == 2);
	test_wait_for_buff(2);
	ck_assert(send(tc, "\x00\x00\x00\x00\x00\x10/qio/ping:1=", 18, 0) == 18);
	test_wait_for_buff(20);
	ck_assert(send(tc, "null", 4, 0) == 4);

	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_ping(tc);

	close(tc);
}
END_TEST

START_TEST(test_raw_route_invalid)
{
	qev_fd_t tc;

	tc = test_client();
	test_send(tc, "lolno");
	test_client_dead(tc);
	close(tc);

	tc = test_client();
	test_send(tc, "/qio/ping:1");
	test_client_dead(tc);
	close(tc);

	tc = test_client();
	test_send(tc, "/qio/ping=null");
	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_raw_minimal_event)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/t:1=",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	test_cb(tc,
		"/test:1=",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	test_cb(tc,
		":1=",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_raw_heartbeats)
{
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();

	test_heartbeat();

	// Not expecting a heartbeat on open
	test_ping(tc);

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(51) - QEV_MS_TO_USEC(1000);
	test_heartbeat();
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(70);
	test_heartbeat();
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	test_heartbeat();
	test_msg(tc, "/qio/heartbeat:1=null");

	client->last_recv = qev_monotonic - QEV_SEC_TO_USEC(60 * 17);
	test_heartbeat();
	test_client_dead(tc);

	close(tc);
}
END_TEST

START_TEST(test_raw_heartbeat_challenge)
{
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	test_heartbeat();
	test_msg(tc, "/qio/heartbeat:1=null");
	test_send(tc, "/qio/callback/1:0=null");
	test_ping(tc);

	close(tc);
}
END_TEST

START_TEST(test_raw_multiple_messages)
{
	gint64 err;
	qev_fd_t tc = test_client();

	err = send(tc,
			"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null"
			"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:2=null", 48, 0);
	ck_assert(err == 48);
	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_raw", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_raw_handshake_slow);
	tcase_add_test(tcase, test_raw_route_slow);
	tcase_add_test(tcase, test_raw_route_invalid);
	tcase_add_test(tcase, test_raw_minimal_event);
	tcase_add_test(tcase, test_raw_heartbeats);
	tcase_add_test(tcase, test_raw_heartbeat_challenge);
	tcase_add_test(tcase, test_raw_multiple_messages);

	return test_do(sr);
}
