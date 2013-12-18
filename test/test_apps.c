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
	gchar buff[128];
	test_client_t *tc = test_client();

	test_send(tc, "/test/good:1=");
	test_recv(tc, buff, sizeof(buff));
	ck_assert_str_eq(buff, "/qio/callback/1:0={\"code\":200,\"data\":\"test\"}");

	test_close(tc);
}
END_TEST

START_TEST(test_subscribe)
{
	gchar buff[128];
	test_client_t *tc = test_client();

	test_send(tc, "/qio/on:1=\"/test/good\"");
	test_recv(tc, buff, sizeof(buff));
	ck_assert_str_eq(buff, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	test_send(tc, "/qio/off:2=\"/test/good\"");
	test_recv(tc, buff, sizeof(buff));
	ck_assert_str_eq(buff, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	test_close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("apps", &sr, &s);

	tcase = test_add(s, "Sanity",
		test_sane,
		test_subscribe,
		NULL);

	return test_do(sr);
}
