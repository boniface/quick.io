#include "test.h"

void _test_evs_client_setup() {
	utils_stats_setup();
	option_parse_config_file(NULL, NULL, 0, NULL);
	apps_init();
	evs_client_init();
	evs_server_init();
}

void _test_evs_client_teardown() {
	utils_stats_teardown();
}

START_TEST(test_evs_client_get_subscription_no_clients) {
	evs_client_sub_t *sub = _get_subscription("/test/event", FALSE);
	test_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_get_subscription_not_valid) {
	evs_client_sub_t *sub = _get_subscription("/what/is/that", FALSE);
	test_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_get_subscription_and_create) {
	evs_client_sub_t *sub = _get_subscription("/test/event", TRUE);
	
	test_str_eq(sub->event_path, "/test/event", "Not created when no clients");
	test(sub->handler != NULL, "Handler is set");
	test(sub->clients != NULL, "Clients table is set");
	test_uint32_eq(g_hash_table_size(sub->clients), 0, "No clients in table");
	test(sub->extra == NULL, "No extra parameters");
	test_uint32_eq(sub->extra_len, 0, "No extra parameters");
	
	test(g_hash_table_lookup(_events, sub->event_path) != NULL, "Event registered");
}
END_TEST

START_TEST(test_evs_client_get_subscription_and_create_invalid) {
	evs_client_sub_t *sub = _get_subscription("/test/not/an/event", TRUE);
	test(sub == NULL, "Bad event, not created");
}
END_TEST

