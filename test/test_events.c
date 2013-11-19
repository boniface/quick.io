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

}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tc;
	test_new("events", &sr, &s);

	tc = test_add(s, "Sanity",
		test_sane,
		NULL);

	return test_do(sr);
}
