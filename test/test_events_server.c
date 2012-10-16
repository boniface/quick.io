#include "test.h"

static void _test_events_setup() {
	utils_stats_setup();
	evs_server_init();
	evs_client_init();
	apps_run();
	conns_init();
	test(stats_init());
}

static void _test_events_teardown() {
	utils_stats_teardown();
}

START_TEST(test_evs_server_format_path_0) {
	gchar *path = evs_server_format_path("///some////slashes//////////", NULL);
	test_str_eq(path, "/some/slashes", "Extra slashes removed");
	g_free(path);
}
END_TEST

START_TEST(test_evs_server_format_path_1) {
	gchar *path = evs_server_format_path("some/slashes", NULL);
	test_str_eq(path, "/some/slashes", "Starting slash added");
	g_free(path);
}
END_TEST

START_TEST(test_evs_server_format_path_2) {
	gchar *path = evs_server_format_path("/some/slashes/", NULL);
	test_str_eq(path, "/some/slashes", "Trailing slash removed");
	g_free(path);
}
END_TEST

START_TEST(test_evs_server_format_path_3) {
	path_extra_t extra = g_ptr_array_new();
	g_ptr_array_add(extra, "with");
	g_ptr_array_add(extra, "path");
	
	gchar *path = evs_server_format_path("/some/slashes", extra);
	test_str_eq(path, "/some/slashes/with/path", "Path concated");
	g_free(path);
	
	g_ptr_array_free(extra, TRUE);
}
END_TEST

START_TEST(test_evs_server_format_path_4) {
	path_extra_t extra = g_ptr_array_new();
	g_ptr_array_add(extra, "with");
	g_ptr_array_add(extra, "path");
	
	gchar *path = evs_server_format_path("/////some///slashes/////", extra);
	test_str_eq(path, "/some/slashes/with/path", "Trailing slash removed");
	g_free(path);
	
	g_ptr_array_free(extra, TRUE);
}
END_TEST

START_TEST(test_evs_event_creation_valid_minimal) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/qio/noop::plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	
	// Test the event data
	test_str_eq(event.path, "/qio/noop", "Correct event name");
	test_uint16_eq(event.extra->len, 0, "No extra segments");
	test_uint32_eq(event.client_callback, 0, "No callback");
	test(event.data_type == d_plain, "Plain data type set");
	test_str_eq(event.data, "", "No data set");
	
	// Test the side-effects
	event_handler_t *right_handler = evs_server_get_handler(event.path, NULL);
	test_ptr_eq(handler, right_handler, "Correct handler retrieved");
	test_size_eq(message->buffer->len, 0, "Buffer cleared");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_valid_single_digit_callback) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	memset(&event, 0, sizeof(event));
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/qio/noop:1:plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	
	// Test the event data
	test_str_eq(event.path, "/qio/noop", "Correct event name");
	test_uint16_eq(event.extra->len, 0, "No extra segments");
	test_uint32_eq(event.client_callback, 1, "No callback");
	test(event.data_type == d_plain, "Plain data type set");
	test_str_eq(event.data, "", "No data set");
	
	// Test the side-effects
	event_handler_t *right_handler = evs_server_get_handler(event.path, NULL);
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
	
	g_string_assign(message->buffer, "/qio/noop:123:plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.client_callback, 123, "Callback gotten!");
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
	
	g_string_assign(message->buffer, "/qio/noop:123:json={\"test\":1}");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.client_callback, 123, "Callback gotten!");
	test(event.data_type == d_json, "JSON data type set");
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
	
	g_string_assign(message->buffer, "/qio/noop:123:plain=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_GOOD, "Valid message");
	test_uint32_eq(event.client_callback, 123, "Callback gotten!");
	test(event.data_type == d_plain, "Plain data type set");
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
	test_uint32_eq(event.client_callback, 123, "Callback gotten!");
	test(event.data_type == d_plain, "Plain data type set");
	test_str_eq(event.data, "test", "Plain data recieved");
	
	// Segment testing
	test_uint16_eq(event.extra->len, 1, "1 extra segment");
	test_str_eq(g_ptr_array_index(event.extra, 0), "something", "Correct extra segment");
	
	_event_free(&event);
	
	test_ptr_eq(event.path, NULL, "Name free'd");
	test_ptr_eq(event.extra, NULL, "Extra segments free'd");
	
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
	
	g_string_assign(message->buffer, "/qio/noop::=");
	
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
	
	g_string_assign(message->buffer, "/qio/noop:abcd1234:=");
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_BAD_MESSAGE_FORMAT, "Invalid message");
	test_uint32_eq(event.client_callback, 0, "Callback parsed");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_event_creation_invalid_callback) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	message_t *message = client->message;
	
	g_string_assign(message->buffer, "/qio/noop:=");
	
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
	
	g_string_assign(message->buffer, "/qio/noop:\x11\x01\x12\xab\x00:=hi");
	
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
	
	g_string_assign(message->buffer, "/qio/noop:123:plain");
	
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
	
	test_status_eq(_event_new(message, &handler, &event), CLIENT_ERROR, "Invalid event");
	
	_event_free(&event);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_new_handler_bad_path_0) {
	test_ptr_eq(evs_server_on("/:event", NULL, NULL, NULL, FALSE), NULL, "Bad path");
}
END_TEST

