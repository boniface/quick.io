/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_subscribe_valid)
{
	test_client_t *tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_close(tc);
}
END_TEST

START_TEST(test_subscribe_invalid)
{
	test_client_t *tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/nonexistent\"",
		"/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");

	test_close(tc);
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
	tcase_add_test(tcase, test_subscribe_valid);
	tcase_add_test(tcase, test_subscribe_invalid);

	return test_do(sr);
}
