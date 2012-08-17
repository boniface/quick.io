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

#define MASK "abcd"

#define MESSAGE_SHORT "test"
#define RFC6455_MESSAGE_SHORT "\x81""\x84"MASK"\x15\x07\x10\x10"

#define RFC6455_MESSAGE_PARTIAL_NO_HEADER_P1 "\x81"
#define RFC6455_MESSAGE_PARTIAL_NO_HEADER_P2 "\x84"MASK"\x15\x07\x10\x10"

#define RFC6455_MESSAGE_SHORT_P1 "\x81""\x84"MASK
#define RFC6455_MESSAGE_SHORT_P2 "\x15\x07\x10\x10"

#define RFC6455_MESSAGE_MULTI_P1 "\x81""\x84"MASK
#define RFC6455_MESSAGE_MULTI_P2 "\x15\x07"
#define RFC6455_MESSAGE_MULTI_P3 "\x10\x10"

#define RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P1 "\x81""\x84""ab"
#define RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P2 "cd""\x15\x07\x10\x10"

#define MESSAGE_LONG "some message that happens to be longer than 125 characters, which makes it the perfect size to cause a larger header to be required to send the message length"
#define RFC6455_MESSAGE_LONG "\x81""\xfe""\x00""\x9e""abcd\x12\r\x0e""\x01""A\x0f\x06\x17\x12\x03\x04""\x01""A\x16\x0b\x05""\x15""B\x0b\x05\x11\x12\x06\n""\x12""B\x17""\x0b""A\x00""\x06""D\r\r\r\x03\x04""\x10""C\x10\t\x03\rDPPVD\x02\n\x02\x16\x00\x01\x17\x01\x13""\x11""OD\x16\n\n\x07\tB\x0e\x05\n\x07""\x10""D\x08""\x16""C\x10\t""\x07""C\x14\x04\x10\x05\x01\x02""\x16""C\x17\x08\x18""\x06""D\x15\rC\x07\x00\x17\x10""\x01""A""\x03""C\x08\x00\x10\x04\x01""\x13""B\x0b\x01\x00\x06\x06""\x16""A\x16""\x0c""D\x03""\x07""C\x16\x04\x13\x16\r\x13\x07""\x07""D\x15\rC\x17\x04\x0c""\x07""D\x15\n""\x06""D\x0c\x07\x10\x17\x00\x05""\x06""D\r\x07\r\x03\x15\n"

#define RFC6455_MESSAGE_LONG_BROKEN_HEADER_1 "\x81""\xfe""\x00"
#define RFC6455_MESSAGE_LONG_BROKEN_HEADER_2 "\x9e""abcd\x12\r\x0e""\x01""A\x0f\x06\x17\x12\x03\x04""\x01""A\x16\x0b\x05""\x15""B\x0b\x05\x11\x12\x06\n""\x12""B\x17""\x0b""A\x00""\x06""D\r\r\r\x03\x04""\x10""C\x10\t\x03\rDPPVD\x02\n\x02\x16\x00\x01\x17\x01\x13""\x11""OD\x16\n\n\x07\tB\x0e\x05\n\x07""\x10""D\x08""\x16""C\x10\t""\x07""C\x14\x04\x10\x05\x01\x02""\x16""C\x17\x08\x18""\x06""D\x15\rC\x07\x00\x17\x10""\x01""A""\x03""C\x08\x00\x10\x04\x01""\x13""B\x0b\x01\x00\x06\x06""\x16""A\x16""\x0c""D\x03""\x07""C\x16\x04\x13\x16\r\x13\x07""\x07""D\x15\rC\x17\x04\x0c""\x07""D\x15\n""\x06""D\x0c\x07\x10\x17\x00\x05""\x06""D\r\x07\r\x03\x15\n"

#define RFC6455_MESSAGE_OVERSIZED_MESSAGE "\x81""\xff""\x00""\x9e"

#define RFC6455_FRAMED_SHORT "\x81""\x04"MESSAGE_SHORT
#define RFC6455_FRAMED_LONG "\x81""\x7e""\x00""\x9e"MESSAGE_LONG
#define RFC6455_OVERSIZED_LEN 0x100000

#define RFC6455_FRAMED_PONG "\x8A""\x04"MESSAGE_SHORT

