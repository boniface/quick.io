/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

static void _set(const gchar *vars)
{
	gboolean die = FALSE;
	GString *buff = qev_buffer_get();
	g_string_printf(buff, "[quick.io-apps]\n%s\n", vars);

	if (!g_file_set_contents("test_apps_errors.ini", buff->str, buff->len, NULL)) {
		goto abort;
	}

	test_setup_with_config("test_apps_errors.ini");

out:
	unlink("test_apps_errors.ini");
	qev_buffer_put(buff);

	if (die) {
		ck_abort();
	}

	return;

abort:
	die = TRUE;
	goto out;
}

START_TEST(test_apps_sane)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/test/good:1={\"key\": \"value\"}",
		"/qio/callback/1:0={\"code\":200,\"data\":\"test\"}");

	test_cb(tc,
		"/test/good:2=",
		"/qio/callback/2:0={\"code\":400,\"data\":null,"
			"\"err_msg\":\"Error with \\\"key\\\" in json.\"}");

	close(tc);
}
END_TEST

START_TEST(test_apps_config)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/test/config:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":172346}");

	qev_cfg_set("test_app_sane", "sane-value", "81723", NULL);

	test_cb(tc,
		"/test/config:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":81723}");

	close(tc);
}
END_TEST

START_TEST(test_apps_apps_test)
{
	apps_test();
}
END_TEST

START_TEST(test_apps_error_path)
{
	GString *buff = qev_buffer_get();
	g_string_printf(buff, "/app = %s", "test/apps/test_app_test_sane");

	_set(buff->str);

	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_apps_error_invalid_app)
{
	GString *buff = qev_buffer_get();
	gchar *cwd = g_get_current_dir();
	g_string_printf(buff, "/app = %s/%s", cwd, "app_test_invalid");

	_set(buff->str);

	g_free(cwd);
	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_apps_error_init)
{
	GString *buff = qev_buffer_get();
	gchar *cwd = g_get_current_dir();
	g_string_printf(buff, "/app = %s/%s", cwd, "app_test_fatal_init");

	_set(buff->str);

	g_free(cwd);
	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_apps_error_duplicate)
{
	GString *buff = qev_buffer_get();
	gchar *cwd = g_get_current_dir();
	g_string_printf(buff, "/test = %s/%s\n", cwd, "app_test_sane");

	_set(buff->str);

	g_free(cwd);
	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_apps_error_exit)
{
	GString *buff = qev_buffer_get();
	gchar *cwd = g_get_current_dir();
	g_string_printf(buff, "/app = %s/%s", cwd, "app_test_fatal_exit");

	_set(buff->str);

	g_free(cwd);
	qev_buffer_put(buff);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("apps", &sr, &s);

	tcase = tcase_create("Sanity");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_apps_sane);
	tcase_add_test(tcase, test_apps_config);
	tcase_add_test(tcase, test_apps_apps_test);

	tcase = tcase_create("Errors");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, NULL, test_teardown);
	tcase_add_test_raise_signal(tcase, test_apps_error_path, 5);
	tcase_add_test_raise_signal(tcase, test_apps_error_invalid_app, 5);
	tcase_add_test_raise_signal(tcase, test_apps_error_init, 5);
	tcase_add_test_raise_signal(tcase, test_apps_error_duplicate, 5);
	tcase_add_test_raise_signal(tcase, test_apps_error_exit, 5);

	return test_do(sr);
}
