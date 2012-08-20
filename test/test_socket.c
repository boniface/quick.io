#include <unistd.h>
#include <stdio.h>

#include "test.h"

#define BAD_MESSAGE "TEST"

#define PING "/ping:123:plain=pingeth"
#define PING_RESPONSE "\x81""\x1d""/callback/123:0:plain=pingeth"
#define PING_DOUBLE_RESPONSE PING_RESPONSE PING_RESPONSE

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

/**
 * For coverage, just make sure that a HUP gets to the server.
 */
START_TEST(test_socket_close) {
	int sock = u_connect();
	
	test(sock, "Connection established");
	
	close(sock);
	
	usleep(MS_TO_USEC(100));
}
END_TEST

START_TEST(test_socket_timeout) {
	int sock = u_connect();
	
	test(sock, "Connection established");
	
	test_int32_eq(send(sock, BAD_MESSAGE, sizeof(BAD_MESSAGE), 0), sizeof(BAD_MESSAGE), "Message sent");
	
	// Wait for the timeout, and then some to avoid race conditions
	usleep(SEC_TO_USEC(option_timeout()) + MS_TO_USEC(100));
	
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
	char buff[sizeof(PING_RESPONSE)];
	memset(&buff, 0, sizeof(buff));
	
	// -1 -> the null terminator doesn't count
	test_size_eq(read(sock, buff, sizeof(buff)), sizeof(buff)-1, "Headers and all sent");
	test_str_eq(buff, PING_RESPONSE, "Decoded message");
	
	close(sock);
}
END_TEST

START_TEST(test_socket_too_much_data) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	char buff[1024];
	memset(&buff, 'a', sizeof(buff));
	
	// Don't buffer anything
	int flag = 1;\
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	// Flood the socket with tons of data and make sure it gets closed
	for (int i = 0; i < 5; i++) {
		send(sock, buff, sizeof(buff), MSG_NOSIGNAL);
	}
	
	usleep(MS_TO_USEC(100));
	
	test_int32_eq(send(sock, buff, sizeof(buff), MSG_NOSIGNAL), -1, "Socket closed");
	
	close(sock);
}
END_TEST

START_TEST(test_socket_two_messages) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	GString *f = g_string_new_len(frame, frame_len);
	g_string_append_len(f, frame, frame_len);
	free(frame);
	
	// Make sure the two messages are sent as close together as possible
	test_int64_eq(send(sock, f->str, f->len, MSG_NOSIGNAL), f->len, "Message sent");
	
	// The server should respond with two pings, make sure it does
	// +2 -> for the headers
	// *2 -> the two messages
	char buff[sizeof(PING_DOUBLE_RESPONSE)];
	memset(buff, 0, sizeof(buff));
	
	gsize got = 0, i = 3;
	while (i-- && (got += read(sock, buff + got, sizeof(buff))) < (sizeof(buff)-1));
	
	test_str_eq(buff, PING_DOUBLE_RESPONSE, "Two messages recieved");
	
	close(sock);
}
END_TEST

START_TEST(test_socket_close_after_message) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Don't buffer anything
	int flag = 1;\
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	test_lock_acquire();
	send(sock, frame, frame_len, MSG_NOSIGNAL);
	
	usleep(SEC_TO_USEC(1));
	
	send(sock, frame, frame_len, MSG_NOSIGNAL);
	test_lock_release();
	
	usleep(SEC_TO_USEC(1));
	
	test_lock_acquire();
	// close(sock);
	test_lock_release();
	
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
	tcase_add_test(tc, test_socket_close);
	tcase_add_test(tc, test_socket_timeout);
	tcase_add_test(tc, test_socket_handshake);
	tcase_add_test(tc, test_socket_ping);
	tcase_add_test(tc, test_socket_too_much_data);
	tcase_add_test(tc, test_socket_two_messages);
	// tcase_add_test(tc, test_socket_close_after_message);
	suite_add_tcase(s, tc);
	
	return s;
}