START_TEST(test_rfc6455_handshake) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, RFC6455_HANDSHAKE);
	
	test_status_eq(client_handshake(client), CLIENT_WRITE, "Response should be written to client");
	test_str_eq(client->message->buffer->str, RFC6455_HANDSHAKE_REPONSE, "Correct handshake sent back");
	
	// Make sure the side-effects are correct
	test_char_eq(client->initing, '\0', "Client init flag updated");
	test_size_eq(client->message->socket_buffer->len, 0, "Socket buffer cleared");
	test_int32_eq(client->handler, h_rfc6455, "Correct handler applied");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_short_message) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	test_size_eq(client->message->socket_buffer->len, 0, "Socket buffer cleared after read");
	test_uint16_eq(client->message->remaining_length, 0, "Message length==0 as buffer cleared");
	test_char_eq(client->message->type, op_text, "Opcode set to text");
	test_uint32_eq(client->message->mask, *((guint32*)MASK), "Correct mask in message");
	test_str_eq(client->message->buffer->str, MESSAGE_SHORT, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_partial_no_header) {
	client_t *client = u_client_create();
	
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_PARTIAL_NO_HEADER_P1);
	test_status_eq(rfc6455_incoming(client), CLIENT_WAIT, "Short header rejected");
	
	g_string_append(client->message->socket_buffer, RFC6455_MESSAGE_PARTIAL_NO_HEADER_P2);
	test_status_eq(rfc6455_incoming(client), CLIENT_GOOD, "Message completed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_partial_no_header_long) {
	client_t *client = u_client_create();
	
	g_string_overwrite_len(client->message->socket_buffer, 0, RFC6455_MESSAGE_LONG_BROKEN_HEADER_1, sizeof(RFC6455_MESSAGE_LONG_BROKEN_HEADER_1)-1);
	test_status_eq(rfc6455_incoming(client), CLIENT_WAIT, "Large header rejected");
	
	g_string_append_len(client->message->socket_buffer, RFC6455_MESSAGE_LONG_BROKEN_HEADER_2, sizeof(RFC6455_MESSAGE_LONG_BROKEN_HEADER_2)-1);
	test_status_eq(rfc6455_incoming(client), CLIENT_GOOD, "Message completed");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_partial_short_message) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P1);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	test_size_eq(client->message->socket_buffer->len, 0, "Socket buffer cleared after read");
	test_uint16_eq(client->message->remaining_length, 4, "Waiting for 4 more characters");
	test_char_eq(client->message->type, op_text, "Opcode set to text");
	test_uint32_eq(client->message->mask, *((guint32*)MASK), "Correct mask in message");
	test_size_eq(client->message->buffer->len, 0, "Nothing in the message buffer");
	
	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P2);
	test_status_eq(rfc6455_continue(client), CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	test_size_eq(client->message->buffer->len, 4, "Message read completely");
	test_str_eq(client->message->buffer->str, MESSAGE_SHORT, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_multi_partial_short_messages) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P1);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	
	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P2);
	test_status_eq(rfc6455_continue(client), CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	test_uint16_eq(client->message->remaining_length, 2, "Waiting for 2 more characters");
	test_size_eq(client->message->buffer->len, 2, "Partially decoded");
	
	// Send the third part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P3);
	test_status_eq(rfc6455_continue(client), CLIENT_GOOD, "Short message response: CLIENT_GOOD");
	test_uint16_eq(client->message->remaining_length, 0, "Waiting for 0 more characters");
	test_size_eq(client->message->buffer->len, 4, "Completely decoded");
	
	test_str_eq(client->message->buffer->str, MESSAGE_SHORT, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_multi_partial_broken_header) {
	client_t *client = u_client_create();
	
	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P1);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_WAIT, "Short message response: CLIENT_WAIT");
	test_int32_eq(client->message->mask, 0, "Mask was not read, so not set");
	test_size_eq(client->message->buffer->len, 0, "Nothing was added to the buffer");
	
	// If the message header is broken, it should be treated as though no
	// messsage existed in the first place
	test_int32_eq(client->message->remaining_length, 0, "No message read");
	
	// Send the rest of the message
	g_string_append(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P2);
	test_status_eq(rfc6455_incoming(client), CLIENT_GOOD, "Message finally read");
	test_str_eq(client->message->buffer->str, MESSAGE_SHORT, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_long_message) {
	client_t *client = u_client_create();
	
	g_string_overwrite_len(client->message->socket_buffer, 0, RFC6455_MESSAGE_LONG, sizeof(RFC6455_MESSAGE_LONG)-1);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_GOOD, "The entire message was read");
	test_char_eq(client->message->type, op_text, "Opcode set to text");
	test_uint32_eq(client->message->mask, *((guint32*)MASK), "Correct mask in message");
	test_str_eq(client->message->buffer->str, MESSAGE_LONG, "Message decoded correctly");
	
	u_client_free(client);
}
END_TEST


