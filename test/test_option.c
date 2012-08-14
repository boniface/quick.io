#include <stdio.h>

#include "test.h"

#define CONFIG_FILE "qio.ini"

#define QIOINI "[quick.io]\n" \
	"# The apps to load (.so is not required)\n" \
	"# If the app is in the same directory as the server, then just the name is required\n" \
	"# If the app lives in a separate directory, then the absolute path to the app is required\n" \
	"#\n" \
	"# type - string list\n" \
	"apps = skeleton;test\n" \
	"\n" \
	"# The prefix for the events in the order of `apps`\n" \
	"# Use a single \";\" to indicate no prefix.\n" \
	"#\n" \
	"# type - string\n" \
	"apps-prefixes = ;/test/\n" \
	"\n" \
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
	"timeout = 15"

#define QIOINI_BAD_0 "[quick.io]\n" \
	"max-subs = 100\n" \

#define QIOINI_BAD_1 "[quick.io]\n" \
	"apps-prefixes = /tons/;/of/;/prefixes/\n" \

static void _command_setup() {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI, 1, sizeof(QIOINI), f);
	fclose(f);
}

START_TEST(test_option_cl_args_short) {
	char *argv[] = {"./server", "-f", CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Parsed options");
	test_str_eq(option_config_file(), CONFIG_FILE, "Correct config file");
}
END_TEST

START_TEST(test_option_cl_args_long) {
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Parsed options");
	test_str_eq(option_config_file(), CONFIG_FILE, "Correct config file");
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
	
	test(option_parse_args(argc, argv, NULL), "File ready");
	test_not(option_parse_config_file(NULL, NULL, 0, NULL), "Couldn't load file");
}
END_TEST

START_TEST(test_option_all) {
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "File parsed");
	
	test_int32_eq(option_apps_count(), 2, "2 apps found");
	test_int32_eq(option_apps_prefixes_count(), 2, "2 app prefixes found");
	test_str_eq(option_bind_address(), "0.0.0.0", "Bind address");
	test_uint64_eq(option_max_message_size(), 4096, "Max message size");
	test_int32_eq(option_port(), 5001, "Correct port");
	test_uint64_eq(option_max_subscriptions(), 128, "Correct subscription count");
	test_int32_eq(option_processes(), 100, "Tons of processes");
	test_int32_eq(option_timeout(), 15, "Tons of timeout time");
	
	gchar **apps = option_apps();
	test_str_eq(*apps, "skeleton", "First is skeleton");
	test_str_eq(*(apps + 1), "test", "Second is test");
	
	gchar **prefixes = option_apps_prefixes();
	test_str_eq(*prefixes, "", "No prefix");
	test_str_eq(*(prefixes + 1), "/test/", "Awesome prefix");
}
END_TEST

START_TEST(test_option_bad_subs) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_BAD_0, 1, sizeof(QIOINI_BAD_0), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test_not(option_parse_config_file(NULL, NULL, 0, NULL), "Bad max subs");
}
END_TEST

START_TEST(test_option_bad_prefixes) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_BAD_1, 1, sizeof(QIOINI_BAD_1), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "Options parsed");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "File accepted");
	
	test_int32_eq(option_apps_prefixes_count(), 0, "No prefixes");
	test_ptr_eq(option_apps_prefixes(), NULL, "No prefixes");
}
END_TEST

Suite* option_suite() {
	TCase *tc;
	Suite *s = suite_create("Option");
	
	tc = tcase_create("Command Line Args");
	tcase_add_checked_fixture(tc, _command_setup, NULL);
	tcase_add_test(tc, test_option_cl_args_short);
	tcase_add_test(tc, test_option_cl_args_long);
	tcase_add_test(tc, test_option_cl_args_fail);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Nonexistant config");
	tcase_add_test(tc, test_option_bad_config);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Config File Options");
	tcase_add_checked_fixture(tc, _command_setup, NULL);
	tcase_add_test(tc, test_option_all);
	tcase_add_test(tc, test_option_bad_subs);
	tcase_add_test(tc, test_option_bad_prefixes);
	suite_add_tcase(s, tc);
	
	return s;
}