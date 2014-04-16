/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

START_TEST(test_client_shutdown)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/on:1=\"/test/good\"",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");

	qev_exit();
	close(tc);
}
END_TEST

START_TEST(test_client_shutdown_while_subscribing)
{
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();

	test_send(tc, "/qio/on:1=\"/test/delayed\"");
	QEV_WAIT_FOR(client->subs != NULL && g_hash_table_size(client->subs) > 0);

	client_free_all(client);

	test_msg(tc, "/qio/callback/1:0={\"code\":420,\"data\":null,\"err_msg\":null}");

	qev_exit();
	close(tc);
}
END_TEST

static enum evs_status _cb1(
	struct client *client G_GNUC_UNUSED,
	const void *data,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	const guint *num = data;

	if (*num == 1) {
		return EVS_STATUS_OK;
	} else {
		return EVS_STATUS_ERR;
	}
}

static enum evs_status _cb0(
	struct client *client,
	const void *data,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	const guint *num = data;
	if (*num == 0) {
		guint *num2 = g_malloc(sizeof(*num));
		*num2 = 1;
		evs_cb_with_cb(client, client_cb, "1", _cb1, num2, g_free);
		return EVS_STATUS_HANDLED;
	} else {
		return EVS_STATUS_ERR;
	}
}

static enum evs_status _handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	guint *num = g_malloc0(sizeof(*num));
	*num = 0;
	evs_cb_with_cb(client, client_cb, "0", _cb0, num, g_free);
	return EVS_STATUS_HANDLED;
}

static void _setup_callbacks()
{
	test_setup();
	evs_add_handler("/test", "/client/cb", _handler, NULL, NULL, FALSE);
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

START_TEST(test_client_callbacks_invalid)
{
	qev_fd_t tc = test_client();

	test_cb(tc,
		"/qio/callback/1:1=null",
		"/qio/callback/1:0={\"code\":404,\"data\":null,"
			"\"err_msg\":\"callback doesn't exist\"}");

	close(tc);
}
END_TEST

START_TEST(test_client_callbacks_all)
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

	test_ping(tc);

	close(tc);
	qev_buffer_put(buff);
}
END_TEST

START_TEST(test_client_callbacks_limits)
{
	guint i;
	qev_fd_t tc = test_client();
	GString *buff = qev_buffer_get();

	test_cb(tc,
		"/qio/callback/-1:1=null",
		"/qio/callback/1:0={\"code\":404,\"data\":null,"
			"\"err_msg\":\"callback doesn't exist\"}");

	test_cb(tc,
		"/qio/callback/-1:-1=null",
		"/qio/callback/18446744073709551615:0={\"code\":404,\"data\":null,"
			"\"err_msg\":\"callback doesn't exist\"}");

	for (i = 0; i < G_N_ELEMENTS(((struct client*)NULL)->cbs) * 2; i++) {
		g_string_printf(buff, "/qio/callback/%u:1=null", i << 16 | 1);
		test_cb(tc,
			buff->str,
			"/qio/callback/1:0={\"code\":404,\"data\":null,"
				"\"err_msg\":\"callback doesn't exist\"}");
	}

	qev_buffer_put(buff);
	close(tc);
}
END_TEST

START_TEST(test_client_callbacks_overflow)
{
	const guint total_cbs = G_N_ELEMENTS(((struct client*)NULL)->cbs);

	guint i;
	guint32 cb;
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();
	GString *buff = qev_buffer_get();
	g_string_set_size(buff, 127);

	client->cbs_id -= total_cbs / 2;

	for (i = 0; i < total_cbs; i++) {
		test_send(tc, "/test/client/cb:1=");
		test_recv(tc, buff->str, buff->allocated_len);

		cb = g_ascii_strtoull(buff->str + sizeof("/qio/callback/1:") - 1, NULL, 10);
		ck_assert_uint_gt(cb, 0);

		g_string_printf(buff, "/qio/callback/%u:0=null", cb);
		test_send(tc, buff->str);
	}

	qev_buffer_put(buff);
	close(tc);
}
END_TEST

static enum evs_status _test_client_callbacks_prune_cb(
	struct client *client G_GNUC_UNUSED,
	const void *data G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	return EVS_STATUS_OK;
}

static void _test_client_callbacks_prune_free(void *i_)
{
	guint *i = i_;
	*i = 100;
}

START_TEST(test_client_callbacks_prune)
{
	guint i = 0;
	qev_fd_t tc = test_client();
	struct client *client = test_get_client();

	client_cb_new(client,
		_test_client_callbacks_prune_cb,
		&i,
		_test_client_callbacks_prune_free);

	ck_assert(client->cbs[0] != NULL);
	client->cbs[0]->created = qev_monotonic - QEV_SEC_TO_USEC(cfg_clients_cb_max_age * 100);

	periodic_run();

	ck_assert(client->cbs[0] == NULL);
	ck_assert_uint_eq(i, 100);

	close(tc);
}
END_TEST

static void _setup_subs()
{
	test_setup();

	union qev_cfg_val val = { .ui64 = 100 };
	qev_cfg_set("quick.io", "clients-subs-total", val, NULL);
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
	g_array_free(tcs, TRUE);
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

	for (i = 0; i < cfg_clients_subs_total; i++) {
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
	qev_mock_add("client_sub_accept", "qev_list_try_add", FALSE, NULL, 0);

	qev_fd_t tc = test_client();
	GString *buff = qev_buffer_get();

	test_cb(tc,
		"/qio/on:1=\"/test/good-childs/1\"",
		"/qio/callback/1:0={\"code\":420,\"data\":null,\"err_msg\":null}");

	qev_buffer_put(buff);
	close(tc);
}
END_TEST

START_TEST(test_client_data_sane)
{
	GVariant *v;
	struct client *client = qev_surrogate_new();
	const GQuark q = g_quark_from_static_string("test");

	client_set(client, q, g_variant_new_boolean(FALSE));

	ck_assert(client_has(client, q));

	v = client_get(client, q);
	ck_assert(g_variant_type_equal(g_variant_get_type(v), G_VARIANT_TYPE_BOOLEAN));
	ck_assert(g_variant_get_boolean(v) == FALSE);
	g_variant_unref(v);

	client_del(client, q);
	ck_assert(!client_has(client, q));

	client_set(client, q, NULL);
	ck_assert(!client_has(client, q));

	client_set(client, q, g_variant_new_boolean(FALSE));

	qev_close(client, 0);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("client", &sr, &s);

	tcase = tcase_create("Shutdown");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_client_shutdown);
	tcase_add_test(tcase, test_client_shutdown_while_subscribing);

	tcase = tcase_create("Callbacks");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, _setup_callbacks, test_teardown);
	tcase_add_test(tcase, test_client_callbacks);
	tcase_add_test(tcase, test_client_callbacks_invalid);
	tcase_add_test(tcase, test_client_callbacks_all);
	tcase_add_test(tcase, test_client_callbacks_limits);
	tcase_add_test(tcase, test_client_callbacks_overflow);
	tcase_add_test(tcase, test_client_callbacks_prune);

	tcase = tcase_create("Subs");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, _setup_subs, test_teardown);
	tcase_add_test(tcase, test_client_subs_sane);
	tcase_add_test(tcase, test_client_subs_unfair);
	tcase_add_test(tcase, test_client_subs_list_add_fail);

	tcase = tcase_create("Data");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_client_data_sane);

	return test_do(sr);
}
