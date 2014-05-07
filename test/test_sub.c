/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

START_TEST(test_sub_get_race)
{
	struct subscription *sub2;
	gchar *ev_extra = NULL;
	struct event *ev = evs_query("/test/good", &ev_extra);
	struct subscription *sub = sub_get(ev, NULL);

	/*
	 * Just ensure that, when ref count reaches 0, the subscription
	 * is replaced
	 */
	sub->refs = 0;
	sub2 = sub_get(ev, NULL);

	ck_assert(sub != sub2);
	ck_assert(g_hash_table_lookup(ev->subs, "") == sub2);

	sub->refs = 1;
	sub_unref(sub);
	sub_unref(sub2);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("sub", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_sub_get_race);

	return test_do(sr);
}
