/**
 * @attention The event used in this file, /test/.*, is registered in app/test.c (the test
 * app used for unit testing apps.  It's loaded with apps_init() here.
 */

#include "test.h"

#define PUB_EVENT "\x81""\x1d""/test/event:0:plain=something"
#define INVALID_EVENT "\x81""\x21""/qio/callback/1:0:plain=qio_error"

#define ASYNC_CALLBACK "\x81""\x1d""/qio/callback/654987:0:plain="
#define HEARTBEAT_EVENT "\x81""\x17""/qio/heartbeat:0:plain="

void _test_evs_client_setup() {
	qev_init();
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	evs_server_init();
	evs_client_init();
	conns_init();
	apps_run();
	test(stats_init());
}

void _test_evs_client_teardown() {
	utils_stats_teardown();
}

START_TEST(test_evs_client_subscription_get_no_clients) {
	evs_client_sub_t *sub = _subscription_get("/test/event", FALSE);
	test_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_subscription_get_not_valid) {
	evs_client_sub_t *sub = _subscription_get("/what/is/that", FALSE);
	test_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_subscription_get_and_create) {
	evs_client_sub_t *sub = _subscription_get("/test/event", TRUE);
	
	test_str_eq(sub->event_path, "/test/event", "Not created when no clients");
	test(sub->handler != NULL, "Handler is set");
	test(sub->clients != NULL, "Clients table is set");
	test_uint32_eq(g_hash_table_size(sub->clients), 0, "No clients in table");
	test_uint64_eq(sub->extra->len, 0, "No extra parameters");
	
	test(g_hash_table_lookup(_events, sub->event_path) != NULL, "Event registered");
}
END_TEST

START_TEST(test_evs_client_subscription_get_and_create_invalid) {
	evs_client_sub_t *sub = _subscription_get("/test/not/an/event", TRUE);
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
	
	test_status_eq(status, CLIENT_ERROR, "Could not find handler");
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
	
	test_status_eq(status, CLIENT_ERROR, "Could not find handler");
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

START_TEST(test_evs_heartbeat) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	conns_client_new(client);
	client->state = cstate_running;
	
	evs_client_heartbeat();
	evs_client_send_async_messages();
	
	char buff[512];
	memset(buff, 0, sizeof(buff));
	test_size_eq(read(socket, buff, sizeof(buff)-1), sizeof(HEARTBEAT_EVENT)-1, "Correct len");
	test_bin_eq(buff, HEARTBEAT_EVENT, sizeof(HEARTBEAT_EVENT)-1, "Correct event sent");
	
	test_size_eq(stats.messages_sent, 1, "1 heartbeat sent");
	
	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat_no_clients) {
	evs_client_heartbeat();
	evs_client_send_async_messages();
	
	test_size_eq(stats.messages_sent, 0, "0 heartbeats sent");
}
END_TEST

START_TEST(test_evs_heartbeat_yield) {
	int socket = 0;
	
	for (int i = 0; i < CONNS_YIELD*2; i++) {
		client_t *client = u_client_create(&socket);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;
	}
	
	evs_client_heartbeat();
	evs_client_send_async_messages();
	
	char buff[512];
	memset(buff, 0, sizeof(buff));
	test_size_eq(read(socket, buff, sizeof(buff)-1), sizeof(HEARTBEAT_EVENT)-1, "Correct len");
	test_bin_eq(buff, HEARTBEAT_EVENT, sizeof(HEARTBEAT_EVENT)-1, "Correct event sent");
	
	test_size_eq(stats.messages_sent, CONNS_YIELD*2, "Heartbeats sent");
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("test", client, 0), CLIENT_ERROR, "Subscription not found");
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
	
	test_status_eq(evs_client_sub_client("/test/event/test2", client, 0), CLIENT_ERROR, "Subscription not found");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0 * option_apps_count(), "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_already_subscribed) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD, "Subscribed");
	test_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_ERROR, "Subscription not found");
	
	evs_client_sub_t *sub = _subscription_get("/test/event/test", FALSE);
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	test_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "In subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "Only real subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "Only real unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_reject_child) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/children/reject", client, 0), CLIENT_ERROR, "Rejected");
	test_status_eq(evs_client_sub_client("/test/children/reject/hi", client, 0), CLIENT_ERROR, "Rejected");
	test_status_eq(evs_client_sub_client("/test/children/reject/another", client, 0), CLIENT_ERROR, "Rejected");
	
	test_size_eq(client->subs->len, 0, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscriptions");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscriptions");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	test_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");
	
	evs_client_send_async_messages();
	
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
	test_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");
	
	evs_client_send_async_messages();
	
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
	
	char buff[512];
	memset(buff, 0, sizeof(buff));
	test_size_eq(read(socket, buff, sizeof(buff)-1), sizeof(ASYNC_CALLBACK)-1, "Correct len");
	test_bin_eq(buff, ASYNC_CALLBACK, sizeof(ASYNC_CALLBACK)-1, "Correct callback sent");
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 async callback sent");
}
END_TEST

