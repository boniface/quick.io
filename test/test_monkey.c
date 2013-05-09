#include "test.h"

START_TEST(test_monkey_gstr_ends_with_0) {
	GString *s = g_string_new("");
	check_not(g_string_ends_with(s, "really long string"));
	g_string_free(s, TRUE);
}
END_TEST

START_TEST(test_monkey_gstr_ends_with_1) {
	GString *s = g_string_new("really long string");
	check(g_string_ends_with(s, "string"));
	g_string_free(s, TRUE);
}
END_TEST

START_TEST(test_monkey_gstr_ends_with_2) {
	GString *s = g_string_new("long string");
	check(g_string_ends_with(s, "long string"));
	g_string_free(s, TRUE);
}
END_TEST

START_TEST(test_monkey_gstr_ends_with_3) {
	GString *s = g_string_new("long string");
	check(g_string_ends_with(s, ""));
	g_string_free(s, TRUE);
}
END_TEST

START_TEST(test_monkey_gstr_ends_with_4) {
	GString *s = g_string_new("");
	check(g_string_ends_with(s, ""));
	g_string_free(s, TRUE);
}
END_TEST

Suite* monkey_suite() {
	TCase *tc;
	Suite *s = suite_create("Monkey");

	tc = tcase_create("g_string_ends_with");
	tcase_add_test(tc, test_monkey_gstr_ends_with_0);
	tcase_add_test(tc, test_monkey_gstr_ends_with_1);
	tcase_add_test(tc, test_monkey_gstr_ends_with_2);
	tcase_add_test(tc, test_monkey_gstr_ends_with_3);
	tcase_add_test(tc, test_monkey_gstr_ends_with_4);
	suite_add_tcase(s, tc);

	return s;
}
