#include "test.h"

#define CONFIG_FILE "qio.ini"
#define QIOINI "[quick.io]\n" \
	"max-message-len = 8388608\n" \
	"[quick.io-apps]"

#define RFC6455_HANDSHAKE "GET /qio HTTP/1.1\n" \
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

#define NOT_RFC6455_HANDSHAKE "GET /qio HTTP/1.1\n" \
	"Host: server.example.com\n" \
	"Upgrade: websocket\n" \
	"Connection: Upgrade\n\n"

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

#define RFC6455_MESSAGE_OVERSIZED_MESSAGE "\x81""\xff""\x80""\x00""\x00""\x00""\x00""\x00""\x00""\x00"

#define RFC6455_FRAMED_SHORT "\x81""\x04"MESSAGE_SHORT
#define RFC6455_FRAMED_LONG "\x81""\x7e""\x00""\x9e"MESSAGE_LONG

#define RFC6455_FRAMED_PONG "\x8A""\x04"MESSAGE_SHORT

#define RFC6455_FRAME_NOT_FINAL "\x01""\x84"MASK"\x15\x07\x10\x10"
#define RFC6455_FRAME_CONTINUATION_NOTFINAL "\x00""\x84"MASK"\x15\x07\x10\x10"

#define RFC6455_FRAME_CLOSE "\x88""\x80"

#define RFC6455_FRAME_PING "\x89""\x84"MASK"\x15\x07\x10\x10"
#define RFC6455_FRAME_INCOMPLETE_PING "\x89""\x84"MASK

#define RFC6455_FRAME_NO_MASK "\x89""\x04"MASK MESSAGE_SHORT

#define RFC6455_FRAME_UNSUPPORTED_OPCODE "\x8B""\x84"MASK"\x15\x07\x10\x10"

#define RFC6455_FRAME_PING_0 "\x89""\x84"MASK
#define RFC6455_FRAME_PING_1 "\x15\x07\x10\x10"

