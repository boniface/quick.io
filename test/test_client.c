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
	evs_cb_with_cb(client, client_cb, "1", _cb1, num, g_free);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	guint *num = g_malloc0(sizeof(*num));
	evs_cb_with_cb(client, client_cb, "0", _cb0, num, g_free);
	return EVS_STATUS_HANDLED;
}

static void _setup_callbacks()
{
	test_setup();
	evs_add_handler("/test/client/cb", _handler, NULL, NULL, FALSE);
}

START_TEST(test_client_callbacks)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/test/client/cb:1=",
		"/qio/callback/1:1={\"code\":200,\"data\":0}");

	test_cb(tc,
		"/qio/callback/1:2=",
		"/qio/callback/2:2={\"code\":200,\"data\":1}");

	close(tc);
}
END_TEST

START_TEST(test_client_callbacks_limits)
{
	guint i;
	guint64 cb;
	gboolean saw_evicted_error = FALSE;
	guint64 cb_ids[G_N_ELEMENTS(((struct client*)NULL)->cbs) + 1];
	GString *buff = qev_buffer_get();
	qev_fd_t tc = test_client();

	g_string_set_size(buff, 127);

	for (i = 0; i < G_N_ELEMENTS(cb_ids); i++) {
		test_send(tc, "/test/client/cb:1=");
		test_recv(tc, buff->str, buff->allocated_len);

		cb = g_ascii_strtoull(buff->str + sizeof("/qio/callback/1:") - 1, NULL, 10);
		ck_assert_uint_gt(cb, 0);
		cb_ids[i] = cb;
	}

	for (i = 0; i < G_N_ELEMENTS(cb_ids); i++) {
		cb = cb_ids[i];
		g_string_printf(buff, "/qio/callback/%" G_GUINT64_FORMAT":1=null", cb);
		test_send(tc, buff->str);
		test_recv(tc, buff->str, buff->allocated_len);
		saw_evicted_error |= g_strstr_len(buff->str, -1, "404") != NULL;
	}

	ck_assert(saw_evicted_error);

	test_cb(tc,
		"/qio/ping:1=",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	close(tc);
	qev_buffer_put(buff);
}
END_TEST

static void _setup_subs()
{
	test_setup();

	union qev_cfg_val val = { .ui64 = 100 };
	qev_cfg_set("quick.io", "clients-max-subs", val, NULL);
}

START_TEST(test_client_subs_sane)
{
	guint i;
	qev_fd_t tc;
	gboolean server_full = FALSE;
	GString *buff = qev_buffer_get();
	GArray *tcs = g_array_new(FALSE, FALSE, sizeof(qev_fd_t));

	while (!server_full) {
		gchar rbuff[128];
		gboolean accepted = TRUE;

		i = 0;
		tc = test_client();
		g_array_append_val(tcs, tc);

		while (accepted) {
			g_string_printf(buff, "/qio/on:1=\"/test/good-childs/%d\"", i++);
			test_send_len(tc, buff->str, buff->len);
			test_recv(tc, rbuff, sizeof(rbuff));
			accepted = g_strcmp0("/qio/callback/1:0={\"code\":200,\"data\":null}", rbuff) == 0;

			server_full = !accepted && i++ == 1;
		}
	}

	tc = test_client();
	g_array_append_val(tcs, tc);

	test_cb(tc,
		"/qio/on:1=\"/test/good-childs/1\"",
		"/qio/callback/1:0={\"code\":420,\"data\":null,\"err_msg\":null}");

	qev_buffer_put(buff);
	for (i = 0; i < tcs->len; i++) {
		close(g_array_index(tcs, qev_fd_t, i));
	}
}
END_TEST

START_TEST(test_client_subs_unfair)
{
	union qev_cfg_val val = { .ui64 = 0 };
	qev_cfg_set("quick.io", "clients-subs-fairness", val, NULL);

	guint i;
	qev_fd_t tc = test_client();
	qev_fd_t tc2 = test_client();
	GString *buff = qev_buffer_get();

	for (i = 0; i < cfg_clients_max_subs; i++) {
		g_string_printf(buff, "/qio/on:1=\"/test/good-childs/%d\"", i);
		test_cb(tc,
			buff->str,
			"/qio/callback/1:0={\"code\":200,\"data\":null}");
	}

	test_cb(tc2,
		"/qio/on:1=\"/test/good-childs/1\"",
		"/qio/callback/1:0={\"code\":420,\"data\":null,\"err_msg\":null}");

	qev_buffer_put(buff);
	close(tc);
	close(tc2);
}
END_TEST

START_TEST(test_client_subs_list_add_fail)
{
	qev_mock_add("client_sub_add", "qev_list_try_add", FALSE, NULL, 0);

	qev_fd_t tc = test_client();
	GString *buff = qev_buffer_get();

	test_cb(tc,
		"/qio/on:1=\"/test/good-childs/1\"",
		"/qio/callback/1:0={\"code\":420,\"data\":null,\"err_msg\":null}");

	qev_buffer_put(buff);
	close(tc);
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
	tcase_add_checked_fixture(tcase, _setup_callbacks, test_teardown);
	tcase_add_test(tcase, test_client_callbacks);
	tcase_add_test(tcase, test_client_callbacks_limits);

	tcase = tcase_create("Subs");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, _setup_subs, test_teardown);
	tcase_add_test(tcase, test_client_subs_sane);
	tcase_add_test(tcase, test_client_subs_unfair);
	tcase_add_test(tcase, test_client_subs_list_add_fail);

	return test_do(sr);
}
