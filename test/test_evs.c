/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_on_off_valid)
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

START_TEST(test_on_already_subscribed)
{
	qev_fd_t tc = test_client();

	test_send(tc, "/qio/on:1=\"/test/delayed\"");
	test_send(tc, "/qio/on:2=\"/test/delayed\"");

	test_msg(tc, "/qio/callback/1:0={\"code\":200,\"data\":null}");
	test_msg(tc, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	test_cb(tc,
		"/qio/off:1=\"/test/delayed\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);
}
END_TEST

START_TEST(test_on_invalid)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/nonexistent\"",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	close(tc);
}
END_TEST

START_TEST(test_off_not_subscribed)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/off:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

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
	tcase_add_test(tcase, test_on_off_valid);
	tcase_add_test(tcase, test_on_already_subscribed);
	tcase_add_test(tcase, test_on_invalid);
	tcase_add_test(tcase, test_off_not_subscribed);

	return test_do(sr);
}