START_TEST(test_h_rfc6455_handshake)
{
	struct client *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, RFC6455_HANDSHAKE);

	check_status_eq(client_handshake(client), CLIENT_WRITE,
			"Response should be written to client");
	check_str_eq(client->message->buffer->str, RFC6455_HANDSHAKE_REPONSE,
			"Correct handshake sent back");

	// Make sure the side-effects are correct
	check_size_eq(client->state, cstate_initing, "Client init flag updated");
	check_size_eq(client->message->socket_buffer->len, 0, "Socket buffer cleared");
	check_int32_eq(client->handler, h_rfc6455, "Correct handler applied");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_handshake_no_key)
{
	struct client *client = u_client_create(NULL);

	GHashTable *headers = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(headers, "Host", "server.example.com");
	g_hash_table_insert(headers, "Upgrade", "websocket");
	g_hash_table_insert(headers, "Connection", "Upgrade");

	check_status_eq(h_rfc6455_handshake(client, 0, headers), CLIENT_FATAL,
			"Client not supported");

	g_hash_table_unref(headers);
	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_short_message)
{
	struct client *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD,
			"Short message response: CLIENT_GOOD");
	check_size_eq(client->message->socket_buffer->len, 0,
			"Socket buffer cleared after read");
	check_uint16_eq(client->message->remaining_length, 0,
			"Message length==0 as buffer cleared");
	check_char_eq(client->message->type, op_text, "Opcode set to text");
	check_uint32_eq(client->message->mask, *((guint32*)MASK),
			"Correct mask in message");
	check_str_eq(client->message->buffer->str, MESSAGE_SHORT,
			"Message decoded correctly");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_partial_no_header)
{
	struct client *client = u_client_create(NULL);

	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_PARTIAL_NO_HEADER_P1);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Short header rejected");

	g_string_append(client->message->socket_buffer, RFC6455_MESSAGE_PARTIAL_NO_HEADER_P2);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD, "Message completed");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_partial_no_header_long)
{
	struct client *client = u_client_create(NULL);

	g_string_overwrite_len(client->message->socket_buffer,
				0, RFC6455_MESSAGE_LONG_BROKEN_HEADER_1,
				sizeof(RFC6455_MESSAGE_LONG_BROKEN_HEADER_1)-1);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT, "Large header rejected");

	g_string_append_len(client->message->socket_buffer,
				RFC6455_MESSAGE_LONG_BROKEN_HEADER_2,
				sizeof(RFC6455_MESSAGE_LONG_BROKEN_HEADER_2)-1);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD, "Message completed");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_partial_short_message)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Short message response: CLIENT_WAIT");
	check_size_eq(client->message->socket_buffer->len, 0,
			"Socket buffer cleared after read");
	check_uint16_eq(client->message->remaining_length, 4,
			"Waiting for 4 more characters");
	check_char_eq(client->message->type, op_text, "Opcode set to text");
	check_uint32_eq(client->message->mask, *((guint32*)MASK),
			"Correct mask in message");
	check_size_eq(client->message->buffer->len, 0, "Nothing in the message buffer");

	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_SHORT_P2);
	check_status_eq(h_rfc6455_continue(client), CLIENT_GOOD,
			"Short message response: CLIENT_GOOD");
	check_size_eq(client->message->buffer->len, 4, "Message read completely");
	check_str_eq(client->message->buffer->str, MESSAGE_SHORT,
			"Message decoded correctly");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_multi_partial_short_messages)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Short message response: CLIENT_WAIT");

	// Send the second part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P2);
	check_status_eq(h_rfc6455_continue(client), CLIENT_WAIT,
			"Short message response: CLIENT_WAIT");
	check_uint16_eq(client->message->remaining_length, 2,
			"Waiting for 2 more characters");
	check_size_eq(client->message->buffer->len, 2, "Partially decoded");

	// Send the third part of the message
	g_string_assign(client->message->socket_buffer, RFC6455_MESSAGE_MULTI_P3);
	check_status_eq(h_rfc6455_continue(client), CLIENT_GOOD,
			"Short message response: CLIENT_GOOD");
	check_uint16_eq(client->message->remaining_length, 0,
			"Waiting for 0 more characters");
	check_size_eq(client->message->buffer->len, 4, "Completely decoded");

	check_str_eq(client->message->buffer->str, MESSAGE_SHORT,
			"Message decoded correctly");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_multi_partial_broken_header)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_assign(client->message->socket_buffer,
				RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Short message response: CLIENT_WAIT");
	check_int32_eq(client->message->mask, 0, "Mask was not read, so not set");
	check_size_eq(client->message->buffer->len, 0, "Nothing was added to the buffer");

	// If the message header is broken, it should be treated as though no
	// messsage existed in the first place
	check_int32_eq(client->message->remaining_length, 0, "No message read");

	// Send the rest of the message
	g_string_append(client->message->socket_buffer,
				RFC6455_MESSAGE_MULTI_BROKEN_HEADER_P2);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD, "Message finally read");
	check_str_eq(client->message->buffer->str,
				MESSAGE_SHORT, "Message decoded correctly");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_multi_partial_broken_header_64bit)
{
	struct client *client = u_client_create(NULL);

	GString *buff = g_string_sized_new(0);
	g_string_set_size(buff, 65792);

	for (guint i = 0; i < buff->len; i++) {
		buff->str[i] = (char)(i % 127) + 1;
	}

	gsize frame_len = 0;
	gchar *frame = h_rfc6455_prepare_frame(FALSE, op_text, TRUE,
							buff->str, buff->len, &frame_len);

	g_string_append_len(client->message->socket_buffer, frame, 4);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Waiting for the rest of the message");

	u_client_free(client);
	g_free(frame);
	g_string_free(buff, TRUE);
}
END_TEST

START_TEST(test_h_rfc6455_continuation)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_NOT_FINAL,
				sizeof(RFC6455_FRAME_NOT_FINAL)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD, "Parsed message okay");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_continuation_frame)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_CONTINUATION_NOTFINAL,
				sizeof(RFC6455_FRAME_CONTINUATION_NOTFINAL)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_FATAL, "Parsed message okay");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_close)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_CLOSE,
				sizeof(RFC6455_FRAME_CLOSE)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_FATAL, "Parsed message okay");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_ping)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_PING,
				sizeof(RFC6455_FRAME_PING)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WRITE, "Parsed message okay");
	check_str_eq(client->message->buffer->str, "test", "Correct response to ping");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_bad_ping)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_INCOMPLETE_PING,
				sizeof(RFC6455_FRAME_INCOMPLETE_PING)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT,
			"Waiting on rest of ping");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_no_mask)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_NO_MASK, sizeof(RFC6455_FRAME_NO_MASK)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_FATAL, "Parsed message okay");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_unsupported_opcode)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
				RFC6455_FRAME_UNSUPPORTED_OPCODE,
				sizeof(RFC6455_FRAME_UNSUPPORTED_OPCODE)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_FATAL, "Unsupported opcode!");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_partial_ping)
{
	struct client *client = u_client_create(NULL);

	// Send the first part of the message
	g_string_overwrite_len(client->message->socket_buffer, 0,
					RFC6455_FRAME_PING_0,
					sizeof(RFC6455_FRAME_PING_0)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT, "Waiting");

	g_string_append_len(client->message->socket_buffer,
					RFC6455_FRAME_PING_1,
					sizeof(RFC6455_FRAME_PING_1)-1);

	check_status_eq(h_rfc6455_continue(client), CLIENT_WRITE, "Writing stuff");

	check_str_eq(client->message->buffer->str, "test", "Correct ping response");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_close_response)
{
	gsize frame_len = 0;
	char *frame = h_rfc6455_close_frame(&frame_len);
	check_int32_eq(frame_len, 2, "Correct frame length");
	check_bin_eq(frame, "\x88" "\x00", 2, "Close frame");
}
END_TEST