START_TEST(test_evs_client_format_message_0) {
	client_t *client = u_client_create();
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event", client), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, NULL, d_plain, "some data", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event:0:plain=some data", "Message formatted correctly");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_1) {
	client_t *client = u_client_create();
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event", client), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, NULL, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_2) {
	client_t *client = u_client_create();
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event/test:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_3) {
	client_t *client = u_client_create();
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(NULL, 0, 0, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_UNKNOWN_EVENT, "Could not find handler");
	test_size_eq(buffer->len, 0, "No message set");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_callback) {
	client_t *client = u_client_create();
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(NULL, 1, 0, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/callback/1:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_server_callback) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(NULL, 1, 123, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/callback/1:123:plain=ßäū€öł", "Message formatted correctly");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, extra, d_plain, "abcd", buffer);
	
	test_status_eq(status, CLIENT_INVALID_SUBSCRIPTION, "No clients subscribed");
	test_size_eq(buffer->len, 0, "No message set");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed_callback) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra, NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 123, 0, extra, d_plain, "abcd", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/callback/123:0:plain=abcd", "Message formatted correctly");
	
	g_list_free_full(extra, g_free);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_ready) {
	client_t *client = u_client_create();
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;
	
	evs_client_client_ready(client);
	evs_client_sub_t *sub = _get_subscription(UNSUBSCRIBED, FALSE);
	
	test(client->subs != NULL, "Subscriptions setup");
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	test_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "Added to unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("test", client), CLIENT_INVALID_SUBSCRIPTION, "Subscription not found");
	test_size_eq(client->subs->len, 0, "No subscriptions");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_too_many_subscriptions) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	
	guint64 max = option_max_subscriptions();
	while (max--) {
		g_ptr_array_add(client->subs, NULL);
	}
	
	test_status_eq(evs_client_sub_client("/test/event/test2", client), CLIENT_TOO_MANY_SUBSCRIPTIONS, "Subscription not found");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_already_subscribed) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_ALREADY_SUBSCRIBED, "Subscription not found");
	
	evs_client_sub_t *sub = _get_subscription("/test/event/test", FALSE);
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	test_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "In subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	evs_client_sub_t *sub = _get_subscription(UNSUBSCRIBED, FALSE);
	test_size_eq(client->subs->len, 1, "In unsubscribed");
	test_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "In unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 1 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_bad_subscription) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_CANNOT_UNSUBSCRIBE, "Subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_not_subscribed) {
	client_t *client = u_client_create();
	
	test(_get_subscription("/test/event/test", TRUE) != NULL, "Subscription created");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_CANNOT_UNSUBSCRIBE, "Subscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_0) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_1) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test2", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test3", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_2) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client), CLIENT_GOOD, "Subscribed");
	
	test_status_eq(evs_client_unsub_client("/test/event/test3", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test2", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean_unsubscribed) {
	client_t *client = u_client_create();
	
	DEBUG("1");
	evs_client_client_ready(client);
	DEBUG("2");
	evs_client_client_clean(client);
	DEBUG("3");
	
	test_ptr_eq(client->subs, NULL, "No subscriptions");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscribes sent");
	
	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean) {
	client_t *client = u_client_create();
	
	test_status_eq(evs_client_sub_client("/test/event/test1", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client), CLIENT_GOOD, "Subscribed");
	
	evs_client_client_clean(client);
	
	test_ptr_eq(client->subs, NULL, "No subscriptions");
	
	evs_client_sub_t *sub = _get_subscription(UNSUBSCRIBED, FALSE);
	test_size_eq(g_hash_table_size(sub->clients), 0, "No more clients UNSUBSCRIBED");
	
	sub = _get_subscription("/test/event/test1", FALSE);
	test_size_eq(g_hash_table_size(sub->clients), 0, "No more clients subscribed 1");
	
	sub = _get_subscription("/test/event/test2", FALSE);
	test_size_eq(g_hash_table_size(sub->clients), 0, "No more clients subscribed 2");
	
	sub = _get_subscription("/test/event/test3", FALSE);
	test_size_eq(g_hash_table_size(sub->clients), 0, "No more clients subscribed 3");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Unsubscribes sent to client close");
	
	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean_bad_client) {
	client_t *client = u_client_create();
	
	// Nothing to test, just make sure it doesn't segfault
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;
	
	evs_client_client_clean(client);
	
	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_pub_messages_0) {
	
}
END_TEST

Suite* events_client_suite() {
	TCase *tc;
	Suite *s = suite_create("Events - Client");
	
	tc = tcase_create("getSubscription");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_get_subscription_no_clients);
	tcase_add_test(tc, test_evs_client_get_subscription_not_valid);
	tcase_add_test(tc, test_evs_client_get_subscription_and_create);
	tcase_add_test(tc, test_evs_client_get_subscription_and_create_invalid);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Format Message");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_format_message_0);
	tcase_add_test(tc, test_evs_client_format_message_1);
	tcase_add_test(tc, test_evs_client_format_message_2);
	tcase_add_test(tc, test_evs_client_format_message_3);
	tcase_add_test(tc, test_evs_client_format_message_with_callback);
	tcase_add_test(tc, test_evs_client_format_message_with_server_callback);
	tcase_add_test(tc, test_evs_client_format_message_not_subscribed);
	tcase_add_test(tc, test_evs_client_format_message_not_subscribed_callback);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Client Ready");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_ready);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Subscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_subscribe_bad_subscription);
	tcase_add_test(tc, test_evs_client_subscribe_too_many_subscriptions);
	tcase_add_test(tc, test_evs_client_subscribe_already_subscribed);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Unsubscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_unsubscribe);
	tcase_add_test(tc, test_evs_client_unsubscribe_bad_subscription);
	tcase_add_test(tc, test_evs_client_unsubscribe_not_subscribed);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Subscribe + Unsubscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_sub_unsub_0);
	tcase_add_test(tc, test_evs_client_sub_unsub_1);
	tcase_add_test(tc, test_evs_client_sub_unsub_2);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Client Cleaning");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_clean_unsubscribed);
	tcase_add_test(tc, test_evs_client_clean);
	tcase_add_test(tc, test_evs_client_clean_bad_client);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Publish Messages");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_pub_messages_0);
	suite_add_tcase(s, tc);
	
	return s;
}