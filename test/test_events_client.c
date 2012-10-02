/**
 * @attention The event used in this file, /test/.*, is registered in app/test.c (the test
 * app used for unit testing apps.  It's loaded with apps_init() here.
 */

#include "test.h"

#define PUB_EVENT "\x81""\x1d""/test/event:0:plain=something"
#define INVALID_EVENT "\x81""\x2b""/qio/callback/1:0:plain=invalid:/test/event"
#define INVALID_EVENT_EXTRA "\x81""\x3a""/qio/callback/1:0:plain=invalid:/test/event/test/something"

#define ASYNC_CALLBACK "\x81""\x1d""/qio/callback/654987:0:plain="

void _test_evs_client_setup() {
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	evs_client_init();
	evs_server_init();
	conns_init();
	apps_run();
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
	test_uint64_eq(sub->extra->len, 0, "No extra parameters");
	
	test(g_hash_table_lookup(_events, sub->event_path) != NULL, "Event registered");
}
END_TEST

START_TEST(test_evs_client_get_subscription_and_create_invalid) {
	evs_client_sub_t *sub = _get_subscription("/test/not/an/event", TRUE);
	test(sub == NULL, "Bad event, not created");
}
END_TEST

START_TEST(test_evs_client_format_message_0) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, NULL, d_plain, "some data", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event:0:plain=some data", "Message formatted correctly");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_1) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, NULL, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_2) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	test_str_eq(buffer->str, "/test/event/test:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_no_handler) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	
	status_t status = evs_client_format_message(NULL, 0, 0, NULL, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_INVALID_SUBSCRIPTION, "Could not find handler");
	test_size_eq(buffer->len, 0, "No message set");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_invalid_handler) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	
	status_t status = evs_client_format_message((const event_handler_t *)1000, 0, 0, NULL, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_INVALID_SUBSCRIPTION, "Could not find handler");
	test_size_eq(buffer->len, 0, "No message set");
	
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_callback) {
	client_t *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(NULL, 1, 0, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/qio/callback/1:0:plain=ßäū€öł", "Message formatted correctly");
	
	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_server_callback) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(NULL, 1, 123, extra, d_plain, "ßäū€öł", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/qio/callback/1:123:plain=ßäū€öł", "Message formatted correctly");
	
	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 0, 0, extra, d_plain, "abcd", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Message formatted even though no clients subscribed");
	test_str_eq(buffer->str, "/test/event/test:0:plain=abcd", "Message formatted correctly");
	
	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed_callback) {
	GString *buffer = g_string_sized_new(1);
	
	path_extra_t extra = NULL;
	event_handler_t *handler = evs_server_get_handler("/test/event/test", &extra);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_format_message(handler, 123, 0, extra, d_plain, "abcd", buffer);
	
	test_status_eq(status, CLIENT_GOOD, "Got message");
	test_str_eq(buffer->str, "/qio/callback/123:0:plain=abcd", "Message formatted correctly");
	
	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_ready) {
	client_t *client = u_client_create(NULL);
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;
	
	evs_client_client_ready(client);
	test(client->subs != NULL, "Subscriptions setup");
	test_size_eq(client->subs->len, 0, "No subscriptions");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("test", client, 0), CLIENT_INVALID_SUBSCRIPTION, "Subscription not found");
	test_size_eq(client->subs->len, 0, "No subscriptions");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_too_many_subscriptions) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	
	guint64 max = option_max_subscriptions();
	while (max--) {
		g_ptr_array_add(client->subs, NULL);
	}
	
	test_status_eq(evs_client_sub_client("/test/event/test2", client, 0), CLIENT_TOO_MANY_SUBSCRIPTIONS, "Subscription not found");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_already_subscribed) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_ALREADY_SUBSCRIBED, "Subscription not found");
	
	evs_client_sub_t *sub = _get_subscription("/test/event/test", FALSE);
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	test_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "In subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_reject_child) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/children/reject", client, 0), CLIENT_INVALID_SUBSCRIPTION, "Rejected");
	test_status_eq(evs_client_sub_client("/test/children/reject/hi", client, 0), CLIENT_INVALID_SUBSCRIPTION, "Rejected");
	test_status_eq(evs_client_sub_client("/test/children/reject/another", client, 0), CLIENT_INVALID_SUBSCRIPTION, "Rejected");
	
	test_size_eq(client->subs->len, 0, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscriptions");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscriptions");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async_with_callback) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/async", client, 654987), CLIENT_ASYNC, "Async");
	
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
	
	evs_client_send_messages();
	
	char buff[1000];
	memset(buff, 0, sizeof(buff));
	test_size_eq(read(socket, buff, sizeof(buff)-1), sizeof(ASYNC_CALLBACK)-1, "Correct len");
	test_bin_eq(buff, ASYNC_CALLBACK, sizeof(ASYNC_CALLBACK)-1, "Correct callback sent");
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 async callback sent");
}
END_TEST

START_TEST(test_evs_client_subscribe_async_without_callback) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
	
	evs_client_send_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 0, "1 async callback sent");
}
END_TEST