START_TEST(test_h_rfc6455_long_message)
{
	struct client *client = u_client_create(NULL);

	g_string_overwrite_len(client->message->socket_buffer, 0,
					RFC6455_MESSAGE_LONG,
					sizeof(RFC6455_MESSAGE_LONG)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD,
			"The entire message was read");
	check_char_eq(client->message->type, op_text, "Opcode set to text");
	check_uint32_eq(client->message->mask, *((guint32*)MASK),
			"Correct mask in message");
	check_str_eq(client->message->buffer->str, MESSAGE_LONG,
			"Message decoded correctly");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_64bit_message)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(QIOINI, 1, sizeof(QIOINI), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "File ready");
	check(option_parse_config_file(NULL, NULL, 0), "Config loaded");

	GString *buff = g_string_sized_new(0);
	g_string_set_size(buff, 65792);

	for (guint i = 0; i < buff->len; i++) {
		buff->str[i] = (char)(i % 127) + 1;
	}

	gsize frame_len = 0;
	gchar *frame = h_rfc6455_prepare_frame(FALSE, op_text, TRUE,
							buff->str, buff->len, &frame_len);

	struct client *client = u_client_create(NULL);

	g_string_overwrite_len(client->message->socket_buffer, 0, frame,
							frame_len);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_GOOD,
			"Message accepted");
	check_uint64_eq(buff->len, client->message->buffer->len,
			"Got right size");
	check_str_eq(client->message->buffer->str, buff->str,
			"Strings match");

	u_client_free(client);
	g_string_free(buff, TRUE);
	unlink(CONFIG_FILE);
}
END_TEST