START_TEST(test_rfc6455_oversized_message) {
	client_t *client = u_client_create();
	
	g_string_overwrite_len(client->message->socket_buffer, 0, RFC6455_MESSAGE_OVERSIZED_MESSAGE, sizeof(RFC6455_MESSAGE_OVERSIZED_MESSAGE)-1);
	
	test_status_eq(rfc6455_incoming(client), CLIENT_MESSAGE_TOO_LONG, "The message was too large");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_frame_short) {
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, FALSE, MESSAGE_SHORT, sizeof(MESSAGE_SHORT)-1, &frame_len);
	
	test_int32_eq(frame_len, sizeof(RFC6455_FRAMED_SHORT)-1, "Frame length correct");
	test_bin_eq(frame, RFC6455_FRAMED_SHORT, sizeof(RFC6455_FRAMED_SHORT)-1, "Frame contains header");
	
	free(frame);
}
END_TEST

START_TEST(test_rfc6455_frame_long) {
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, FALSE, MESSAGE_LONG, sizeof(MESSAGE_LONG)-1, &frame_len);
	
	test_int32_eq(frame_len, sizeof(RFC6455_FRAMED_LONG)-1, "Frame length correct");
	test_bin_eq(frame, RFC6455_FRAMED_LONG, sizeof(RFC6455_FRAMED_LONG)-1, "Frame contains header");
	
	free(frame);
}
END_TEST

START_TEST(test_rfc6455_frame_oversized) {
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, FALSE, "", RFC6455_OVERSIZED_LEN, &frame_len);
	
	test_int32_eq(frame_len, 0, "Frame length correct");
	test_ptr_eq(frame, NULL, "Frame is null");
	
	free(frame);
}
END_TEST

START_TEST(test_rfc6455_pong) {
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_pong, FALSE, MESSAGE_SHORT, sizeof(MESSAGE_SHORT)-1, &frame_len);
	
	test_int32_eq(frame_len, sizeof(RFC6455_FRAMED_PONG)-1, "Frame length correct");
	test_bin_eq(frame, RFC6455_FRAMED_PONG, sizeof(RFC6455_FRAMED_PONG)-1, "Pong frame contains pong");
	
	free(frame);
}
END_TEST

START_TEST(test_rfc6455_frame_from_message) {
	message_t message;
	message.type = op_text;
	message.buffer = g_string_new(MESSAGE_SHORT);
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame_from_message(&message, &frame_len);
	
	test_int32_eq(frame_len, sizeof(RFC6455_FRAMED_SHORT)-1, "Correct short frame length");
	test_bin_eq(frame, RFC6455_FRAMED_SHORT, sizeof(RFC6455_FRAMED_SHORT)-1, "Short frame correct");
	
	g_string_free(message.buffer, TRUE);
	free(frame);
}
END_TEST

Suite* rfc6455_suite() {
	TCase *tc;
	Suite *s = suite_create("RFC6455");
	
	tc = tcase_create("Handshake");
	tcase_add_test(tc, test_rfc6455_handshake);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Recieve Messages");
	tcase_add_test(tc, test_rfc6455_short_message);
	tcase_add_test(tc, test_rfc6455_long_message);
	tcase_add_test(tc, test_rfc6455_oversized_message);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Partial Messages");
	tcase_add_test(tc, test_rfc6455_partial_no_header);
	tcase_add_test(tc, test_rfc6455_partial_no_header_long);
	tcase_add_test(tc, test_rfc6455_partial_short_message);
	tcase_add_test(tc, test_rfc6455_multi_partial_short_messages);
	tcase_add_test(tc, test_rfc6455_multi_partial_broken_header);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Send Frames");
	tcase_add_test(tc, test_rfc6455_frame_short);
	tcase_add_test(tc, test_rfc6455_frame_long);
	tcase_add_test(tc, test_rfc6455_frame_oversized);
	tcase_add_test(tc, test_rfc6455_pong);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Send from Message");
	tcase_add_test(tc, test_rfc6455_frame_from_message);
	suite_add_tcase(s, tc);
	
	return s;
}