#include "test.h"

void _test_event_creation_setup() {
	evs_server_init();
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
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
	client_t *client = u_client_create();
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/noop:123:plain");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	
	_event_free(&event);
	u_client_free(client);
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
	suite_add_tcase(s, tc);
	
	return s;
}