START_TEST(test_evs_new_handler_bad_path_1) {
	test_ptr_eq(evs_server_on("/=event", NULL, NULL, NULL, FALSE), NULL, "Bad path");
}
END_TEST

START_TEST(test_evs_new_handler_bad_path_2) {
	test_ptr_eq(evs_server_on("/=:event", NULL, NULL, NULL, FALSE), NULL, "Bad path");
}
END_TEST

START_TEST(test_evs_new_handler_no_fn) {
	event_handler_t *handler = evs_server_on("/another/event", NULL, NULL, NULL, FALSE);
	test(handler != NULL, "Handler created");
	
	client_t *client = u_client_create(NULL);
	g_string_assign(client->message->buffer, "/another/event:0:plain=");
	
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "No handler called");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_new_handler_already_exists) {
	event_handler_t *handler = evs_server_on("/another/event", NULL, NULL, NULL, FALSE);
	test(handler != NULL, "Handler created");
	
	handler = evs_server_on("/another/event", NULL, NULL, NULL, FALSE);
	test(handler == NULL, "Handler rejected created");
}
END_TEST

START_TEST(test_evs_get_handler_0) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("qio/noop", &extra);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_1) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("qio/noop/", &extra);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_2) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("/qio/noop/", &extra);
	test_ptr_eq(handler, evs_server_get_handler("/qio/noop", NULL), "Correct handler retrieved");
	test_uint64_eq(extra->len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_3) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("/qio/noop/extra/segs", &extra);
	test_ptr_eq(handler, NULL, "Correct handler retrieved");
	test_ptr_eq(extra, NULL, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_4) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("/qio/noop/////", &extra);
	test_ptr_eq(handler, evs_server_get_handler("/qio/noop", NULL), "Correct handler retrieved");
	test_uint64_eq(extra->len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_5) {
	path_extra_t extra;
	
	event_handler_t *handler = evs_server_get_handler("/qio/noop\x00/more/items", &extra);
	test_ptr_eq(handler, evs_server_get_handler("/qio/noop", NULL), "Correct handler retrieved");
	test_uint64_eq(extra->len, 0, "No extra segments");
}
END_TEST

START_TEST(test_evs_get_handler_6) {
	path_extra_t extra;
	
	event_handler_t *expected = evs_server_on("/multi", NULL, NULL, NULL, TRUE);
	
	event_handler_t *handler = evs_server_get_handler("/multi/handler/another/segment", &extra);
	test_ptr_eq(handler, expected, "Correct handler retrieved");
	test_uint16_eq(extra->len, 3, "3 extra segments");
	test_str_eq(g_ptr_array_index(extra, 0), "handler", "Correct extra segment");
	test_str_eq(g_ptr_array_index(extra, 1), "another", "Correct extra segment");
	test_str_eq(g_ptr_array_index(extra, 2), "segment", "Correct extra segment");
	
	g_ptr_array_unref(extra);
}
END_TEST

START_TEST(test_evs_path_from_handler) {
	event_handler_t *handler = g_hash_table_lookup(_events_by_path, "/qio/noop");
	
	test_str_eq(evs_server_path_from_handler(handler), "/qio/noop", "Found correct event");
}
END_TEST

