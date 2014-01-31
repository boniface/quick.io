/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

#define HEARTBEAT_0 \
	"[quick.io]\n" \
	"heartbeat-interval = 3\n"

#define HEARTBEAT_1 \
	"[quick.io]\n" \
	"heartbeat-interval = 180\n"

#define SUB_MIN_SIZE_0 \
	"[quick.io]\n" \
	"sub-min-size = 0\n"

#define SUB_MIN_SIZE_1 \
	"[quick.io]\n" \
	"heartbeat-interval = 4294967295\n"

#define SUB_FAIRNESS \
	"[quick.io]\n" \
	"clients-subs-fairness = 4294967295\n"

static void _do(const gchar *cfg)
{
	gboolean die = FALSE;

	if (!g_file_set_contents("test_config.ini", cfg, strlen(cfg), NULL)) {
		goto abort;
	}

	if (!qev_cfg_parse("test_config.ini")) {
		goto abort;
	}

out:
	unlink("test_config.ini");

	if (die) {
		ck_abort();
	}

	return;

abort:
	die = TRUE;
	goto out;
}

START_TEST(test_config_invalid_heartbeat_interval_0)
{
	guint64 before = cfg_heartbeat_interval;

	_do(HEARTBEAT_0);
	ck_assert_uint_eq(before, cfg_heartbeat_interval);
}
END_TEST

START_TEST(test_config_invalid_heartbeat_interval_1)
{
	guint64 before = cfg_heartbeat_interval;

	_do(HEARTBEAT_1);
	ck_assert_uint_eq(before, cfg_heartbeat_interval);
}
END_TEST

START_TEST(test_config_invalid_sub_min_size_0)
{
	guint64 before = cfg_sub_min_size;

	_do(SUB_MIN_SIZE_0);
	ck_assert_uint_eq(before, cfg_sub_min_size);
}
END_TEST

START_TEST(test_config_invalid_sub_min_size_1)
{
	guint64 before = cfg_sub_min_size;

	_do(SUB_MIN_SIZE_1);
	ck_assert_uint_eq(before, cfg_sub_min_size);
}
END_TEST

START_TEST(test_config_invalid_client_subs_fairness)
{
	guint64 before = cfg_clients_subs_fairness;

	_do(SUB_FAIRNESS);
	ck_assert_uint_eq(before, cfg_clients_subs_fairness);
}
END_TEST

START_TEST(test_config_coverage_support_flash)
{
	gchar *args[] = {"test"};
	cfg_support_flash = TRUE;
	qio_main(1, args);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_config", &sr, &s);

	tcase = tcase_create("Options");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_0);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_1);
	tcase_add_test(tcase, test_config_invalid_sub_min_size_0);
	tcase_add_test(tcase, test_config_invalid_sub_min_size_1);
	tcase_add_test(tcase, test_config_invalid_client_subs_fairness);

	tcase = tcase_create("Coverage");
	suite_add_tcase(s, tcase);
	tcase_add_test_raise_signal(tcase, test_config_coverage_support_flash, 5);

	return test_do(sr);
}
