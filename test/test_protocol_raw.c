/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

static gboolean _heartbeat = FALSE;

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
	if (_heartbeat) {
		protocols_heartbeat();
		_heartbeat = FALSE;
	}
}

START_TEST(test_heartbeat)
{
	struct client *client = NULL;
	test_client_t *tc = test_client();

	qev_timer(_heartbeat_timer, 0, 1);
	_get_client(&client);

	_heartbeat = TRUE;
	test_msg(tc, "/qio/heartbeat:0=null");

	_heartbeat = TRUE;
	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(51);

	_heartbeat = TRUE;
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(70);
	_heartbeat = TRUE;
	test_msg(tc, "/qio/heartbeat:0=null");

	client->last_recv = qev_monotonic - (QEV_SEC_TO_USEC(60 * 15) + 1);
	_heartbeat = TRUE;
	test_msg(tc, "/qio/heartbeat:1=null");

	client->last_recv = qev_monotonic - QEV_SEC_TO_USEC(60 * 17);
	_heartbeat = TRUE;

	test_client_dead(tc);
	test_close(tc);
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
	tcase_add_test(tcase, test_heartbeat);

	return test_do(sr);
}
