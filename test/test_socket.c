#include <unistd.h>
#include <stdio.h>

#include "test.h"

#define BAD_MESSAGE "TEST"

#define PING "/ping:123:plain=pingeth"
#define PING_RESPONSE "/callback/123:0:plain=pingeth"

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
	int sock = u_ws_connect();
	close(sock);
}
END_TEST

START_TEST(test_socket_ping) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Time to cheat: there's an rfc6455 constructor that works, so use that for framing
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	send(sock, frame, frame_len, MSG_NOSIGNAL);
	free(frame);
	
	// +2 -> make room for the header
	char buff[sizeof(PING_RESPONSE) + 2];
	memset(&buff, 0, sizeof(buff));
	
	// -1 -> the null terminator doesn't count
	test_size_eq(read(sock, buff, sizeof(buff)), sizeof(buff)-1, "Headers and all sent");
	test_str_eq(buff + 2, PING_RESPONSE, "Decoded message");
	
	close(sock);
}
END_TEST

Suite* socket_suite() {
	TCase *tc;
	Suite *s = suite_create("Socket");
	
	tc = tcase_create("Connections");
	tcase_add_checked_fixture(tc, _socket_setup, _socket_teardown);
	tcase_set_timeout(tc, option_timeout() + 1);
	// tcase_add_test(tc, test_socket_accept);
	// tcase_add_test(tc, test_socket_timeout);
	// tcase_add_test(tc, test_socket_handshake);
	tcase_add_test(tc, test_socket_ping);
	suite_add_tcase(s, tc);
	
	return s;
}