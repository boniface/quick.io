#include "test.h"

#define CONFIG_FILE "qio.ini"

#define BAD_CONFIG "[quick.io]\n" \
	"[quick.io-apps]\n" \
	"test-bad = test_bad"

#define BAD_CONFIG_NONEXISTENT "[quick.io]\n" \
	"[quick.io-apps]\n" \
	"test-bad = /this/path/does/not/exist"

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

START_TEST(test_apps_events_register_bad) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(BAD_CONFIG, 1, sizeof(BAD_CONFIG), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "File ready");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "Config loaded");
	
	test_not(apps_init(), "Bad app failed to init");
}
END_TEST

START_TEST(test_apps_events_register_nonexistent) {
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(BAD_CONFIG_NONEXISTENT, 1, sizeof(BAD_CONFIG_NONEXISTENT), f);
	fclose(f);
	
	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);
	
	test(option_parse_args(argc, argv, NULL), "File ready");
	test(option_parse_config_file(NULL, NULL, 0, NULL), "Config loaded");
	
	test_not(apps_init(), "Bad app failed to init");
}
END_TEST

START_TEST(test_apps_events_on_0) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/test/event", client);
	
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_on_1) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/event", client);
	
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_off_0) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/test/event", client);
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	evs_client_unsub_client("/test/event", client);
	test_size_eq(utils_stats()->apps_client_handler_off, 1, "Single client unsubscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_off_1) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/event", client);
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	evs_client_unsub_client("/event", client);
	test_size_eq(utils_stats()->apps_client_handler_off, 1, "Single client unsubscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_handle_0) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/test/event", client);
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	g_string_assign(client->message->buffer, "/test/event:0:plain=");
	evs_server_handle(client);
	test_size_eq(utils_stats()->apps_client_handler, 1, "Single handler called");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_events_handle_1) {
	apps_register_events();
	client_t *client = u_client_create();
	
	evs_client_sub_client("/event", client);
	test_size_eq(utils_stats()->apps_client_handler_on, 1, "Single client subscribed");
	
	g_string_assign(client->message->buffer, "/event:0:plain=");
	evs_server_handle(client);
	test_size_eq(utils_stats()->apps_client_handler, 1, "Single handler called");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_register) {
	// Make sure our only app was registered
	test_size_eq(_apps->len, 2, "App registered");
	
	app_t *app = g_ptr_array_index(_apps, 0);
	test_str_eq(app->name, "test", "Test app registered");
	test_str_eq(app->event_prefix, "/test", "Test app registered");
	
	app = g_ptr_array_index(_apps, 1);
	test_str_eq(app->name, "test2", "Test2 app registered");
	test_ptr_eq(app->event_prefix, NULL, "Test2 app registered");
}
END_TEST

START_TEST(test_apps_cb_prefork) {
	test(apps_prefork(), "Apps preforked");
	test_size_eq(utils_stats()->apps_prefork, 2, "Test apps preforked");
}
END_TEST

START_TEST(test_apps_cb_postfork) {
	test(apps_postfork(), "Apps postforked");
	test_size_eq(utils_stats()->apps_postfork, 2, "Test apps postforked");
}
END_TEST

START_TEST(test_apps_cb_run) {
	test(apps_run(), "Apps postforked");
	
	// Give the app time to get going
	usleep(MS_TO_USEC(100));
	
	test_size_eq(utils_stats()->apps_run, 2, "Test apps running");
}
END_TEST

START_TEST(test_apps_cb_client_connect) {
	client_t *client = u_client_create();
	
	apps_client_connect(client);
	test_size_eq(utils_stats()->apps_client_connect, 2, "Single client connected, 2 apps fired");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_apps_cb_client_close) {
	client_t *client = u_client_create();
	
	apps_client_close(client);
	test_size_eq(utils_stats()->apps_client_close, 2, "Single client disconnected, 2 apps fired");
	
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
	test_size_eq(utils_stats()->apps_client_subscribe, 2, "Single client subscribed, 2 apps fired");
	
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
	test_size_eq(utils_stats()->apps_client_unsubscribe, 2, "Single client unsubscribed, 2 apps fired");
	
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
	tcase_add_test(tc, test_apps_events_register_bad);
	tcase_add_test(tc, test_apps_events_register_nonexistent);
	tcase_add_test(tc, test_apps_events_on_0);
	tcase_add_test(tc, test_apps_events_on_1);
	tcase_add_test(tc, test_apps_events_off_0);
	tcase_add_test(tc, test_apps_events_off_1);
	tcase_add_test(tc, test_apps_events_handle_0);
	tcase_add_test(tc, test_apps_events_handle_1);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Callbacks");
	tcase_add_checked_fixture(tc, _apps_setup, _apps_teardown);
	tcase_add_test(tc, test_apps_cb_register);
	tcase_add_test(tc, test_apps_cb_prefork);
	tcase_add_test(tc, test_apps_cb_postfork);
	tcase_add_test(tc, test_apps_cb_run);
	tcase_add_test(tc, test_apps_cb_client_connect);
	tcase_add_test(tc, test_apps_cb_client_close);
	tcase_add_test(tc, test_apps_cb_client_subscribe);
	tcase_add_test(tc, test_apps_cb_client_unsubscribe);
	suite_add_tcase(s, tc);
	
	return s;
}