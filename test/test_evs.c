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
	gchar buff[128];
	guint64 len;
	test_client_t *tc = test_client();

	test_send(tc, "/qio/on:1=\"/test/good\"", 0);
	len = test_recv(tc, buff, sizeof(buff));
	buff[len] = '\0';

	ck_assert_str_eq("/qio/callback/1:0={\"code\":200,\"data\":null}", buff);

	test_close(tc);
}
END_TEST

START_TEST(test_subscribe_invalid)
{
	gchar buff[128];
	guint64 len;
	test_client_t *tc = test_client();

	test_send(tc, "/qio/on:1=\"/test/nonexistent\"", 0);
	len = test_recv(tc, buff, sizeof(buff));
	buff[len] = '\0';

	ck_assert_str_eq("/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}", buff);

	test_close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("evs", &sr, &s);

	tcase = test_add(s, "Sanity",
		test_subscribe_valid,
		test_subscribe_invalid,
		NULL);

	return test_do(sr);
}
