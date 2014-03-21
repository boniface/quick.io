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

#define PUBLIC_ADDRESS \
	"[quick.io]\n" \
	"public-address = http://what is this?\n"

static void _do(const gchar *cfg)
{
	gboolean die = FALSE;

	if (!g_file_set_contents("test_config.ini", cfg, strlen(cfg), NULL)) {
		goto abort;
	}

	test_setup_with_config("test_config.ini");

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
	_do(HEARTBEAT_0);
	ck_assert_uint_eq(cfg_heartbeat_interval, 10);
}
END_TEST

START_TEST(test_config_invalid_heartbeat_interval_1)
{
	_do(HEARTBEAT_1);
	ck_assert_uint_eq(cfg_heartbeat_interval, 10);
}
END_TEST

START_TEST(test_config_invalid_sub_min_size_0)
{
	_do(SUB_MIN_SIZE_0);
	ck_assert_uint_eq(cfg_sub_min_size, 8192);
}
END_TEST

START_TEST(test_config_invalid_sub_min_size_1)
{
	_do(SUB_MIN_SIZE_1);
	ck_assert_uint_eq(cfg_sub_min_size, 8192);
}
END_TEST

START_TEST(test_config_invalid_client_subs_fairness)
{
	_do(SUB_FAIRNESS);
	ck_assert_uint_eq(cfg_clients_subs_fairness, 80);
}
END_TEST

START_TEST(test_config_invalid_public_address)
{
	// Don't make this depend on the network
	qev_mock_add("_validate_public_address", "getaddrinfo", EAI_NONAME, NULL, EINVAL);

	_do(PUBLIC_ADDRESS);
	ck_assert_ptr_eq(cfg_public_address, NULL);
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
	tcase_add_checked_fixture(tcase, NULL, test_teardown);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_0);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_1);
	tcase_add_test(tcase, test_config_invalid_sub_min_size_0);
	tcase_add_test(tcase, test_config_invalid_sub_min_size_1);
	tcase_add_test(tcase, test_config_invalid_client_subs_fairness);
	tcase_add_test(tcase, test_config_invalid_public_address);

	tcase = tcase_create("Coverage");
	suite_add_tcase(s, tcase);
	tcase_add_test_raise_signal(tcase, test_config_coverage_support_flash, 5);

	return test_do(sr);
}
