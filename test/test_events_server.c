#include "test.h"

void _test_event_creation_setup() {
	apps_init();
	evs_server_init();
	evs_client_init();
}

START_TEST(test_evs_server_clean_name_0) {
	gchar *path = _clean_event_name("///some////slashes//////////");
	test_str_eq(path, "/some/slashes", "Extra slashes removed");
	g_free(path);
}
END_TEST

START_TEST(test_evs_server_clean_name_1) {
	gchar *path = _clean_event_name("some/slashes");
	test_str_eq(path, "/some/slashes", "Starting slash added");
	g_free(path);
}
END_TEST

START_TEST(test_evs_server_clean_name_2) {
	gchar *path = _clean_event_name("/some/slashes/");
	test_str_eq(path, "/some/slashes", "Trailing slash removed");
	g_free(path);
}
END_TEST

START_TEST(test_evs_event_creation_valid_minimal) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop::plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	
	// Test the event data
	test_str_eq(event.name, "/noop", "Correct event name");
	test_ptr_eq(event.extra_segments, NULL, "No extra segments");
	test_uint16_eq(event.extra_segments_len, 0, "No extra segments");
	test_uint32_eq(event.callback, 0, "No callback");
	test(event.type == d_plain, "Plain data type set");
	test_str_eq(event.data, "", "No data set");
	
	// Test the side-effects
	event_handler_t *right_handler = evs_server_get_handler(event.name, NULL, NULL);
	test_ptr_eq(handler, right_handler, "Correct handler retrieved");
	test_size_eq(message->buffer->len, 0, "Buffer cleared");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_valid_callback) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:123:plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.callback, 123, "Callback gotten!");
	test_str_eq(event.data, "", "No data set");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_valid_json) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:123:json={\"test\":1}");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.callback, 123, "Callback gotten!");
	test(event.type == d_json, "JSON data type set");
	test_str_eq(event.data, "{\"test\":1}", "JSON data recieved");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_no_data) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:123:plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.callback, 123, "Callback gotten!");
	test(event.type == d_plain, "Plain data type set");
	test_str_eq(event.data, "", "No data set");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_handle_children) {
	client_t *client = u_client_create(NULL);
	
	event_handler_t *expected = evs_server_on("/multi", NULL, NULL, NULL, TRUE);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/multi/something:123:plain=test");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_ptr_eq(handler, expected, "Got correct handler");
	test_uint32_eq(event.callback, 123, "Callback gotten!");
	test(event.type == d_plain, "Plain data type set");
	test_str_eq(event.data, "test", "Plain data recieved");
	
	// Segment testing
	test_uint16_eq(event.extra_segments_len, 1, "1 extra segment");
	test_str_eq(event.extra_segments->data, "something", "Correct extra segment");
	
	_event_free(&event);
	
	test_ptr_eq(event.name, NULL, "Name free'd");
	test_ptr_eq(event.extra_segments, NULL, "Extra segments free'd");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "::=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_with_event) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop::=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_callback_id) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:abcd1234:=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	test_uint32_eq(event.callback, 0, "Callback parsed");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_callback) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_junk) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:\x11\x01\x12\xab\x00:=hi");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_data) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:123:plain");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_handler) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/:123:plain");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_UNKNOWN_EVENT, "Invalid event");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_new_handler_bad_path) {
	test_ptr_eq(evs_server_on("/:event", NULL, NULL, NULL, FALSE), NULL, "Bad path");
}
END_TEST

