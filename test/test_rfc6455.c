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
	"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"

#define MESSAGE "test"
#define MASK "abcd"
#define RFC6455_MESSAGE_SHORT "\x81""\x84"MASK"\x15\x07\x10\x10"

#define RFC6455_MESSAGE_SHORT_P1 "\x81""\x84"MASK
#define RFC6455_MESSAGE_SHORT_P2 "\x15\x07\x10\x10"

#define RFC6455_MESSAGE_MULTI_P1 "\x81""\x84"MASK
#define RFC6455_MESSAGE_MULTI_P2 "\x15\x07"
#define RFC6455_MESSAGE_MULTI_P3 "\x10\x10"

#define RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P1 "\x81""\x84""ab"
#define RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P2 "cd""\x15\x07\x10\x10"

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
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT);
	
	fail_unless(rfc6455_incoming(client) == CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	fail_unless(client->message->socket_buffer->len == 0, "Socket buffer cleared after read");
	fail_unless(client->message->remaining_length == 0, "Message length==0 as buffer cleared");
	fail_unless(client->message->type == op_text, "Opcode set to text");
	fail_unless(client->message->mask == *((guint32*)MASK), "Correct mask in message");
	fail_unless(strcmp(client->message->buffer->str, MESSAGE) == 0, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_partial_message) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P1);
	
	fail_unless(rfc6455_incoming(client) == CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	fail_unless(client->message->socket_buffer->len == 0, "Socket buffer cleared after read");
	fail_unless(client->message->remaining_length == 4, "Waiting for 4 more characters");
	fail_unless(client->message->type == op_text, "Opcode set to text");
	fail_unless(client->message->mask == *((guint32*)MASK), "Correct mask in message");
	fail_unless(client->message->buffer->len == 0, "Nothing in the message buffer");
	
	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P2);
	fail_unless(rfc6455_continue(client) == CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	fail_unless(client->message->buffer->len == 4, "Message read completely");
	fail_unless(strcmp(client->message->buffer->str, MESSAGE) == 0, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_multi_partial_messages) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P1);
	
	fail_unless(rfc6455_incoming(client) == CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	
	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P2);
	fail_unless(rfc6455_continue(client) == CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	fail_unless(client->message->remaining_length == 2, "Waiting for 2 more characters");
	fail_unless(client->message->buffer->len == 2, "Partially decoded");
	
	// Send the third part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P3);
	fail_unless(rfc6455_continue(client) == CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	fail_unless(client->message->remaining_length == 0, "Waiting for 0 more characters");
	fail_unless(client->message->buffer->len == 4, "Completely decoded");
	
	fail_unless(strcmp(client->message->buffer->str, MESSAGE) == 0, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

/**
 * If the message header is broken, it should be treated as though
 */
START_TEST(test_rfc6455_multi_partial_broken_header) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P1);
	
	fail_unless(rfc6455_incoming(client) == CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	fail_unless(client->message->mask == 0, "Mask was not read, so not set");
	fail_unless(client->message->buffer->len == 0, "Nothing was added to the buffer");
	fail_unless(client->message->remaining_length == 0, "No message read");
	
	// Send the rest of the message
	g_string_append(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P2);
	fail_unless(rfc6455_incoming(client) == CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	fail_unless(strcmp(client->message->buffer->str, MESSAGE) == 0, "Message decoded correctly");
	
	u_client_free(client);
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
	
	tc = tcase_create("Partial Message");
	tcase_add_test(tc, test_rfc6455_partial_message);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Multi Partial Messages");
	tcase_add_test(tc, test_rfc6455_multi_partial_messages);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Partial message, broken header");
	tcase_add_test(tc, test_rfc6455_multi_partial_broken_header);
	suite_add_tcase(s, tc);
	
	return s;
}