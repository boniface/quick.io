/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

START_TEST(test_coverage_udp)
{
	qev_on_udp(NULL, 0, NULL);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("coverage", &sr, &s);

	tcase = tcase_create("Coverage");
	suite_add_tcase(s, tcase);
	tcase_add_test(tcase, test_coverage_udp);

	return test_do(sr);
}