START_TEST(test_evs_handler_noop) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/noop:0:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Noop recieved");
	test_size_eq(client->message->buffer->len, 0, "No data set");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_noop_callback) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/noop:123:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Noop recieved");
	test_str_eq(client->message->buffer->str, "/qio/callback/123:0:plain=", "Callback with no data");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_ping) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/ping:123:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Data sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/123:0:plain=test", "Ping sent back correct data");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_ping_no_data) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/ping:123:plain=");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Data sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/123:0:plain=", "Ping sent back correct data");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:456:plain=/doesnt/exist");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_to_bad_format) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:456:plain=0");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_too_many_subs) {
	client_t *client = u_client_create(NULL);
	
	guint64 max = option_max_subscriptions();
	while (max--) {
		g_ptr_array_add(client->subs, NULL);
	}
	
	g_string_assign(client->message->buffer, "/qio/sub:456:plain=/qio/noop");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_already_subscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:0:plain=/qio/noop");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Subscribed");
	test_size_eq(client->message->buffer->len, 0, "Subscribed!");
	
	g_string_assign(client->message->buffer, "/qio/sub:456:plain=/qio/noop");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_subscribe_rejected) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:456:plain=/doesnt/exist");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:0:plain=/qio/noop");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Subscribed");
	test_size_eq(client->message->buffer->len, 0, "Subscribed!");
	
	g_string_assign(client->message->buffer, "/qio/unsub:456:plain=/qio/noop");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Unsubscribed");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=", "Unsubscribed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe_not_subscribed) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/unsub:456:plain=/qio/noop");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_handler_unsubscribe_bad_event) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/unsub:456:plain=0");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Error sent back");
	test_str_eq(client->message->buffer->str, "/qio/callback/456:0:plain=qio_error", "Bad event");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_default_callbacks_0) {
	client_t *client = u_client_create(NULL);
	g_string_append(client->message->buffer, "/qio/ping:0:plain=");
	
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "No callback sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_default_callbacks_1) {
	client_t *client = u_client_create(NULL);
	g_string_append(client->message->buffer, "/qio/ping:0:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "No callback sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_default_callbacks_2) {
	client_t *client = u_client_create(NULL);
	g_string_append(client->message->buffer, "/qio/ping:123:plain=test");
	
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Callback sent");
	test_str_eq(client->message->buffer->str, "/qio/callback/123:0:plain=test", "Correct data sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_sane_0) {
	client_t *client = u_client_create(NULL);
	
	status_t _on(client_t *client, void *data, event_t *event) {
		return CLIENT_GOOD;
	}
	
	test(evs_server_callback_new(client, _on, NULL, NULL) > 0, "Callback ID sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_sane_1) {
	client_t *client = u_client_create(NULL);
	
	status_t _on(client_t *client, void *data, event_t *event) {
		return CLIENT_GOOD;
	}
	
	for (guint i = 0; i < MAX_CALLBACKS+2; i++) {
		test(evs_server_callback_new(client, _on, NULL, NULL) > 0, "Callback ID sent");
	}
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_evict) {
	client_t *client = u_client_create(NULL);
	
	callback_t cbs[MAX_CALLBACKS+2];
	
	guint16 frees = 0;
	void _free(void *data) {
		free(data);
		frees++;
	}
	
	guint16 calls = 0;
	status_t _on(client_t *client, void *data, event_t *event) {
		calls++;
		return CLIENT_GOOD;
	}
	
	// 1: Get a few callbacks evicted
	for (guint i = 0; i < G_N_ELEMENTS(cbs); i++) {
		void *t = g_malloc(sizeof(*t));
		cbs[i] = evs_server_callback_new(client, _on, t, _free);
	}
	
	// 2: Find one of them, just to make sure
	client_cb_t *cb;
	for (guint i = 0; i < MAX_CALLBACKS; i++) {
		if (client->callbacks[i].id > 1) {
			cb = &(client->callbacks[i]);
			break;
		}
	}
	
	test(cb != NULL, "Found callback");
	
	// 3: Fire all of the callbacks
	for (guint i = 0; i < G_N_ELEMENTS(cbs); i++) {
		g_string_printf(client->message->buffer, F_CALLBACK_PATH ":0:plain=", cbs[i]);
		evs_server_handle(client);
	}
	
	test_uint32_eq(calls, MAX_CALLBACKS, "Callbacks called");
	test_uint32_eq(frees, G_N_ELEMENTS(cbs), "Everything free'd");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_no_callback) {
	client_t *client = u_client_create(NULL);
	
	guint16 frees = 0;
	void _free(void *data) {
		free(data);
		frees++;
	}
	
	void *t = g_malloc(sizeof(*t));
	callback_t callback = evs_server_callback_new(client, NULL, t, _free);
	
	test_uint32_eq(callback, 0, "Callback rejected");
	test_uint32_eq(frees, 1, "Everything free'd");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_client_close) {
	client_t *client = u_client_create(NULL);
	
	callback_t cbs[MAX_CALLBACKS];
	
	guint16 frees = 0;
	void _free(void *data) {
		free(data);
		frees++;
	}
	
	guint16 calls = 0;
	status_t _on(client_t *client, void *data, event_t *event) {
		calls++;
		return CLIENT_GOOD;
	}
	
	// 1: Get a few callbacks evicted
	for (guint i = 0; i < G_N_ELEMENTS(cbs); i++) {
		void *t = g_malloc(sizeof(*t));
		cbs[i] = evs_server_callback_new(client, _on, t, _free);
		test(cbs[i] > 0, "Callback accepted");
	}
	
	evs_server_client_close(client);
	
	test_uint32_eq(frees, G_N_ELEMENTS(cbs), "Everything free'd");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_bad_id) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/callback/:1112:plain=");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Callback coming");
	
	test_str_eq(client->message->buffer->str, "/qio/callback/1112:0:plain=qio_error", "Correct response sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_subscribe_to_callback) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/qio/sub:0:plain=/qio/callback/");
	test_status_eq(evs_server_handle(client), CLIENT_ERROR, "Rejected");
	
	test_str_eq(client->message->buffer->str, "", "No response sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_subscribe_to_bad_event) {
	client_t *client = u_client_create(NULL);
	
	g_string_assign(client->message->buffer, "/abcd/:1:plain=/asdf/");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Rejected");
	
	test_str_eq(client->message->buffer->str, "/qio/callback/1:0:plain=qio_error", "Error callback sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_fatal) {
	client_t *client = u_client_create(NULL);
	
	status_t _on(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
		return CLIENT_ABORTED;
	}
	
	evs_server_on("/test/fatal", _on, NULL, NULL, FALSE);
	
	g_string_assign(client->message->buffer, "/test/fatal:1:plain=/asdf/");
	test_status_eq(evs_server_handle(client), CLIENT_ABORTED, "Aborted");
}
END_TEST

START_TEST(test_evs_server_server_callback_id_overflow) {
	client_t *client = u_client_create(NULL);
	
	void _free(void *data) {}
	
	status_t _on(client_t *client, void *data, event_t *event) {
		return CLIENT_GOOD;
	}
	
	void *t = g_malloc(sizeof(*t));
	callback_t callback = evs_server_callback_new(client, _on, t, _free);
	
	// Make the callback slot absurd
	callback |= 0xFF00;
	
	g_string_printf(client->message->buffer, F_CALLBACK_PATH ":0:plain=", callback);
	test_status_eq(evs_server_handle(client), CLIENT_ERROR, "Lame callback rejected");
	
	g_string_printf(client->message->buffer, F_CALLBACK_PATH ":1:plain=", callback);
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Lame callback rejected");
	test_str_eq(client->message->buffer->str, "/qio/callback/1:0:plain=qio_error", "Correct callback sent");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_chain) {
	client_t *client = u_client_create(NULL);
	
	event_t event;
	event_handler_t *handler = NULL;
	
	guint frees = 0;
	void _free(void *data) {
		frees++;
		g_free(data);
	}
	
	guint calls = 0;
	status_t _on(client_t *client, void *data, event_t *event) {
		calls++;
		
		void *t = g_malloc(sizeof(*t));
		callback_t callback = evs_server_callback_new(client, _on, t, _free);
		
		test(callback > 0, "Created server callback");
		
		event->server_callback = callback;
		
		return CLIENT_GOOD;
	}
	
	status_t _handler(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
		void *t = g_malloc(sizeof(*t));
		callback_t callback = evs_server_callback_new(client, _on, t, _free);
		
		test(callback > 0, "Created server callback");
		
		event->server_callback = callback;
		
		return CLIENT_GOOD;
	}
	
	evs_server_on("/test/server/cbs", _handler, NULL, NULL, FALSE);
	
	// ----------------------
	// First callbacks
	// ----------------------
	
	// Send event to the server, expect a server callback
	g_string_assign(client->message->buffer, "/test/server/cbs:900:plain=");
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Callback sent");
	
	// Check the message on the client
	test(g_strstr_len(client->message->buffer->str, client->message->buffer->len, "/qio/callback/900") == client->message->buffer->str);
	_event_new(client->message, &handler, &event);
	
	// Make sure we have a server callback
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Second callbacks
	// ----------------------
	
	// Call the server callback
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":901:plain=", event.client_callback);
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Callback sent to server");
	
	// Expect a server callback on the client
	test(g_strstr_len(client->message->buffer->str, client->message->buffer->len, "/qio/callback/901") == client->message->buffer->str);
	_event_new(client->message, &handler, &event);
	
	// Make sure we have a server callback
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Third callbacks
	// ----------------------
	
	// Call the server callback
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":902:plain=", event.client_callback);
	test_status_eq(evs_server_handle(client), CLIENT_WRITE, "Callback sent to server");
	
	// Expect a server callback on the client
	test(g_strstr_len(client->message->buffer->str, client->message->buffer->len, "/qio/callback/902") == client->message->buffer->str);
	_event_new(client->message, &handler, &event);
	
	// Make sure we have a server callback
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Fourth callbacks
	// ----------------------
	
	// Call the server callback
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":0:plain=", event.client_callback);
	
	// In the last call, we didn't send a client callback, so there should be a server
	// callback created, but we're not doing anything with it, so it should be discarded
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "No callback returned");
	
	test_uint32_eq(calls, 3, "Server callbacks");
	test_uint32_eq(frees, 4, "Free'd callbacks");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_chain_async) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	event_t event;
	event_handler_t *handler = NULL;
	callback_t server_callback;
	callback_t client_callback;
	
	guint frees = 0;
	void _free(void *data) {
		frees++;
		g_free(data);
	}
	
	guint calls = 0;
	status_t _on(client_t *client, void *data, event_t *event) {
		calls++;
		
		void *t = g_malloc(sizeof(*t));
		callback_t callback = evs_server_callback_new(client, _on, t, _free);
		
		test(callback > 0, "Created server callback");
		
		server_callback = callback;
		client_callback = event->client_callback;
		
		return CLIENT_ASYNC;
	}
	
	status_t _handler(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
		void *t = g_malloc(sizeof(*t));
		server_callback = evs_server_callback_new(client, _on, t, _free);
		
		test(server_callback > 0, "Created server callback");
		
		client_callback = event->client_callback;
		
		return CLIENT_ASYNC;
	}
	
	evs_server_on("/test/server/cbs", _handler, NULL, NULL, FALSE);
	
	// ----------------------
	// First callbacks
	// ----------------------
	
	g_string_assign(client->message->buffer, "/test/server/cbs:900:plain=");
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Callback sent");
	evs_client_send_callback(client, client_callback, d_plain, "", server_callback);
	evs_client_send_async_messages();
	
	gchar buff[512];
	memset(&buff, 0, sizeof(buff));
	read(socket, buff, sizeof(buff));
	test_str_eq(buff, "\x81""\x1a""/qio/callback/900:1:plain=", "Callback sent");
	
	// Get the server callback to call
	g_string_assign(client->message->buffer, buff+2);
	_event_new(client->message, &handler, &event);
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Second callbacks
	// ----------------------
	
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":901:plain=", event.client_callback);
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Callback sent");
	evs_client_send_callback(client, client_callback, d_plain, "", server_callback);
	evs_client_send_async_messages();
	
	memset(&buff, 0, sizeof(buff));
	read(socket, buff, sizeof(buff));
	test_str_eq(buff, "\x81""\x1a""/qio/callback/901:2:plain=", "Callback sent");
	
	// Get the server callback to call
	g_string_assign(client->message->buffer, buff+2);
	_event_new(client->message, &handler, &event);
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Third callbacks
	// ----------------------
	
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":902:plain=", event.client_callback);
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Callback sent");
	evs_client_send_callback(client, client_callback, d_plain, "", server_callback);
	evs_client_send_async_messages();
	
	memset(&buff, 0, sizeof(buff));
	read(socket, buff, sizeof(buff));
	test_str_eq(buff, "\x81""\x1a""/qio/callback/902:3:plain=", "Callback sent");
	
	// Get the server callback to call
	g_string_assign(client->message->buffer, buff+2);
	_event_new(client->message, &handler, &event);
	test(event.client_callback > 0, "Server callback sent");
	_event_free(&event);
	
	// ----------------------
	// Fourth callbacks
	// ----------------------
	
	g_string_printf(client->message->buffer, F_CALLBACK_PATH":0:plain=", event.client_callback);
	test_status_eq(evs_server_handle(client), CLIENT_GOOD, "Callback sent");
	evs_client_send_callback(client, client_callback, d_plain, "", server_callback);
	evs_client_send_async_messages();
	
	test_uint32_eq(calls, 3, "Server callbacks");
	test_uint32_eq(frees, 4, "Free'd callbacks");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_server_server_callback_bad_status) {
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	
	status_t _on(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
		event->server_callback = 123;
		return 99;
	}
	
	evs_server_on("/bad_status", _on, NULL, NULL, TRUE);
	
	// NO client callback, trigger server_callback handler
	g_string_assign(client->message->buffer, "/bad_status:0:plain=/asdf/");
	test_status_eq(evs_server_handle(client), CLIENT_ABORTED, "Aborted");
	
	u_client_free(client);
}
END_TEST