START_TEST(test_evs_client_subscribe_async_without_callback) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	test_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");
	
	evs_client_send_async_messages();
	test_size_eq(client->subs->len, 1, "Only 1 subscription");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
	
	evs_client_send_async_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 0, "0 async callbacks sent");
}
END_TEST

START_TEST(test_evs_client_subscribe_async_reject) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/async/reject", client, 1), CLIENT_ASYNC, "Async");
	test_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");
	
	evs_client_send_async_messages();
	test_size_eq(client->subs->len, 0, "Subscription rejected");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0 * option_apps_count(), "All rejected");
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 async callback sent");
	
	gchar buff[512];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(INVALID_EVENT)-1, "Correct message size sent");
	test_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct message sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async_reject_too_many_subscriptions) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/async", client, 1), CLIENT_ASYNC, "Async");
	test_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");
	
	for (guint32 i = 0; i <= option_max_subscriptions(); i++) {
		gchar ep[128];
		snprintf(ep, sizeof(ep), "/test/event/%d", i);
		evs_client_sub_client(ep, client, 1);
	}
	test_size_eq(client->subs->len, option_max_subscriptions(), "Has max subscriptions");
	
	evs_client_send_async_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 async callback sent");
	
	gchar buff[512];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(INVALID_EVENT)-1, "Correct message size sent");
	test_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct message sent");
	
	u_client_free(client);
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
	
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_ERROR, "Subscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_not_subscribed) {
	client_t *client = u_client_create(NULL);
	
	evs_client_sub_t *sub = _subscription_get("/test/event/test", TRUE);
	test(sub != NULL, "Subscription created");
	g_mutex_unlock(&sub->lock);
	
	test_status_eq(evs_client_unsub_client("/test/event/test", client), CLIENT_ERROR, "Subscribed");
	
	test_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscribes sent");
	test_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscribes sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscription_cleanup_0) {
	// Just make sure the test completes without segfaulting
	_subscription_cleanup("/not/gonna/happen");
}
END_TEST

START_TEST(test_evs_client_subscription_cleanup_1) {
	// Just make sure the test completes without segfaulting
	client_t *client = u_client_create(NULL);
	evs_client_sub_t *sub = _subscription_get("/test/event/test", TRUE);
	g_hash_table_add(sub->clients, client);
	
	g_mutex_unlock(&sub->lock);
	
	_subscription_cleanup("/test/event/test");
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
	
	test_ptr_eq(_subscription_get("/test/event/test1", FALSE), NULL, "No more clients subscribed 1");
	test_ptr_eq(_subscription_get("/test/event/test2", FALSE), NULL, "No more clients subscribed 2");
	test_ptr_eq(_subscription_get("/test/event/test3", FALSE), NULL, "No more clients subscribed 3");
	
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

START_TEST(test_evs_client_send_async_messages_empty) {
	// Mainly for coverage
	evs_client_send_async_messages();
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
	
	evs_client_send_async_messages();
	
	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(PUB_EVENT)-1, "Correct message size sent");
	test_bin_eq(buff, PUB_EVENT, sizeof(PUB_EVENT)-1, "Correct message sent");
	
	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_send_async_messages_closed_client_broadcast) {
	// Client created without a socket == closed
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	test_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something"), CLIENT_GOOD, "Message published");
	test_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something1"), CLIENT_GOOD, "Message published");
	test_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something2"), CLIENT_GOOD, "Message published");
	test_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something3"), CLIENT_GOOD, "Message published");
	
	evs_client_send_async_messages();
	qev_debug_flush();
	
	test_ptr_eq(_subscription_get("/test/event", FALSE), NULL, "Client closed, sub freed");
	test_size_eq(utils_stats()->evs_client_send_pub_closes, 4, "Client closed in each pub");
}
END_TEST

