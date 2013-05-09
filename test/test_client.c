#include "test.h"

#define INCOMPLETE_HANDSHAKE_HTTP "GET /qio HTTP/1.1\n"
#define INCOMPLETE_HANDSHAKE_WS "GET /qio HTTP/1.1\r\n"

#define NO_HANDLER_HANDSHAKE_KEYS "GET /qio HTTP/1.1\n" \
	"Herp: Derp\r\n" \
	"Merp: Terp\r\n\r\n"
#define NO_HANDLER_HANDSHAKE_HORRIBLE "GET /qio HTTP/1.1\n" \
	"Herp\r\n" \
	"Merp\r\n\r\n"

#define BAD_PATH "GET /nope HTTP/1.1\n" \
	"Test: something\n\n"

#define BAD_HANDSHAKE "asdfsadfasdfasdfasdfasdf\n\n"

#define MASK "abcd"
#define MESSAGE_RFC6455_NOOP "\x81\x91""abcd""N""\x13""\n""\x0b""N""\x0c""\x0c""\x0b""\x11""X""Y""\x14""\r""\x03""\n""\n""\\"
#define MESSAGE_RFC6455_NOOP_0 "\x81\x91""abcd"
#define MESSAGE_RFC6455_NOOP_1 "N""\x13""\n""\x0b""N""\x0c""\x0c""\x0b""\x11""X""Y""\x14""\r""\x03""\n""\n""\\"

static void _test_message_setup() {
	evs_server_init();
	apps_run();
	check(stats_init());
}

static void _test_refcount_setup() {
	evs_server_init();
	conns_init();
	apps_run();
	check(stats_init());
}

START_TEST(test_client_incomplete_handshake_0) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, INCOMPLETE_HANDSHAKE_HTTP);
	check_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete handshake");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_incomplete_handshake_1) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, INCOMPLETE_HANDSHAKE_WS);
	check_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete handshake");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_empty_handshake) {
	client_t *client = u_client_create(NULL);

	// Make sure that soup_headers_parse_request is fine parsing empty strings
	g_string_assign(client->message->socket_buffer, "");
	check_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete headers");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_handlers_0) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, NO_HANDLER_HANDSHAKE_KEYS);
	check_status_eq(client_handshake(client), CLIENT_FATAL, "No handler found");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_handlers_1) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, NO_HANDLER_HANDSHAKE_HORRIBLE);
	check_status_eq(client_handshake(client), CLIENT_FATAL, "Rejected headers");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_bad_headers) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	check_status_eq(client_handshake(client), CLIENT_FATAL, "Headers rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_bad_path) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, BAD_PATH);
	check_status_eq(client_handshake(client), CLIENT_FATAL, "Path rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_no_handler_incoming) {
	client_t *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	check_status_eq(client_message(client), CLIENT_FATAL, "Client rejected: no handler");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_no_handler_continue) {
	client_t *client = u_client_create(NULL);
	client->message->remaining_length = 100;

	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	check_status_eq(client_message(client), CLIENT_FATAL, "Client rejected: no handler");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_incoming) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	g_string_overwrite_len(client->message->socket_buffer, 0, MESSAGE_RFC6455_NOOP, sizeof(MESSAGE_RFC6455_NOOP)-1);
	check_status_eq(client_message(client), CLIENT_GOOD, "Read message");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_continue) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	g_string_overwrite_len(client->message->socket_buffer, 0, MESSAGE_RFC6455_NOOP_0, sizeof(MESSAGE_RFC6455_NOOP_0)-1);
	check_status_eq(client_message(client), CLIENT_WAIT, "Read message");
	check_size_eq(client->message->socket_buffer->len, 0, "Socket buffer emptied");

	g_string_overwrite_len(client->message->socket_buffer, 0, MESSAGE_RFC6455_NOOP_1, sizeof(MESSAGE_RFC6455_NOOP_1)-1);
	check_status_eq(client_message(client), CLIENT_GOOD, "Read message");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_message) {
	client_t *client = u_client_create(NULL);
	message_t *message = client->message;
	client->message = NULL;

	check_status_eq(client_write(client, NULL), CLIENT_FATAL, "No message to write");

	client->message = message;
	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_handler) {
	client_t *client = u_client_create(NULL);

	check_status_eq(client_write(client, NULL), CLIENT_FATAL, "No handler");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_oversized_message) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	gchar *really_long = g_strnfill(0xFFFF + 1, 'a');
	g_string_assign(client->message->buffer, really_long);
	g_free(really_long);

	check_status_eq(client_write(client, NULL), CLIENT_FATAL, "Message too long");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_frame_len) {
	client_t *client = u_client_create(NULL);

	client_write_frame(client, NULL, 0);

	u_client_free(client);
}
END_TEST

START_TEST(test_client_refcount_0) {
	client_t *client = u_client_create(NULL);

	client_ref(client);
	check_size_eq(client->ref_count, 2, "Correct ref count");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_refcount_1) {
	client_t *client = u_client_create(NULL);

	client_ref(client);
	client_ref(client);
	client_ref(client);
	client_ref(client);

	check_size_eq(client->ref_count, 5, "Correct ref count");

	client_unref(client);
	client_unref(client);

	check_size_eq(client->ref_count, 3, "Correct ref count");

}
END_TEST

