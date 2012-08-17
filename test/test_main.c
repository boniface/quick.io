#include <unistd.h>
#include <stdio.h>

#include "test.h"

static pid_t _server;

void _main_setup() {
	u_main_setup(&_server);
}

void _main_teardown() {
	u_main_teardown(_server);
}

START_TEST(test_main_init) {
	// Yep, that's pretty much it, just make sure everything inits
}
END_TEST

Suite* main_suite() {
	TCase *tc;
	Suite *s = suite_create("Option");
	
	tc = tcase_create("Clean Init");
	tcase_add_checked_fixture(tc, _main_setup, _main_teardown);
	tcase_add_test(tc, test_main_init);
	suite_add_tcase(s, tc);
	
	return s;
}