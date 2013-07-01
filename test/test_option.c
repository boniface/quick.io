#include <stdio.h>

#include "test.h"

#define CONFIG_FILE "qio.ini"

#define QIOINI "[quick.io]\n" \
	"bind-address = 0.0.0.0\n" \
	"bind-port = 5001\n" \
	"log-file = /var/log/qio.log\n" \
	"hostname = notmyhostname\n" \
	"threads = 100\n" \
	"max-subs = 128\n" \
	"max-message-len = 4096\n" \
	"support-flash = 1\n" \
	"timeout = 15\n" \
	"user = test\n" \
	"[quick.io-apps]\n" \
	"skeleton = skeleton\n" \
	"test = test\n" \
	"test:prefix = /test\n" \
	"test2 = /some/path\n" \
	"test2:prefix = /two/things\n"

#define QIOINI_BAD_MAX_SUBS "[quick.io]\n" \
	"max-subs = 100\n" \
	"[quick.io-apps]"

#define QIOINI_BAD_TIMEOUT "[quick.io]\n" \
	"timeout = 129\n" \
	"[quick.io-apps]"

#define QIOINI_STRINGV "[test-app]\n" \
	"string-array = three;different;values"

#define INVALID_CONFIG "alksdflskajdfsadlkfj"

static void _test_option_setup()
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI, 1, sizeof(QIOINI), f);
	fclose(f);
}

static void _test_option_teardown()
{
	unlink(CONFIG_FILE);
}

START_TEST(test_option_cl_args_short)
{
	char *argv[] = {"./server", "-c", CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	char path[PATH_MAX+1];
	memset(&path, 0, sizeof(path));
	if (realpath(CONFIG_FILE, path) == NULL) {
		check(FALSE, "Could not find config file");
	}

	check(option_parse_args(argc, argv, NULL), "Parsed options");
	check_uint32_eq(_config_files->len, 1, "Only 1 config file");
	check_str_eq(g_ptr_array_index(_config_files, 0), path,
					"Correct config file");
}
END_TEST

START_TEST(test_option_cl_args_long)
{
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	char path[PATH_MAX+1];
	memset(&path, 0, sizeof(path));
	if (realpath(CONFIG_FILE, path) == NULL) {
		check(FALSE, "Could not find config file");
	}

	check(option_parse_args(argc, argv, NULL), "Parsed options");
	check_uint32_eq(_config_files->len, 1, "Only 1 config file");
	check_str_eq(g_ptr_array_index(_config_files, 0), path,
					"Correct config file");
}
END_TEST

START_TEST(test_option_cl_args_fail)
{
	char *argv[] = {"./server", "--config-f ile="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check_not(option_parse_args(argc, argv, NULL), "Failed to parse");
}
END_TEST

START_TEST(test_option_nonexistent)
{
	char *argv[] = {"./server", "--config-file=this_cant_exist_EVER.ini"};
	int argc = G_N_ELEMENTS(argv);

	check_not(option_parse_args(argc, argv, NULL), "File ready");
	check_not(option_parse_config_file(NULL, NULL, 0), "Couldn't load file");
}
END_TEST

START_TEST(test_option_invalid_config)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(INVALID_CONFIG, 1, sizeof(INVALID_CONFIG), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "File ready");
	check_not(option_parse_config_file(NULL, NULL, 0), "Couldn't load file");
}
END_TEST

START_TEST(test_option_setup_config)
{
	gchar *a = NULL;
	gchar *b = "test";
	gchar **c = (gchar*[]){
		"test",
		"test2",
		NULL,
	};
	struct config_file_entry config[] = {
		{"a", e_string, &a},
		{"b", e_string, &b},
		{"c", e_stringv, &c},
	};

	struct config_file_entry *config_orig = config;

	option_setup_config(config, G_N_ELEMENTS(config));

	check(a == NULL);
	check(b != config_orig[1].arg_data);
	check(c != config_orig[2].arg_data);
}
END_TEST

START_TEST(test_option_config_dir)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI, 1, sizeof(QIOINI), f);
	fclose(f);

	char *argv[] = {"./server", "--config-dir=."};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "Config files found");
	check_uint32_eq(_config_files->len, 2, "Files all found");
}
END_TEST

START_TEST(test_option_config_dir_bad)
{
	char *argv[] = {"./server", "--config-dir=.", "--config-file=askdfkjasdflasdf"};
	int argc = G_N_ELEMENTS(argv);

	check_not(option_parse_args(argc, argv, NULL), "Config files found");
}
END_TEST

