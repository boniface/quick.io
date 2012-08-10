#include "test.h"

#define INCOMPLETE_HANDSHAKE_HTTP "GET /chat HTTP/1.1\n"
#define INCOMPLETE_HANDSHAKE_WS "GET /chat HTTP/1.1\r\n"

#define NO_HANDLER_HANDSHAKE_KEYS "GET /chat HTTP/1.1\n" \
	"Herp: Derp\r\n" \
	"Merp: Terp\r\n\r\n"
#define NO_HANDLER_HANDSHAKE_HORRIBLE "GET /chat HTTP/1.1\n" \
	"Herp\r\n" \
	"Merp\r\n\r\n"

#define BAD_HANDSHAKE "asdfsadfasdfasdfasdfasdf\n\n"

#define MASK "abcd"
#define MESSAGE "test"
#define MESSAGE_FRAMED "\x81""\x84"MASK"\x15\x07\x10\x10"

START_TEST(test_client_incomplete_handshake_0) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, INCOMPLETE_HANDSHAKE_HTTP);
	test_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete handshake");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_incomplete_handshake_1) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, INCOMPLETE_HANDSHAKE_WS);
	test_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete handshake");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_empty_handshake) {
	client_t *client = u_client_create();
	
	// Make sure that soup_headers_parse_request is fine parsing empty strings
	g_string_assign(client->message->socket_buffer, "");
	test_status_eq(client_handshake(client), CLIENT_WAIT, "Waiting for complete headers");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_handlers_0) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, NO_HANDLER_HANDSHAKE_KEYS);
	test_status_eq(client_handshake(client), CLIENT_UNSUPPORTED, "No handler found");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_no_handlers_1) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, NO_HANDLER_HANDSHAKE_HORRIBLE);
	test_status_eq(client_handshake(client), CLIENT_UNSUPPORTED, "Rejected headers");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_bad_headers) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	test_status_eq(client_handshake(client), CLIENT_ABORTED, "Headers rejected");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_no_handler_incoming) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	test_status_eq(client_message(client), CLIENT_ABORTED, "Client rejected: no handler");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_no_handler_continue) {
	client_t *client = u_client_create();
	client->message->remaining_length = 100;
	
	g_string_assign(client->message->socket_buffer, BAD_HANDSHAKE);
	test_status_eq(client_message(client), CLIENT_ABORTED, "Client rejected: no handler");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_client_message_incoming) {
	client_t *client = u_client_create();
	client->handler = h_rfc6455;
	
	// g_string_assign(client->message->socket_buffer, MESSAGE_FRAMED);
	// test_status_eq(client_message(client), CLIENT_GOOD, "Read message");
	
	u_client_free(client);
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
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Message");
	tcase_add_test(tc, test_client_message_no_handler_incoming);
	tcase_add_test(tc, test_client_message_no_handler_continue);
	tcase_add_test(tc, test_client_message_incoming);
	suite_add_tcase(s, tc);
	
	return s;
}