START_TEST(test_client_refcount_2) {
	client_t *client = u_client_create(NULL);

	client_ref(client);
	check_size_eq(client->ref_count, 2, "Correct ref count");

	u_client_free(client);
}
END_TEST

START_TEST(test_client_refcount_3) {
	client_t *client = u_client_create(NULL);

	client_ref(client);
	client_ref(client);
	check_size_eq(client->ref_count, 3, "Correct ref count");
	conns_client_close(client);
	check_size_eq(client->ref_count, 2, "Correct ref count");

	// Incrementing ref count after client is closed
	client_ref(client);
	check_size_eq(client->ref_count, 3, "Correct ref count");
}
END_TEST

START_TEST(test_client_killed_closed) {
	client_t *client = u_client_create(NULL);
	client->state = cstate_running;

	conns_client_close(client);
	check_size_eq(client->state, cstate_running, "Client closed");

	conns_client_killed(client);
	check_size_eq(client->state, cstate_dead, "Client closed");
}
END_TEST

START_TEST(test_client_external_data_sane) {
	client_t *client = u_client_create(NULL);

	check_ptr_eq(client_get(client, "test"), NULL, "No data set");
	check_not(client_has(client, "test"), "Not set");

	client_set(client, "test", "test");

	const gchar *val = client_get(client, "test");
	check_str_eq(val, "test", "Correct value");
	check(client_has(client, "test"), "Not set");

	client_ref(client);
	client_unref(client);

	val = client_get(client, "test");
	check_str_eq(val, "test", "Correct value");
	check(client_has(client, "test"), "Not set");
}
END_TEST

START_TEST(test_client_external_data_overwrite) {
	client_t *client = u_client_create(NULL);

	client_set(client, "test", "test");
	client_set(client, "test", "test2");
	client_set(client, "test", "test3");

	const gchar *val = client_get(client, "test");
	check_str_eq(val, "test3", "Correct value");
}
END_TEST

START_TEST(test_client_external_data_delete) {
	client_t *client = u_client_create(NULL);

	client_set(client, "test", "test");
	client_set(client, "test", NULL);

	check_not(client_has(client, "test"), "Key removed");
}
END_TEST

START_TEST(test_client_external_data_client_unref_0) {
	client_t *client = u_client_create(NULL);
	client_unref(client);
}
END_TEST

START_TEST(test_client_external_data_client_unref_1) {
	client_t *client = u_client_create(NULL);

	client_set(client, "test", "test");
	check(client_has(client, "test"), "Key set");

	client_unref(client);
}
END_TEST

Suite* client_suite() {
	TCase *tc;
	Suite *s = suite_create("Client");

	tc = tcase_create("Handshake");
	tcase_add_test(tc, test_client_incomplete_handshake_0);
	tcase_add_test(tc, test_client_incomplete_handshake_1);
	tcase_add_test(tc, test_client_empty_handshake);
	tcase_add_test(tc, test_client_no_handlers_0);
	tcase_add_test(tc, test_client_no_handlers_1);
	tcase_add_test(tc, test_client_bad_headers);
	tcase_add_test(tc, test_client_bad_path);
	suite_add_tcase(s, tc);

	tc = tcase_create("Message");
	tcase_add_checked_fixture(tc, _test_message_setup, NULL);
	tcase_add_test(tc, test_client_message_no_handler_incoming);
	tcase_add_test(tc, test_client_message_no_handler_continue);
	tcase_add_test(tc, test_client_message_incoming);
	tcase_add_test(tc, test_client_message_continue);
	tcase_add_test(tc, test_client_oversized_message);
	suite_add_tcase(s, tc);

	tc = tcase_create("Writing");
	tcase_add_test(tc, test_client_no_message);
	tcase_add_test(tc, test_client_no_handler);
	tcase_add_test(tc, test_client_no_frame_len);
	suite_add_tcase(s, tc);

	tc = tcase_create("Ref Counting");
	tcase_add_checked_fixture(tc, _test_refcount_setup, NULL);
	tcase_add_test(tc, test_client_refcount_0);
	tcase_add_test(tc, test_client_refcount_1);
	tcase_add_test(tc, test_client_refcount_2);
	tcase_add_test(tc, test_client_refcount_3);
	tcase_add_test(tc, test_client_killed_closed);
	suite_add_tcase(s, tc);

	tc = tcase_create("External Data");
	tcase_add_checked_fixture(tc, _test_refcount_setup, NULL);
	tcase_add_test(tc, test_client_external_data_sane);
	tcase_add_test(tc, test_client_external_data_overwrite);
	tcase_add_test(tc, test_client_external_data_delete);
	tcase_add_test(tc, test_client_external_data_client_unref_0);
	tcase_add_test(tc, test_client_external_data_client_unref_1);
	suite_add_tcase(s, tc);

	return s;
}
