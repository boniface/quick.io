#include "test.h"

#define RFC6455_HANDSHAKE "GET /chat HTTP/1.1\n" \
	"Host: server.example.com\n" \
	"Upgrade: websocket\n" \
	"Connection: Upgrade\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\n" \
	"Origin: http://example.com\n" \
	"Sec-WebSocket-Version: 13\n\n"

#define RFC6455_HANDSHAKE_REPONSE "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n" \

START_TEST(test_rfc6455_handshake) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, RFC6455_HANDSHAKE);
	
	fail_unless(client_handshake(client) == CLIENT_WRITE, "Client handshake");
	
	fail_unless(strcmp(client->message->buffer->str, RFC6455_HANDSHAKE_REPONSE) == 0, "Handshake response from server incorrect");
	
	// Make sure the side-effects are correct
	fail_unless(client->initing == 0, "Client init flag updated");
	fail_unless(client->message->socket_buffer->len == 0, "Socket buffer cleared");
	fail_unless(client->handler == h_rfc6455, "Correct handler applied");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_message) {
	
}
END_TEST

Suite* rfc6455_suite() {
	TCase *tc;
	Suite *s = suite_create("RFC6455");
	
	tc = tcase_create("Handshake");
	tcase_add_test(tc, test_rfc6455_handshake);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Message");
	tcase_add_test(tc, test_rfc6455_message);
	suite_add_tcase(s, tc);
	
	return s;
}