START_TEST(test_evs_client_send_async_messages_closed_client_single) {
	// Client created without a socket == closed
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	
	evs_client_send_callback(client, 1, d_plain, "test", 0);
	evs_client_send_callback(client, 1, d_plain, "test1", 0);
	evs_client_send_callback(client, 1, d_plain, "test2", 0);
	evs_client_send_callback(client, 1, d_plain, "test3", 0);
	evs_client_send_callback(client, 1, d_plain, "test4", 0);
	evs_client_send_callback(client, 1, d_plain, "test5", 0);
	
	evs_client_send_async_messages();
	test_size_eq(utils_stats()->evs_client_send_single_closes, 6, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_async_messages_no_client_handler) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	evs_client_send_async_messages();
	qev_debug_flush();
	
	// Client not allocated with a socket, so will be closed
	test_ptr_eq(_subscription_get("/test/event", FALSE), NULL, "Client closed");
	
	test_size_eq(utils_stats()->evs_client_pubd_messages, 1, "1 message sent");
	test_size_eq(utils_stats()->evs_client_send_pub_closes, 1, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_async_messages_clear_subscription) {
	client_t *client = u_client_create(NULL);
	
	test_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test(handler != NULL, "Got handler");
	
	status_t status = evs_client_pub_event(handler, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_GOOD, "Message published");
	
	conns_client_close(client);
	
	// Attempt to publish to an empty subscription
	evs_client_send_async_messages();
	
	test_size_eq(utils_stats()->evs_client_pubd_messages, 0, "No messages sent");
	test_size_eq(utils_stats()->evs_client_send_pub_closes, 0, "Client closed");
}
END_TEST

START_TEST(test_evs_client_send_async_messages_no_handler) {
	status_t status = evs_client_pub_event(NULL, NULL, d_plain, "something");
	test_status_eq(status, CLIENT_ERROR, "Message published");
	}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_0) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	
	evs_client_app_sub_cb(client, handler, NULL, 0, FALSE);
	
	evs_client_send_async_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 0, "0 messages sent");
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_1) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	
	evs_client_app_sub_cb(client, handler, NULL, 1, FALSE);
	
	evs_client_send_async_messages();
	
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
	evs_client_app_sub_cb(client, handler, extra, 1, FALSE);
	
	evs_client_send_async_messages();
	
	test_size_eq(utils_stats()->evs_client_async_messages, 1, "1 message sent");
	
	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	test_int32_eq(len, sizeof(INVALID_EVENT)-1, "Message sent");
	test_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct invalid response");
}
END_TEST

START_TEST(test_evs_client_number_subscribed_0) {
	client_t *client = u_client_create(NULL);
	
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test_uint64_eq(evs_client_number_subscribed(handler, NULL), 0, "No Subscriptions");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_number_subscribed_1) {
	client_t *client = u_client_create(NULL);
	
	evs_client_sub_client("/test/event", client, 0);
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test_uint64_eq(evs_client_number_subscribed(handler, NULL), 1, "Single subscription");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_number_subscribed_2) {
	client_t *client = u_client_create(NULL);
	
	path_extra_t extra = g_ptr_array_new();
	g_ptr_array_add(extra, "something");
	
	evs_client_sub_client("/test/event/something", client, 0);
	event_handler_t *handler = evs_server_get_handler("/test/event", NULL);
	test_uint64_eq(evs_client_number_subscribed(handler, extra), 1, "Single subscription");
	
	g_ptr_array_free(extra, TRUE);
	u_client_free(client);
}
END_TEST

Suite* events_client_suite() {
	TCase *tc;
	Suite *s = suite_create("Events - Client");
	
	tc = tcase_create("getSubscription");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_subscription_get_no_clients);
	tcase_add_test(tc, test_evs_client_subscription_get_not_valid);
	tcase_add_test(tc, test_evs_client_subscription_get_and_create);
	tcase_add_test(tc, test_evs_client_subscription_get_and_create_invalid);
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
	
	tc = tcase_create("Heartbeat");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_heartbeat);
	tcase_add_test(tc, test_evs_heartbeat_no_clients);
	tcase_add_test(tc, test_evs_heartbeat_yield);
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
	tcase_add_test(tc, test_evs_client_subscribe_async_reject);
	tcase_add_test(tc, test_evs_client_subscribe_async_reject_too_many_subscriptions);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Unsubscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_unsubscribe);
	tcase_add_test(tc, test_evs_client_unsubscribe_bad_subscription);
	tcase_add_test(tc, test_evs_client_unsubscribe_not_subscribed);
	tcase_add_test(tc, test_evs_client_subscription_cleanup_0);
	tcase_add_test(tc, test_evs_client_subscription_cleanup_1);
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
	tcase_add_test(tc, test_evs_client_send_async_messages_empty);
	tcase_add_test(tc, test_evs_client_pub_message);
	tcase_add_test(tc, test_evs_client_send_async_messages_closed_client_broadcast);
	tcase_add_test(tc, test_evs_client_send_async_messages_closed_client_single);
	tcase_add_test(tc, test_evs_client_send_async_messages_no_client_handler);
	tcase_add_test(tc, test_evs_client_send_async_messages_clear_subscription);
	tcase_add_test(tc, test_evs_client_send_async_messages_no_handler);
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
	tcase_add_test(tc, test_evs_client_number_subscribed_2);
	suite_add_tcase(s, tc);
	
	return s;
}