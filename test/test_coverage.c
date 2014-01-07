/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
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
