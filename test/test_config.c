/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

#define PORTS_0 \
	"[quick.io]\n" \
	"bind-port = 120521\n" \
	"bind-port-ssl = 321987\n"

#define PORTS_1 \
	"[quick.io]\n" \
	"bind-port = 0\n" \
	"bind-port-ssl = 0\n"

#define HEARTBEAT_0 \
	"[quick.io]\n" \
	"heartbeat-interval = 3\n"

#define HEARTBEAT_1 \
	"[quick.io]\n" \
	"heartbeat-interval = 180\n"

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

START_TEST(test_config_invalid_ports_0)
{
	guint64 before = cfg_bind_port;

	_do(PORTS_0);
	ck_assert_uint_eq(before, cfg_bind_port);
}
END_TEST

START_TEST(test_config_invalid_ports_1)
{
	guint64 before = cfg_bind_port;

	_do(PORTS_1);
	ck_assert_uint_eq(before, cfg_bind_port);
}
END_TEST

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

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_config", &sr, &s);

	tcase = tcase_create("Options");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_config_invalid_ports_0);
	tcase_add_test(tcase, test_config_invalid_ports_1);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_0);
	tcase_add_test(tcase, test_config_invalid_heartbeat_interval_1);

	return test_do(sr);
}
