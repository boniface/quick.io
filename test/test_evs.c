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
	test_client_t *tc = test_client(FALSE);

	test_send(tc, "/qio/sub:1=\"/test\"", 0);
	test_recv(tc, buff, sizeof(buff));

	test_close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tc;
	test_new("evs", &sr, &s);

	tc = test_add(s, "Sanity",
		test_sane,
		NULL);

	return test_do(sr);
}
