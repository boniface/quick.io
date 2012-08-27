#include "test.h"

static void _apps_setup() {
	
}

START_TEST(test_apps_register) {
}
END_TEST

Suite* apps_suite() {
	TCase *tc;
	Suite *s = suite_create("Apps");
	
	tc = tcase_create("Register");
	tcase_add_checked_fixture(tc, _apps_setup, NULL);
	tcase_add_test(tc, test_apps_register);
	suite_add_tcase(s, tc);
	
	return s;
}