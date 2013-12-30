/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

static gboolean _do_heartbeat = FALSE;

static void _get_client_cb(struct client *client, void *ptr_)
{
	struct client **ptr = ptr_;
	if (*ptr == NULL) {
		*ptr = client;
	}
}

static void _get_client(struct client **client)
{
	qev_foreach(_get_client_cb, 1, client);
}

/**
 * Heartbeats need to run from the QEV event loop: qev_foreach() only runs
 * unlocked from QEV threads, and since we need to test removal of clients from
 * heartbeats, we have to run there.
 */
static void _heartbeat_timer()
{
	if (_do_heartbeat) {
		protocols_heartbeat();
		_do_heartbeat = FALSE;
	}
}

static void _heartbeat()
{
	_do_heartbeat = TRUE;
	QEV_WAIT_FOR(_do_heartbeat == FALSE);
}

START_TEST(test_handshake_slow)
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

START_TEST(test_route_slow)
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

START_TEST(test_route_invalid)
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

START_TEST(test_heartbeat)
{
	struct client *client = NULL;
	qev_fd_t tc = test_client();

	qev_timer(_heartbeat_timer, 0, 1);
	_get_client(&client);

	_heartbeat();
	test_msg(tc, "/qio/heartbeat:0=null");

	_heartbeat();
	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(51);

	_heartbeat();
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(70);
	_heartbeat();
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	_heartbeat();
	test_msg(tc, "/qio/heartbeat:1=null");

	client->last_recv = qev_monotonic - QEV_SEC_TO_USEC(60 * 17);
	_heartbeat();

	test_client_dead(tc);
	close(tc);
}
END_TEST

START_TEST(test_heartbeat_challenge)
{
	struct client *client = NULL;
	qev_fd_t tc = test_client();

	qev_timer(_heartbeat_timer, 0, 1);
	_get_client(&client);

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	_heartbeat();
	test_msg(tc, "/qio/heartbeat:1=null");
	test_send(tc, "/qio/callback/1:0=null");
	test_ping(tc);

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
	tcase_add_test(tcase, test_handshake_slow);
	tcase_add_test(tcase, test_route_slow);
	tcase_add_test(tcase, test_route_invalid);
	tcase_add_test(tcase, test_heartbeat);
	tcase_add_test(tcase, test_heartbeat_challenge);

	return test_do(sr);
}
