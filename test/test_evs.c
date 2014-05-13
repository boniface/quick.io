/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

START_TEST(test_evs_multiple_add_handler)
{
	struct event *ev;

	ev = evs_add_handler(NULL, "/multiple_add", NULL, NULL, NULL, TRUE);
	ck_assert(ev != NULL);

	ev = evs_add_handler("", "/multiple_add", NULL, NULL, NULL, TRUE);
	ck_assert(ev == NULL);
}
END_TEST

START_TEST(test_evs_root_handler)
{
	struct event *ev;

	ev = evs_add_handler("", "/", NULL, NULL, NULL, TRUE);
	ck_assert(ev == NULL);

	ev = evs_add_handler(NULL, "", NULL, NULL, NULL, TRUE);
	ck_assert(ev == NULL);

	ev = evs_add_handler(NULL, "/t", NULL, NULL, NULL, TRUE);
	ck_assert(ev != NULL);

	ev = evs_add_handler("meow", "/t", NULL, NULL, NULL, TRUE);
	ck_assert(ev != NULL);
}
END_TEST

START_TEST(test_evs_make_path)
{
	GString *p;

	p = evs_make_path(NULL, NULL, NULL);
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_make_path("", "", "");
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_make_path("/", "/", "/");
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_make_path("/a", "/b", "/c");
	ck_assert_str_eq(p->str, "/a/b/c");
	ck_assert_int_eq(p->len, 6);
	qev_buffer_put(p);

	p = evs_make_path("", "/b", "/c//////////////");
	ck_assert_str_eq(p->str, "/b/c");
	ck_assert_int_eq(p->len, 4);
	qev_buffer_put(p);
}
END_TEST

START_TEST(test_evs_query_handles_children_lookups)
{
	gchar *ev_extra = NULL;
	struct event *handles;
	struct event *an;
	struct event *another;
	struct event *specific;
	struct event *found;

 	handles = evs_add_handler("/query-test", NULL,
					NULL, NULL, NULL, TRUE);
 	an = evs_add_handler("/query-test", "/an",
					NULL, NULL, NULL, TRUE);
	another = evs_add_handler("/query-test", "/another",
					NULL, NULL, NULL, FALSE);
	specific = evs_add_handler("/query-test", "/specific",
					NULL, NULL, NULL, FALSE);

	found = evs_query("/query-tes", &ev_extra);
	ck_assert_ptr_eq(NULL, found);

	found = evs_query("/query-test", &ev_extra);
	ck_assert_ptr_eq(handles, found);
	ck_assert_str_eq(ev_extra, "");

	found = evs_query("/query-test/not/setup", &ev_extra);
	ck_assert_ptr_eq(handles, found);
	ck_assert_str_eq(ev_extra, "/not/setup");

	found = evs_query("/query-test/specific/with/children", &ev_extra);
	ck_assert_ptr_eq(handles, found);
	ck_assert_str_eq(ev_extra, "/specific/with/children");

	found = evs_query("/query-test/specific-event", &ev_extra);
	ck_assert_ptr_eq(handles, found);
	ck_assert_str_eq(ev_extra, "/specific-event");

	found = evs_query("/query-test/specific", &ev_extra);
	ck_assert_ptr_eq(specific, found);
	ck_assert_str_eq(ev_extra, "");

	found = evs_query("/query-test/another", &ev_extra);
	ck_assert_ptr_eq(another, found);
	ck_assert_str_eq(ev_extra, "");

	found = evs_query("/query-test/another-test", &ev_extra);
	ck_assert_ptr_eq(NULL, found);

	found = evs_query("/query-test/an", &ev_extra);
	ck_assert_ptr_eq(an, found);
	ck_assert_str_eq(ev_extra, "");

	found = evs_query("/query-test/an/t", &ev_extra);
	ck_assert_ptr_eq(an, found);
	ck_assert_str_eq(ev_extra, "/t");

	found = evs_query("/query-test/a", &ev_extra);
	ck_assert_ptr_eq(NULL, found);
}
END_TEST

