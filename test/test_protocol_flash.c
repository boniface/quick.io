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
	gint len;
	gchar buff[128];
	qev_fd_t ts = test_socket();

	ck_assert(send(ts, "<policy-file-request/>", 22, MSG_NOSIGNAL) == 22);

	len = recv(ts, buff, sizeof(buff), 0);
	ck_assert(len > 0);
	buff[len] = '\0';

	ck_assert_str_eq(buff,
		"<cross-domain-policy>"
			"<allow-access-from domain=\"*\" to-ports=\"*\" />"
		"</cross-domain-policy>");

	close(ts);
}
END_TEST

START_TEST(test_slow)
{
	gint len;
	gchar buff[128];
	qev_fd_t ts = test_socket();

	ck_assert(send(ts, "<policy-", 8, MSG_NOSIGNAL) == 8);
	test_wait_for_buff(8);
	ck_assert(send(ts, "file-request/>", 14, MSG_NOSIGNAL) == 14);

	len = recv(ts, buff, sizeof(buff), 0);
	ck_assert(len > 0);
	buff[len] = '\0';

	ck_assert_str_eq(buff,
		"<cross-domain-policy>"
			"<allow-access-from domain=\"*\" to-ports=\"*\" />"
		"</cross-domain-policy>");

	close(ts);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_flash", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_sane);
	tcase_add_test(tcase, test_slow);

	return test_do(sr);
}
