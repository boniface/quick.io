#include "test.h"

#define PARTIAL_REQUEST_0 "<policy-fil"
#define PARTIAL_REQUEST_1 "e-request/>"

START_TEST(test_h_flash_policy_handshake) {
	client_t *client = u_client_create(NULL);
	
	g_string_append(client->message->socket_buffer, H_FLASH_POLICY_REQUEST);
	
	status_t status = client_handshake(client);
	
	test_str_eq(client->message->buffer->str, H_FLASH_POLICY_RESPONSE, "Response sent");
	test_status_eq(status, CLIENT_FATAL, "Aborting");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_h_flash_policy_partial_handshake) {
	client_t *client = u_client_create(NULL);
	
	g_string_append(client->message->socket_buffer, PARTIAL_REQUEST_0);
	status_t status = client_handshake(client);
	
	test_size_eq(client->message->buffer->len, 0, "No response sent");
	test_status_eq(status, CLIENT_WAIT, "Waiting for rest of header");
	
	g_string_append(client->message->socket_buffer, PARTIAL_REQUEST_1);
	status = client_handshake(client);
	
	test_str_eq(client->message->buffer->str, H_FLASH_POLICY_RESPONSE, "Response sent");
	test_status_eq(status, CLIENT_FATAL, "Aborting");
	
	u_client_free(client);
}
END_TEST

Suite* h_flash_policy_suite() {
	TCase *tc;
	Suite *s = suite_create("Handler: Flash Policy");
	
	tc = tcase_create("Check");
	tcase_add_test(tc, test_h_flash_policy_handshake);
	tcase_add_test(tc, test_h_flash_policy_partial_handshake);
	suite_add_tcase(s, tc);
	
	return s;
}