#include <unistd.h>
#include <stdio.h>

#include "test.h"

#define BAD_MESSAGE "TEST"

#define HANDSHAKE "GET /chat HTTP/1.1\r\n" \
	"Host: server.example.com\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" \
	"Origin: http://example.com\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

#define HANDSHAKE_RESPONSE "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"

static pid_t _server;

static void _socket_setup() {
	u_main_setup(&_server);
}

static void _socket_teardown() {
	u_main_teardown(_server);
}

START_TEST(test_socket_accept) {
	int sock = u_connect();
	
	test(sock, "Connection established");
	
	close(sock);
}
END_TEST

START_TEST(test_socket_timeout) {
	int sock = u_connect();
	
	test(sock, "Connection established");
	
	test_int32_eq(send(sock, BAD_MESSAGE, sizeof(BAD_MESSAGE), 0), sizeof(BAD_MESSAGE), "Message sent");
	
	// Wait for the timeout, and then some to avoid race conditions
	sleep(option_timeout() + 1);
	
	send(sock, BAD_MESSAGE, sizeof(BAD_MESSAGE), MSG_NOSIGNAL);
	test_int32_eq(send(sock, BAD_MESSAGE, sizeof(BAD_MESSAGE), MSG_NOSIGNAL), -1, "Send failed");
}
END_TEST

START_TEST(test_socket_handshake) {
	int sock = u_connect();
	
	char buff[sizeof(HANDSHAKE_RESPONSE)];
	memset(&buff, 0, sizeof(buff));
	
	test(sock, "Connection established");
	test_int32_eq(send(sock, HANDSHAKE, sizeof(HANDSHAKE)-1, 0), sizeof(HANDSHAKE)-1, "Handshake sent");
	test_int32_eq(read(sock, buff, sizeof(buff)), sizeof(HANDSHAKE_RESPONSE)-1, "Correct response length read");
	
	test_str_eq(buff, HANDSHAKE_RESPONSE, "Correct response sent");
	
	close(sock);
}
END_TEST

Suite* socket_suite() {
	TCase *tc;
	Suite *s = suite_create("Socket");
	
	tc = tcase_create("Connections");
	tcase_add_checked_fixture(tc, _socket_setup, _socket_teardown);
	tcase_set_timeout(tc, option_timeout() + 1);
	tcase_add_test(tc, test_socket_accept);
	tcase_add_test(tc, test_socket_timeout);
	tcase_add_test(tc, test_socket_handshake);
	suite_add_tcase(s, tc);
	
	return s;
}