START_TEST(test_evs_404)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/i/can/haz/404:1=null",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_off_valid)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/off:2=\"/test/good\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_off_children)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/delayed-childs/child\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/on:2=\"/test/delayed-childs/invalid\"",
		"/qio/callback/2:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	test_cb(tc,
		"/qio/off:3=\"/test/delayed-childs/child\"",
		"/qio/callback/3:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/off:4=\"/test/delayed-childs/invalid\"",
		"/qio/callback/4:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/off:4=\"/test/delayed-childs/i/never/subscribed/to/this\"",
		"/qio/callback/4:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_already_subscribed)
{
	gint err;
	qev_fd_t tc = test_client();

	test_send(tc, "/qio/on:1=\"/test/good\"");
	test_send(tc, "/qio/on:2=\"/test/good\"");

	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	/*
	 * Race condition if not sent all together: possible for the second
	 * subscribe to happen after the first is successful, and its callback
	 * is sent before the first
	 */
	err = send(tc,
		"\x00\x00\x00\x00\x00\x00\x00\x19/qio/on:3=\"/test/delayed\""
		"\x00\x00\x00\x00\x00\x00\x00\x19/qio/on:4=\"/test/delayed\"", 66, MSG_NOSIGNAL);
	ck_assert_int_eq(err, 66);

	test_msg(tc, "/qio/callback/4:0={\"code\":202,\"data\":null,"
					"\"err_msg\":\"subscription pending\"}");
	test_msg(tc, "/qio/callback/3:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/off:5=\"/test/delayed\"",
		"/qio/callback/5:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_invalid)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/nonexistent\"",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_off_not_subscribed)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/off:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_unsubscribed_send)
{
	gchar *extra = NULL;
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();
	struct event *ev = evs_query("/test/delayed-childs", &extra);
	ck_assert(ev != NULL);

	evs_send(client, ev, NULL, "test");

	close(tc);
}
END_TEST

START_TEST(test_evs_send_doesnt_handle_children)
{
	gchar *extra = NULL;
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();
	struct event *ev = evs_query("/test/good", &extra);
	ck_assert(ev != NULL);

	evs_send(client, ev, "/child", "test");

	close(tc);
}
END_TEST

START_TEST(test_evs_malformed_path)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"\334ufff",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	test_cb(tc,
		"///////???qio//////**ping///////:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"qio/ping***&^^$#@:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"///////???qio//////**ping***&^^$#@///////:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_send_sub)
{
	gchar *extra = NULL;
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();
	struct event *ev = evs_query("/test/good", &extra);
	struct subscription *sub = sub_get(ev, extra);

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	evs_send_sub(client, sub, "\"hooray!\"");

	test_msg(tc, "/test/good:0=\"hooray!\"");

	sub_unref(sub);
	close(tc);
}
END_TEST

START_TEST(test_evs_on)
{
	guint i;
	gchar buff[128];
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/on:2=\"/test/good2\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	evs_broadcast_path("/test/good", "\"json!\"");
	evs_broadcast_path("/test/good2", "\"json!\"");
	evs_broadcast_tick();

	for (i = 0; i < 2; i++) {
		/*
		 * No implied ordering for broadcasts
		 */
		test_recv(tc, buff, sizeof(buff));
		ck_assert(g_str_has_prefix(buff, "/test/good"));
		ck_assert(g_str_has_suffix(buff, "\"json!\""));
	}

	test_cb(tc,
		"/qio/off:3=\"/test/good2\"",
		"/qio/callback/3:0={\"code\":200,\"data\":null}");

	for (i = 0; i < 2; i++) {
		evs_broadcast_path("/test/good", "\"json!\"");
		evs_broadcast_path("/test/good2", "\"json!\"");
	}

	evs_broadcast_tick();

	for (i = 0; i < 2; i++) {
		test_msg(tc, "/test/good:0=\"json!\"");
	}

	test_cb(tc,
		"/test/stats:100=",
		"/qio/callback/100:0={\"code\":200,\"data\":[2,1,10,20]}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_empty_broadcast)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	evs_broadcast_path("/test/good", "\"json!\"");
	evs_broadcast_tick();
	test_msg(tc, "/test/good:0=\"json!\"");

	evs_broadcast_path("/test/good", NULL);
	evs_broadcast_tick();
	test_msg(tc, "/test/good:0=null");

	evs_broadcast_path("/test/good", "");
	evs_broadcast_tick();
	test_msg(tc, "/test/good:0=null");
}
END_TEST

START_TEST(test_evs_on_delayed)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/delayed\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_delayed_with_broadcast)
{
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();

	test_send(tc, "/qio/on:2=\"/test/delayed\"");
	QEV_WAIT_FOR(client->subs != NULL && g_hash_table_size(client->subs) > 0);

	evs_broadcast_path("/test/delayed", NULL);
	evs_broadcast_tick();

	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	evs_broadcast_path("/test/delayed", NULL);
	evs_broadcast_tick();
	test_msg(tc, "/test/delayed:0=null");

	test_ping(tc);

	close(tc);
}
END_TEST

