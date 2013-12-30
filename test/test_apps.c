/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_sane)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/test/good:1={\"key\": \"value\"}",
		"/qio/callback/1:0={\"code\":200,\"data\":\"test\"}");

	test_cb(tc,
		"/test/good:2=",
		"/qio/callback/2:0={\"code\":400,\"data\":null,\"err_msg\":\"Error with \\\"key\\\" in json.\"}");

	close(tc);
}
END_TEST

START_TEST(test_on)
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
		"/qio/callback/100:0={\"code\":200,\"data\":[2,1]}");

	close(tc);
}
END_TEST

START_TEST(test_on_delayed)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/delayed\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_on_reject)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/reject\"",
		"/qio/callback/2:0={\"code\":401,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_on_delayed_reject)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/delayed-reject\"",
		"/qio/callback/2:0={\"code\":401,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_on_with_evs_send)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:2=\"/test/with-send\"",
		"/qio/callback/2:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("apps", &sr, &s);

	tcase = tcase_create("Sanity");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_sane);
	tcase_add_test(tcase, test_on);
	tcase_add_test(tcase, test_on_delayed);
	tcase_add_test(tcase, test_on_reject);
	tcase_add_test(tcase, test_on_delayed_reject);
	tcase_add_test(tcase, test_on_with_evs_send);

	return test_do(sr);
}