START_TEST(test_evs_client_unsubscribe) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(client->subs->len, 0, "No subscriptions");
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 1 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_bad_subscription) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_CANNOT_UNSUBSCRIBE, "Subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_not_subscribed) {
	client_t *client = u_client_create(NULL);
	
	test(_get_subscription("/test/event/test", TRUE) != NULL, "Subscription created");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_CANNOT_UNSUBSCRIBE, "Subscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_0) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_1) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test2", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test3", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_2) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client, 0), CLIENT_GOOD, "Subscribed");
	
	test_status_eq(evs_client_unsub_client("/test/event/test3", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test2", client), CLIENT_GOOD, "Unsubscribed");
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_GOOD, "Unsubscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test1", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test2", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test3", client, 0), CLIENT_GOOD, "Subscribed");
	
	evs_client_client_close(client);
	
	test_ptr_eq(client->subs, NULL, "No subscriptions");
	
	evs_client_sub_t *sub;
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
	client_t *client = u_client_create(NULL);
	
	// Nothing to test, just make sure it doesn't segfault
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;
	
	evs_client_client_close(client);
	
	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_send_messages_empty) {
	// Mainly for coverage
	evs_client_send_messages();
}
END_TEST

START_TEST(test_evs_client_pub_message) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	evs_client_send_messages();
	
	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(PUB_EVENT)-1, "Correct message size sent");
	test_bin_eq(buff, PUB_EVENT, sizeof(PUB_EVENT)-1, "Correct message sent");
	
	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_send_messages_closed_client) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	evs_client_send_messages();
	
	evs_client_sub_t *sub = _get_subscription("/test/event", FALSE);
	test_size_eq(g_hash_table_size(sub->clients), 0, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_messages_no_client_handler) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	evs_client_send_messages();
	
	evs_client_sub_t *sub = _get_subscription("/test/event", FALSE);
	
	// Client not allocated with a socket, so will be closed
	test_size_eq(g_hash_table_size(sub->clients), 0, "Client closed");
	
	test_size_eq(utils_stats()->evs_client_pubd_messages, 1, "1 message sent");
	test_size_eq(utils_stats()->evs_client_pub_closes, 1, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_messages_clear_subscription) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	conns_client_close(client);
	
	// Get rid of the empty subscription
	evs_client_cleanup();
	
	// Attempt to publish to an empty subscription
	evs_client_send_messages();
	
	test_size_eq(utils_stats()->evs_client_pubd_messages, 0, "1 message sent");
	test_size_eq(utils_stats()->evs_client_pub_closes, 0, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_messages_no_handler) {
	status_t status = evs_client_pub_event(NULL, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_INVALID_SUBSCRIPTION, "Message published");
	}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_0) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	
	evs_client_invalid_event(client, handler, NULL, 0);
	
	evs_client_send_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 0, "0 messages sent");
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_1) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	
	evs_client_invalid_event(client, handler, NULL, 1);
	
	evs_client_send_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 message sent");
	
	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(INVALID_EVENT)-1, "Message sent");
	test_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct invalid response");
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_extra) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	
	path_extra_t extra = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(extra, "test");
	g_ptr_array_add(extra, "something");
	evs_client_invalid_event(client, handler, extra, 1);
	
	evs_client_send_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 message sent");
	
	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(INVALID_EVENT_EXTRA)-1, "Message sent");
	test_bin_eq(buff, INVALID_EVENT_EXTRA, sizeof(INVALID_EVENT_EXTRA)-1, "Correct invalid response");
}
END_TEST

START_TEST(test_evs_client_number_subscribed_0) {
	client_t *client = u_client_create(NULL);
	
	test_uint64_eq(evs_client_number_subscribed("/test/event", NULL), 0, "No Subscriptions");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_number_subscribed_1) {
	client_t *client = u_client_create(NULL);
	
	evs_client_sub_client("/test/event", client, 0);
	test_uint64_eq(evs_client_number_subscribed("/test/event", NULL), 1, "Single subscription");
	
	u_client_free(client);
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
	tcase_add_test(tc, test_evs_client_format_message_no_handler);
	tcase_add_test(tc, test_evs_client_format_message_invalid_handler);
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
	tcase_add_test(tc, test_evs_client_subscribe_reject_child);
	tcase_add_test(tc, test_evs_client_subscribe_async);
	tcase_add_test(tc, test_evs_client_subscribe_async_with_callback);
	tcase_add_test(tc, test_evs_client_subscribe_async_without_callback);
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
	tcase_add_test(tc, test_evs_client_clean);
	tcase_add_test(tc, test_evs_client_clean_bad_client);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Publish Messages");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_send_messages_empty);
	tcase_add_test(tc, test_evs_client_pub_message);
	tcase_add_test(tc, test_evs_client_send_messages_closed_client);
	tcase_add_test(tc, test_evs_client_send_messages_no_client_handler);
	tcase_add_test(tc, test_evs_client_send_messages_clear_subscription);
	tcase_add_test(tc, test_evs_client_send_messages_no_handler);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Async Message");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_0);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_1);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_extra);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Subscribed Count");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_number_subscribed_0);
	tcase_add_test(tc, test_evs_client_number_subscribed_1);
	suite_add_tcase(s, tc);
	
	return s;
}