#include "test.h"

static void _apps_setup() {
	utils_stats_setup();
	option_parse_config_file(NULL, NULL, 0, NULL);
	apps_init();
	evs_client_init();
	evs_server_init();
}

static void _apps_teardown() {
	utils_stats_teardown();
}

START_TEST(test_apps_events_register) {
	// Just make sure registering works
	apps_register_events();
}
END_TEST

START_TEST(test_apps_events_on) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/test/event", client);
	
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_off) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/test/event", client);
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	evs_client_unsub_client("/test/event", client);
	test_size_eq(utils_stats()->apps_client_handler_off, 1, "Single client unsubscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_register) {
	// Make sure our only app was registered
	test_size_eq(_apps->len, 1, "App registered");
	
	app_t *app = g_ptr_array_index(_apps, 0);
	test_str_eq(app->name, "test", "Test app registered");
	test_str_eq(app->event_prefix, "/test", "Test app registered");
}
END_TEST

START_TEST(test_apps_cb_prefork) {
	test(apps_prefork(), "Apps preforked");
	test_size_eq(utils_stats()->apps_prefork, 1, "Test app preforked");
}
END_TEST

START_TEST(test_apps_cb_postfork) {
	test(apps_postfork(), "Apps postforked");
	test_size_eq(utils_stats()->apps_postfork, 1, "Test app postforked");
}
END_TEST

START_TEST(test_apps_cb_client_connect) {
	client_t *client = u_client_create();
	
	apps_client_connect(client);
	test_size_eq(utils_stats()->apps_client_connect, 1, "Single client connected");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_client_close) {
	client_t *client = u_client_create();
	
	apps_client_close(client);
	test_size_eq(utils_stats()->apps_client_close, 1, "Single client disconnected");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_client_subscribe) {
	client_t *client = u_client_create();
	evs_client_sub_t *sub = g_malloc0(sizeof(*sub));
	sub->event_path = "/test";
	sub->extra = NULL;
	sub->extra_len = 0;
	sub->handler = g_malloc0(sizeof(sub->handler));
	
	apps_evs_client_subscribe(client, sub);
	test_size_eq(utils_stats()->apps_client_subscribe, 1, "Single client subscribed");
	
	g_free(sub->handler);
	g_free(sub);
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_client_unsubscribe) {
	client_t *client = u_client_create();
	evs_client_sub_t *sub = g_malloc0(sizeof(*sub));
	sub->event_path = "/test";
	sub->extra = NULL;
	sub->extra_len = 0;
	sub->handler = g_malloc0(sizeof(sub->handler));
	
	apps_evs_client_unsubscribe(client, sub);
	test_size_eq(utils_stats()->apps_client_unsubscribe, 1, "Single client unsubscribed");
	
	g_free(sub->handler);
	g_free(sub);
	u_client_free(client);
}
END_TEST

Suite* apps_suite() {
	TCase *tc;
	Suite *s = suite_create("Apps");
	
	tc = tcase_create("Events");
	tcase_add_checked_fixture(tc, _apps_setup, _apps_teardown);
	tcase_add_test(tc, test_apps_events_register);
	tcase_add_test(tc, test_apps_events_on);
	tcase_add_test(tc, test_apps_events_off);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Callbacks");
	tcase_add_checked_fixture(tc, _apps_setup, _apps_teardown);
	tcase_add_test(tc, test_apps_cb_register);
	tcase_add_test(tc, test_apps_cb_prefork);
	tcase_add_test(tc, test_apps_cb_postfork);
	tcase_add_test(tc, test_apps_cb_client_connect);
	tcase_add_test(tc, test_apps_cb_client_close);
	tcase_add_test(tc, test_apps_cb_client_subscribe);
	tcase_add_test(tc, test_apps_cb_client_unsubscribe);
	suite_add_tcase(s, tc);
	
	return s;
}