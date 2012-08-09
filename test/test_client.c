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

Suite* client_suite() {
	TCase *tc;
	Suite *s = suite_create("Client");
	
	tc = tcase_create("Incomplete Handshake");
	tcase_add_test(tc, test_client_incomplete_handshake_0);
	tcase_add_test(tc, test_client_incomplete_handshake_1);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Empty Handshake");
	tcase_add_test(tc, test_client_empty_handshake);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("No Handlers");
	tcase_add_test(tc, test_client_no_handlers_0);
	tcase_add_test(tc, test_client_no_handlers_1);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("No Handlers");
	tcase_add_test(tc, test_client_bad_headers);
	suite_add_tcase(s, tc);
	
	return s;
}