START_TEST(test_option_all)
{
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "Options parsed");
	check(option_parse_config_file(NULL, NULL, 0), "File parsed");

	check_int32_eq(option_apps_count(), 3, "3 apps found");
	check_str_eq(option_bind_address(), "0.0.0.0", "Bind address");
	check_int32_eq(option_bind_port(), 5001, "Correct port");
	check(option_bind_address_ssl() == NULL, "No bind address");
	check_int32_eq(option_bind_port_ssl(), 443, "Correct port");
	check_str_eq(option_log_file(), "/var/log/qio.log", "Correct port");
	check_str_eq(option_hostname(), "notmyhostname", "Correct hostname");
	check_uint64_eq(option_max_message_size(), 4096, "Max message size");
	check_uint64_eq(option_max_subscriptions(), 128, "Correct subscription count");
	check_not(option_run_app_test(), "Not running app tests");
	check(option_ssl_cert_chain() == NULL, "No cert chain");
	check(option_ssl_private_key() == NULL, "No private key");
	check(option_support_flash(), "Flash supported");
	check_int32_eq(option_threads(), 100, "Tons of threads");
	check_int32_eq(option_timeout(), 15, "Tons of timeout time");
	check_str_eq(option_user(), "test", "Correct user");

	const struct opt_app **apps = option_apps();

	check_str_eq((*apps)->config_group, "skeleton", "First is skeleton");
	check_str_eq((*apps)->path, "skeleton", "Skeleton has simple path");
	check_ptr_eq((*apps)->prefix, NULL, "Skeleton has no prefix");

	check_str_eq((*(apps + 1))->config_group, "test", "Second is test");
	check_str_eq((*(apps + 1))->path, "test", "Test has simple path");
	check_str_eq((*(apps + 1))->prefix, "/test", "Test prefixed with /test");

	check_str_eq((*(apps + 2))->config_group, "test2", "Third is test2");
	check_str_eq((*(apps + 2))->path, "/some/path", "Test2 has absolute path");
	check_str_eq((*(apps + 2))->prefix, "/two/things",
			"Test2 prefixed with /two/things");
}
END_TEST

START_TEST(test_option_empty)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite("", 1, 0, f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "Options parsed");
	check_not(option_parse_config_file(NULL, NULL, 0),
			"Could not parse empty file");
}
END_TEST

START_TEST(test_option_bad_subs)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_BAD_MAX_SUBS, 1, sizeof(QIOINI_BAD_MAX_SUBS), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "Options parsed");
	check_not(option_parse_config_file(NULL, NULL, 0), "Bad max subs");
}
END_TEST

START_TEST(test_option_bad_timeout)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_BAD_TIMEOUT, 1, sizeof(QIOINI_BAD_TIMEOUT), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "Options parsed");
	check_not(option_parse_config_file(NULL, NULL, 0), "Bad max subs");
}
END_TEST

START_TEST(test_option_stringv)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_STRINGV, 1, sizeof(QIOINI_STRINGV), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	check(option_parse_args(argc, argv, NULL), "Options parsed");

	gchar **vec = (gchar*[]) {
		"not",
		"right",
		NULL,
	};
	gint len = 0;
	struct config_file_entry opts[] = {
		{"string-array", e_stringv, &vec, &len}
	};

	option_setup_config(opts, G_N_ELEMENTS(opts));
	check(option_parse_config_file("test-app", opts, G_N_ELEMENTS(opts)),
				"Config file parsed");

	check_int32_eq(len, 3, "Correct string length");
	check_str_eq(*vec, "three", "First value right");
	check_str_eq(*(vec + 1), "different", "Second value right");
	check_str_eq(*(vec + 2), "values", "Third value right");
}
END_TEST

START_TEST(test_option_multiple_configs)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI_STRINGV, 1, sizeof(QIOINI_STRINGV), f);
	fclose(f);

	f = fopen("test.ini", "w");
	fwrite(QIOINI_STRINGV, 1, sizeof(QIOINI_STRINGV), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE, "--config-file=test.ini"};
	int argc = G_N_ELEMENTS(argv);
	check(option_parse_args(argc, argv, NULL), "Options parsed");
	check_uint32_eq(_config_files->len, 2, "Two files loaded");

	unlink("test.ini");
}
END_TEST

Suite* option_suite()
{
	TCase *tc;
	Suite *s = suite_create("Option");

	tc = tcase_create("Command Line Args");
	tcase_add_checked_fixture(tc, _test_option_setup, _test_option_teardown);
	tcase_add_test(tc, test_option_cl_args_short);
	tcase_add_test(tc, test_option_cl_args_long);
	tcase_add_test(tc, test_option_cl_args_fail);
	suite_add_tcase(s, tc);

	tc = tcase_create("Nonexistent config");
	tcase_add_checked_fixture(tc, NULL, _test_option_teardown);
	tcase_add_test(tc, test_option_nonexistent);
	tcase_add_test(tc, test_option_invalid_config);
	suite_add_tcase(s, tc);

	tc = tcase_create("Config File Options");
	tcase_add_checked_fixture(tc, _test_option_setup, _test_option_teardown);
	tcase_add_test(tc, test_option_setup_config);
	tcase_add_test(tc, test_option_config_dir);
	tcase_add_test(tc, test_option_config_dir_bad);
	tcase_add_test(tc, test_option_all);
	tcase_add_test(tc, test_option_empty);
	tcase_add_test(tc, test_option_bad_subs);
	tcase_add_test(tc, test_option_bad_timeout);
	tcase_add_test(tc, test_option_stringv);
	tcase_add_test(tc, test_option_multiple_configs);
	suite_add_tcase(s, tc);

	return s;
}