START_TEST(test_evs_get_handler_0) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("noop", &extra, &extra_len);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_1) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("noop/", &extra, &extra_len);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_2) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("/noop/", &extra, &extra_len);
	test_ptr_eq(handler, evs_server_get_handler("/noop", NULL, NULL), "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_3) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("/noop/extra/segs", &extra, &extra_len);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_4) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("/noop/////", &extra, &extra_len);
	test_ptr_eq(handler, evs_server_get_handler("/noop", NULL, NULL), "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_5) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *handler = evs_server_get_handler("/noop\x00/more/items", &extra, &extra_len);
	test_ptr_eq(handler, evs_server_get_handler("/noop", NULL, NULL), "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
	test_uint16_eq(extra_len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_6) {
	path_extra_t extra;
	guint16 extra_len;
	
	event_handler_t *expected = evs_server_on("/multi", NULL, NULL, NULL, TRUE);
	
	event_handler_t *handler = evs_server_get_handler("/multi/handler", &extra, &extra_len);
	test_ptr_eq(handler, expected, "Correct handler retrieved");
	test_uint16_eq(extra_len, 1, "1 extra segment");
	test_str_eq(extra->data, "handler", "Correct extra segment");
	
	g_list_free_full(extra, g_free);
}
END_TEST

START_TEST(test_evs_path_from_handler) {
	event_handler_t *handler = g_hash_table_lookup(_events_by_path, "/noop");
	
	test_str_eq(evs_server_path_from_handler(handler), "/noop", "Found correct event");
}
END_TEST

START_TEST(test_evs_handler_noop) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/noop:0:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Noop recieved");
	test_size_eq(client->message->buffer->len, 0, "No data set");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_noop_callback) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/noop:123:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Noop recieved");
	test_str_eq(client->message->buffer->str, "/callback/123:0:plain=", "Callback with no data");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_ping) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/ping:123:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/123:0:plain=test", "Ping sent back correct data");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/sub:456:plain=/doesnt/exist");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_INVALID_SUBSCRIPTION":/doesnt/exist", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_to_unsubscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/sub:456:plain=0");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_INVALID_SUBSCRIPTION":0", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_too_many_subs) {
	client_t *client = u_client_create(NULL);
	
	guint64 max = option_max_subscriptions();
	while (max--) {
		g_ptr_array_add(client->subs, NULL);
	}
	
	g_string_assign(client->message->buffer, "/sub:456:plain=/noop");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_MAX_SUBSCRIPTIONS":/noop", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_already_subscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/sub:456:plain=/noop");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Subscribed");
	test_size_eq(client->message->buffer->len, 0, "Subscribed!");
	
	g_string_assign(client->message->buffer, "/sub:456:plain=/noop");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_ALREADY_SUBSCRIBED":/noop", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/sub:456:plain=/noop");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Subscribed");
	test_size_eq(client->message->buffer->len, 0, "Subscribed!");
	
	g_string_assign(client->message->buffer, "/unsub:456:plain=/noop");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Unsubscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe_not_subscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/unsub:456:plain=/noop");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_CANNOT_UNSUBSCRIBE":/noop", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe_unsubscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/unsub:456:plain=0");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/callback/456:0:plain="EVENT_RESPONSE_CANNOT_UNSUBSCRIBE":0", "Bad event");
	
	u_client_free(client);
}
END_TEST

Suite* events_server_suite() {
	TCase *tc;
	Suite *s = suite_create("Events - Server");
	
	tc = tcase_create("Name Cleaning");
	tcase_add_test(tc, test_evs_server_clean_name_0);
	tcase_add_test(tc, test_evs_server_clean_name_1);
	tcase_add_test(tc, test_evs_server_clean_name_2);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Event Creation");
	tcase_add_checked_fixture(tc, _test_event_creation_setup, NULL);
	tcase_add_test(tc, test_evs_event_creation_valid_minimal);
	tcase_add_test(tc, test_evs_event_creation_valid_callback);
	tcase_add_test(tc, test_evs_event_creation_valid_json);
	tcase_add_test(tc, test_evs_event_creation_no_data);
	tcase_add_test(tc, test_evs_event_creation_handle_children);
	tcase_add_test(tc, test_evs_event_creation_invalid);
	tcase_add_test(tc, test_evs_event_creation_invalid_with_event);
	tcase_add_test(tc, test_evs_event_creation_invalid_callback_id);
	tcase_add_test(tc, test_evs_event_creation_invalid_callback);
	tcase_add_test(tc, test_evs_event_creation_invalid_junk);
	tcase_add_test(tc, test_evs_event_creation_invalid_data);
	tcase_add_test(tc, test_evs_event_creation_invalid_handler);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Handler Creation");
	tcase_add_checked_fixture(tc, _test_event_creation_setup, NULL);
	tcase_add_test(tc, test_evs_new_handler_bad_path);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Handler Getting");
	tcase_add_checked_fixture(tc, _test_event_creation_setup, NULL);
	tcase_add_test(tc, test_evs_get_handler_0);
	tcase_add_test(tc, test_evs_get_handler_1);
	tcase_add_test(tc, test_evs_get_handler_2);
	tcase_add_test(tc, test_evs_get_handler_3);
	tcase_add_test(tc, test_evs_get_handler_4);
	tcase_add_test(tc, test_evs_get_handler_5);
	tcase_add_test(tc, test_evs_get_handler_6);
	tcase_add_test(tc, test_evs_path_from_handler);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Default Event Handlers");
	tcase_add_checked_fixture(tc, _test_event_creation_setup, NULL);
	tcase_add_test(tc, test_evs_handler_noop);
	tcase_add_test(tc, test_evs_handler_noop_callback);
	tcase_add_test(tc, test_evs_handler_ping);
	tcase_add_test(tc, test_evs_handler_subscribe);
	tcase_add_test(tc, test_evs_handler_subscribe_to_unsubscribed);
	tcase_add_test(tc, test_evs_handler_subscribe_too_many_subs);
	tcase_add_test(tc, test_evs_handler_subscribe_already_subscribed);
	tcase_add_test(tc, test_evs_handler_unsubscribe);
	tcase_add_test(tc, test_evs_handler_unsubscribe_not_subscribed);
	tcase_add_test(tc, test_evs_handler_unsubscribe_unsubscribed);
	suite_add_tcase(s, tc);
	
	return s;
}