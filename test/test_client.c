/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

static enum evs_status _cb1(
	client_t *client G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	return EVS_STATUS_OK;
}

static enum evs_status _cb0(
	client_t *client,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	guint *num = g_malloc(sizeof(*num));
	*num = 1;
	qio_evs_cb_with_cb(client, client_cb, "1", _cb1, num, g_free);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	guint *num = g_malloc0(sizeof(*num));
	qio_evs_cb_with_cb(client, client_cb, "0", _cb0, num, g_free);
	return EVS_STATUS_HANDLED;
}

static void _setup()
{
	test_setup();
	evs_add_handler("/test/client/cb", _handler, NULL, NULL, FALSE);
}

START_TEST(test_callbacks)
{
	test_client_t *tc = test_client();

	test_cb(tc,
		"/test/client/cb:1=",
		"/qio/callback/1:1={\"code\":200,\"data\":0}");

	test_cb(tc,
		"/qio/callback/1:2=",
		"/qio/callback/2:2={\"code\":200,\"data\":1}");

	test_close(tc);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("client", &sr, &s);

	tcase = tcase_create("Callbacks");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, _setup, test_teardown);
	tcase_add_test(tcase, test_callbacks);

	return test_do(sr);
}