START_TEST(test_evs_on_reject)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/reject\"",
		"/qio/callback/2:0={\"code\":401,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_delayed_reject)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/delayed-reject\"",
		"/qio/callback/2:0={\"code\":401,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_with_evs_send)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/with-send\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_off_before_on_cb)
{
	gint err;
	qev_fd_t tc = test_client();

	err = send(tc,
		"\x00\x00\x00\x00\x00\x00\x00\x19/qio/on:1=\"/test/delayed\""
		"\x00\x00\x00\x00\x00\x00\x00\x1a/qio/off:2=\"/test/delayed\"",
		67, MSG_NOSIGNAL);
	ck_assert_int_eq(err, 67);

	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	evs_broadcast_path("/test/delayed", "\"json!\"");
	evs_broadcast_tick();

	test_ping(tc);

	close(tc);
}
END_TEST

START_TEST(test_evs_on_off_on_before_first_on_cb)
{
	gint err;
	qev_fd_t tc = test_client();

	err = send(tc,
		"\x00\x00\x00\x00\x00\x00\x00\x19/qio/on:1=\"/test/delayed\""
		"\x00\x00\x00\x00\x00\x00\x00\x1a/qio/off:2=\"/test/delayed\""
		"\x00\x00\x00\x00\x00\x00\x00\x19/qio/on:3=\"/test/delayed\"",
		100, MSG_NOSIGNAL);
	ck_assert_int_eq(err, 100);

	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/3:0={\"code\":202,\"data\":null,"
					"\"err_msg\":\"subscription pending\"}");
	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	evs_broadcast_path("/test/delayed", "\"json!\"");
	evs_broadcast_tick();

	test_msg(tc, "/test/delayed:0=\"json!\"");
	test_ping(tc);

	close(tc);
}
END_TEST

static gboolean _test_evs_off_after_close_off_called = FALSE;
static void _test_evs_off_after_close_off(
	struct client *client G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED)
{
	_test_evs_off_after_close_off_called = TRUE;
}

START_TEST(test_evs_off_after_close)
{
	qev_fd_t tc;

	evs_add_handler("/test-evs", "/off", NULL, NULL,
		_test_evs_off_after_close_off, FALSE);

 	tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test-evs/off\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);

	QEV_WAIT_FOR(_test_evs_off_after_close_off_called);
}
END_TEST

START_TEST(test_evs_callback_invalid_id)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/callback/nope:1=null",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid server callback id\"}");

	close(tc);
}
END_TEST

START_TEST(test_evs_on_invalid_ev_path)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=null",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/on:1={\"test\":null}",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/on:1=12312",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/on:1=\"wat\\be/that\"",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_evs_off_invalid_ev_path)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/off:1=null",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/off:1={\"test\":null}",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/off:1=12312",
		"/qio/callback/1:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"invalid json ev_path\"}");

	test_cb(tc,
		"/qio/off:1=\"wat\\be/that\"",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("evs", &sr, &s);

	tcase = tcase_create("Sanity");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_evs_multiple_add_handler);
	tcase_add_test(tcase, test_evs_root_handler);
	tcase_add_test(tcase, test_evs_make_path);

	tcase = tcase_create("Query");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_evs_query_handles_children_lookups);

	tcase = tcase_create("Events");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_evs_404);
	tcase_add_test(tcase, test_evs_on_off_valid);
	tcase_add_test(tcase, test_evs_on_off_children);
	tcase_add_test(tcase, test_evs_on_already_subscribed);
	tcase_add_test(tcase, test_evs_on_invalid);
	tcase_add_test(tcase, test_evs_off_not_subscribed);
	tcase_add_test(tcase, test_evs_unsubscribed_send);
	tcase_add_test(tcase, test_evs_send_doesnt_handle_children);
	tcase_add_test(tcase, test_evs_malformed_path);
	tcase_add_test(tcase, test_evs_send_sub);
	tcase_add_test(tcase, test_evs_on);
	tcase_add_test(tcase, test_evs_on_empty_broadcast);
	tcase_add_test(tcase, test_evs_on_delayed);
	tcase_add_test(tcase, test_evs_on_delayed_with_broadcast);
	tcase_add_test(tcase, test_evs_on_reject);
	tcase_add_test(tcase, test_evs_on_delayed_reject);
	tcase_add_test(tcase, test_evs_on_with_evs_send);
	tcase_add_test(tcase, test_evs_off_before_on_cb);
	tcase_add_test(tcase, test_evs_on_off_on_before_first_on_cb);
	tcase_add_test(tcase, test_evs_off_after_close);

	tcase = tcase_create("QIO Builtins");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_evs_callback_invalid_id);
	tcase_add_test(tcase, test_evs_on_invalid_ev_path);
	tcase_add_test(tcase, test_evs_off_invalid_ev_path);

	return test_do(sr);
}