Suite* events_server_suite() {
	TCase *tc;
	Suite *s = suite_create("Events - Server");
	
	tc = tcase_create("Name Cleaning");
	tcase_add_test(tc, test_evs_server_format_path_0);
	tcase_add_test(tc, test_evs_server_format_path_1);
	tcase_add_test(tc, test_evs_server_format_path_2);
	tcase_add_test(tc, test_evs_server_format_path_3);
	tcase_add_test(tc, test_evs_server_format_path_4);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Event Creation");
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
	tcase_add_test(tc, test_evs_event_creation_valid_minimal);
	tcase_add_test(tc, test_evs_event_creation_valid_single_digit_callback);
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
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
	tcase_add_test(tc, test_evs_new_handler_bad_path_0);
	tcase_add_test(tc, test_evs_new_handler_bad_path_1);
	tcase_add_test(tc, test_evs_new_handler_bad_path_2);
	tcase_add_test(tc, test_evs_new_handler_no_fn);
	tcase_add_test(tc, test_evs_new_handler_already_exists);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Handler Getting");
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
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
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
	tcase_add_test(tc, test_evs_handler_noop);
	tcase_add_test(tc, test_evs_handler_noop_callback);
	tcase_add_test(tc, test_evs_handler_ping);
	tcase_add_test(tc, test_evs_handler_ping_no_data);
	tcase_add_test(tc, test_evs_handler_subscribe);
	tcase_add_test(tc, test_evs_handler_subscribe_to_bad_format);
	tcase_add_test(tc, test_evs_handler_subscribe_too_many_subs);
	tcase_add_test(tc, test_evs_handler_subscribe_already_subscribed);
	tcase_add_test(tc, test_evs_handler_subscribe_rejected);
	tcase_add_test(tc, test_evs_handler_unsubscribe);
	tcase_add_test(tc, test_evs_handler_unsubscribe_not_subscribed);
	tcase_add_test(tc, test_evs_handler_unsubscribe_bad_event);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Default callbacks");
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
	tcase_add_test(tc, test_evs_server_default_callbacks_0);
	tcase_add_test(tc, test_evs_server_default_callbacks_1);
	tcase_add_test(tc, test_evs_server_default_callbacks_2);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Server callbacks");
	tcase_add_checked_fixture(tc, _test_events_setup, _test_events_teardown);
	tcase_add_test(tc, test_evs_server_server_callback_sane_0);
	tcase_add_test(tc, test_evs_server_server_callback_sane_1);
	tcase_add_test(tc, test_evs_server_server_callback_evict);
	tcase_add_test(tc, test_evs_server_server_callback_no_callback);
	tcase_add_test(tc, test_evs_server_server_callback_client_close);
	tcase_add_test(tc, test_evs_server_server_callback_bad_id);
	tcase_add_test(tc, test_evs_server_server_callback_subscribe_to_callback);
	tcase_add_test(tc, test_evs_server_server_callback_subscribe_to_bad_event);
	tcase_add_test(tc, test_evs_server_server_callback_fatal);
	tcase_add_test(tc, test_evs_server_server_callback_id_overflow);
	tcase_add_test(tc, test_evs_server_server_callback_chain);
	tcase_add_test(tc, test_evs_server_server_callback_chain_async);
	tcase_add_test(tc, test_evs_server_server_callback_bad_status);
	suite_add_tcase(s, tc);
	
	return s;
}