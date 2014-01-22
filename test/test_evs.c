/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
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

START_TEST(test_evs_clean_path)
{
	GString *p;

	p = evs_clean_path(NULL, NULL, NULL);
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_clean_path("", "", "");
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_clean_path("/", "/", "/");
	ck_assert_str_eq(p->str, "");
	ck_assert_int_eq(p->len, 0);
	qev_buffer_put(p);

	p = evs_clean_path("/a", "/b", "/c");
	ck_assert_str_eq(p->str, "/a/b/c");
	ck_assert_int_eq(p->len, 6);
	qev_buffer_put(p);

	p = evs_clean_path("", "/b", "/c//////////////");
	ck_assert_str_eq(p->str, "/b/c");
	ck_assert_int_eq(p->len, 4);
	qev_buffer_put(p);
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
		"/qio/callback/2:0={\"code\":401,\"data\":null,\"err_msg\":null}");

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

	test_msg(tc, "/qio/callback/3:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/4:0={\"code\":200,\"data\":null}");

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
		"///////???qio//////**ping///////:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"qio/ping***&^^$#@:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);
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
	tcase_add_test(tcase, test_evs_clean_path);

	tcase = tcase_create("Clients");
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

	tcase = tcase_create("QIO Builtins");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_evs_callback_invalid_id);
	tcase_add_test(tcase, test_evs_on_invalid_ev_path);
	tcase_add_test(tcase, test_evs_off_invalid_ev_path);

	return test_do(sr);
}