START_TEST(test_h_rfc6455_max_message_size)
{
	struct client *client = u_client_create(NULL);

	g_string_overwrite_len(client->message->socket_buffer, 0,
					RFC6455_MESSAGE_OVERSIZED_MESSAGE,
					sizeof(RFC6455_MESSAGE_OVERSIZED_MESSAGE)-1);

	check_status_eq(h_rfc6455_incoming(client), CLIENT_FATAL,
			"Message rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_h_rfc6455_frame_short)
{
	gsize frame_len = 0;
	gchar *frame = h_rfc6455_prepare_frame(FALSE, op_text, FALSE,
							MESSAGE_SHORT, sizeof(MESSAGE_SHORT)-1,
							&frame_len);

	check_int32_eq(frame_len, sizeof(RFC6455_FRAMED_SHORT)-1, "Frame length correct");
	check_bin_eq(frame, RFC6455_FRAMED_SHORT, sizeof(RFC6455_FRAMED_SHORT)-1,
				"Frame contains header");

	g_free(frame);
}
END_TEST

START_TEST(test_h_rfc6455_frame_long)
{
	gsize frame_len = 0;
	char *frame = h_rfc6455_prepare_frame(FALSE, op_text, FALSE,
							MESSAGE_LONG, sizeof(MESSAGE_LONG)-1,
							&frame_len);

	check_int32_eq(frame_len, sizeof(RFC6455_FRAMED_LONG)-1, "Frame length correct");
	check_bin_eq(frame, RFC6455_FRAMED_LONG, sizeof(RFC6455_FRAMED_LONG)-1,
			"Frame contains header");

	g_free(frame);
}
END_TEST

START_TEST(test_h_rfc6455_frame_64bit)
{
	GString *buff = g_string_sized_new(0);
	g_string_set_size(buff, 65792);

	gsize frame_len = 0;
	char *frame = h_rfc6455_prepare_frame(FALSE, op_text, FALSE, buff->str,
						buff->len, &frame_len);

	// 2 for WS header, 8 for 64bit len
	check_int32_eq(frame_len, buff->len + 8 + 2, "Frame length correct");
	check(frame != NULL, "Frame isn't null");

	g_free(frame);
	g_string_free(buff, TRUE);
}
END_TEST

START_TEST(test_h_rfc6455_pong)
{
	gsize frame_len = 0;
	char *frame = h_rfc6455_prepare_frame(FALSE, op_pong, FALSE,
						MESSAGE_SHORT, sizeof(MESSAGE_SHORT)-1,
						&frame_len);

	check_int32_eq(frame_len, sizeof(RFC6455_FRAMED_PONG)-1, "Frame length correct");
	check_bin_eq(frame, RFC6455_FRAMED_PONG, sizeof(RFC6455_FRAMED_PONG)-1,
			"Pong frame contains pong");

	g_free(frame);
}
END_TEST

START_TEST(test_h_rfc6455_frame_from_message)
{
	struct message message;
	message.type = op_text;
	message.buffer = g_string_new(MESSAGE_SHORT);

	gsize frame_len = 0;
	char *frame = h_rfc6455_prepare_frame_from_message(&message, &frame_len);

	check_int32_eq(frame_len, sizeof(RFC6455_FRAMED_SHORT)-1,
			"Correct short frame length");
	check_bin_eq(frame, RFC6455_FRAMED_SHORT, sizeof(RFC6455_FRAMED_SHORT)-1,
			"Short frame correct");

	g_string_free(message.buffer, TRUE);
	g_free(frame);
}
END_TEST

Suite* h_rfc6455_suite()
{
	TCase *tc;
	Suite *s = suite_create("Handler: RFC6455");

	tc = tcase_create("Handshake");
	tcase_add_test(tc, test_h_rfc6455_handshake);
	tcase_add_test(tc, test_h_rfc6455_handshake_no_key);
	suite_add_tcase(s, tc);

	tc = tcase_create("Recieve Messages");
	tcase_add_test(tc, test_h_rfc6455_short_message);
	tcase_add_test(tc, test_h_rfc6455_long_message);
	tcase_add_test(tc, test_h_rfc6455_64bit_message);
	tcase_add_test(tc, test_h_rfc6455_max_message_size);
	suite_add_tcase(s, tc);

	tc = tcase_create("Partial Messages");
	tcase_add_test(tc, test_h_rfc6455_partial_no_header);
	tcase_add_test(tc, test_h_rfc6455_partial_no_header_long);
	tcase_add_test(tc, test_h_rfc6455_partial_short_message);
	tcase_add_test(tc, test_h_rfc6455_multi_partial_short_messages);
	tcase_add_test(tc, test_h_rfc6455_multi_partial_broken_header);
	tcase_add_test(tc, test_h_rfc6455_multi_partial_broken_header_64bit);
	suite_add_tcase(s, tc);

	tc = tcase_create("Continuation Frames");
	tcase_add_test(tc, test_h_rfc6455_continuation);
	tcase_add_test(tc, test_h_rfc6455_continuation_frame);
	suite_add_tcase(s, tc);

	tc = tcase_create("Frame Types");
	tcase_add_test(tc, test_h_rfc6455_close);
	tcase_add_test(tc, test_h_rfc6455_ping);
	tcase_add_test(tc, test_h_rfc6455_bad_ping);
	tcase_add_test(tc, test_h_rfc6455_no_mask);
	tcase_add_test(tc, test_h_rfc6455_partial_ping);
	tcase_add_test(tc, test_h_rfc6455_close_response);
	suite_add_tcase(s, tc);

	tc = tcase_create("Send Frames");
	tcase_add_test(tc, test_h_rfc6455_frame_short);
	tcase_add_test(tc, test_h_rfc6455_frame_long);
	tcase_add_test(tc, test_h_rfc6455_frame_64bit);
	tcase_add_test(tc, test_h_rfc6455_pong);
	tcase_add_test(tc, test_h_rfc6455_unsupported_opcode);
	suite_add_tcase(s, tc);

	tc = tcase_create("Send from Message");
	tcase_add_test(tc, test_h_rfc6455_frame_from_message);
	suite_add_tcase(s, tc);

	return s;
}
