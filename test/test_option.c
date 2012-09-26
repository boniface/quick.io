#include <stdio.h>

#include "test.h"

#define CONFIG_FILE "qio.ini"

#define QIOINI "[quick.io]\n" \
	"# The address the main server should listen on\n" \
	"# Use 0.0.0.0 to listen everywhere.\n" \
	"#\n" \
	"# type - string\n" \
	"bind-address = 0.0.0.0\n" \
	"\n" \
	"# The port that the server listens on\n" \
	"#\n" \
	"# type - int\n" \
	"port = 5001\n" \
	"\n" \
	"# The number of processes to run to service clients\n" \
	"processes = 100\n" \
	"\n" \
	"# The maximum number of subscriptions a single client may have\n" \
	"#\n" \
	"# type - uint64\n" \
	"max-subs = 128\n" \
	"\n" \
	"# The maximum length of a message from a client\n" \
	"# This number should be a power of 2 for maximum efficiency.\n" \
	"#\n" \
	"# type - uint64\n" \
	"max-message-len = 4096\n" \
	"\n" \
	"# The time a misbehaving client has to finish a request before he is killed\n" \
	"#\n" \
	"# type - int\n" \
	"timeout = 15\n" \
	"[quick.io-apps]\n" \
	"skeleton = skeleton\n" \
	"test = test\n" \
	"test:prefix = /test\n" \
	"test2 = /some/path\n" \
	"test2:prefix = /two/things\n"

#define QIOINI_BAD_MAX_SUBS "[quick.io]\n" \
	"max-subs = 100\n" \
	"[quick.io-apps]"

#define QIOINI_STRING_ARRAY "[test-app]\n" \
	"string-array = three;different;values"

static void _test_option_setup() {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI, 1, sizeof(QIOINI), f);
	fclose(f);
}

START_TEST(test_option_cl_args_short) {
	char *argv[] = {"./server", "-c", CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	char path[PATH_MAX+1];
	memset(&path, 0, sizeof(path));
	if (realpath(CONFIG_FILE, path) == NULL) {
		test(FALSE, "Could not find config file");
	}
	
	test(option_parse_args(argc, argv, NULL), "Parsed options");
	test_str_eq(option_config_file(), path, "Correct config file");
}
END_TEST

START_TEST(test_option_cl_args_long) {
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	char path[PATH_MAX+1];
	memset(&path, 0, sizeof(path));
	if (realpath(CONFIG_FILE, path) == NULL) {
		test(FALSE, "Could not find config file");
	}
	
	test(option_parse_args(argc, argv, NULL), "Parsed options");
	test_str_eq(option_config_file(), path, "Correct config file");
}
END_TEST

START_TEST(test_option_cl_args_fail) {
	char *argv[] = {"./server", "--config-f ile="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test_not(option_parse_args(argc, argv, NULL), "Failed to parse");
}
END_TEST

START_TEST(test_option_bad_config) {
	char *argv[] = {"./server", "--config-file=this_cant_exist_EVER.ini"};
	int argc = G_N_ELEMENTS(argv);
	
	test_not(option_parse_args(argc, argv, NULL), "File ready");
	test_not(option_parse_config_file(NULL, NULL, 0, NULL), "Couldn't load file");
}
END_TEST

START_TEST(test_option_all) {
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "File parsed");
	
	test_int32_eq(option_apps_count(), 3, "3 apps found");
	test_str_eq(option_bind_address(), "0.0.0.0", "Bind address");
	test_uint64_eq(option_max_message_size(), 4096, "Max message size");
	test_int32_eq(option_port(), 5001, "Correct port");
	test_uint64_eq(option_max_subscriptions(), 128, "Correct subscription count");
	test_int32_eq(option_processes(), 100, "Tons of processes");
	test_int32_eq(option_timeout(), 15, "Tons of timeout time");
	
	opt_app_t **apps = option_apps();
	
	test_str_eq((*apps)->config_group, "skeleton", "First is skeleton");
	test_str_eq((*apps)->path, "skeleton", "Skeleton has simple path");
	test_ptr_eq((*apps)->prefix, NULL, "Skeleton has no prefix");
	
	test_str_eq((*(apps + 1))->config_group, "test", "Second is test");
	test_str_eq((*(apps + 1))->path, "test", "Test has simple path");
	test_str_eq((*(apps + 1))->prefix, "/test", "Test prefixed with /test");
	
	test_str_eq((*(apps + 2))->config_group, "test2", "Third is test2");
	test_str_eq((*(apps + 2))->path, "/some/path", "Test2 has absolute path");
	test_str_eq((*(apps + 2))->prefix, "/two/things", "Test2 prefixed with /two/things");
}
END_TEST

START_TEST(test_option_empty) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite("", 1, 0, f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test_not(option_parse_config_file(NULL, NULL, 0, NULL), "Could not parse empty file");
}
END_TEST

START_TEST(test_option_bad_subs) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_BAD_MAX_SUBS, 1, sizeof(QIOINI_BAD_MAX_SUBS), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test_not(option_parse_config_file(NULL, NULL, 0, NULL), "Bad max subs");
}
END_TEST

START_TEST(test_option_string_array) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_STRING_ARRAY, 1, sizeof(QIOINI_STRING_ARRAY), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	
	gchar **array = NULL;
	gint len = 0;
	config_file_entry_t opts[] = {
		{"string-array", e_string_array, &array, &len}
	};
	
	test(option_parse_config_file("test-app", opts, G_N_ELEMENTS(opts), NULL), "Config file parsed");
	
	test_int32_eq(len, 3, "Correct string length");
	test_str_eq(*array, "three", "First value right");
	test_str_eq(*(array + 1), "different", "Second value right");
	test_str_eq(*(array + 2), "values", "Third value right");
}
END_TEST

START_TEST(test_option_process_specific_0) {
	test(option_parse_args(0, NULL, NULL), "File ready");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "Couldn't load file");
	
	test_int32_eq(option_port(), 5000, "Correct port");
	
	option_set_process(0);
	
	test_int32_eq(option_port(), 5000, "Correct port");
}
END_TEST

START_TEST(test_option_process_specific_1) {
	test(option_parse_args(0, NULL, NULL), "File ready");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "Couldn't load file");
	
	test_int32_eq(option_port(), 5000, "Correct port");
	
	option_set_process(1);
	test_int32_eq(option_port(), 5001, "Correct port");
	
	// Make sure it's not idempotent
	option_set_process(1);
	test(option_port() != 5001, "Not idempotent");
}
END_TEST

Suite* option_suite() {
	TCase *tc;
	Suite *s = suite_create("Option");
	
	tc = tcase_create("Command Line Args");
	tcase_add_checked_fixture(tc, _test_option_setup, NULL);
	tcase_add_test(tc, test_option_cl_args_short);
	tcase_add_test(tc, test_option_cl_args_long);
	tcase_add_test(tc, test_option_cl_args_fail);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Nonexistant config");
	tcase_add_test(tc, test_option_bad_config);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Config File Options");
	tcase_add_checked_fixture(tc, _test_option_setup, NULL);
	tcase_add_test(tc, test_option_all);
	tcase_add_test(tc, test_option_empty);
	tcase_add_test(tc, test_option_bad_subs);
	tcase_add_test(tc, test_option_string_array);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Process-specific settings");
	tcase_add_checked_fixture(tc, _test_option_setup, NULL);
	tcase_add_test(tc, test_option_process_specific_0);
	tcase_add_test(tc, test_option_process_specific_1);
	suite_add_tcase(s, tc);
	
	return s;
}