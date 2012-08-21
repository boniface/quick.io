#include <unistd.h>
#include <stdio.h>

#include "test.h"

#define TEST_EPOLL_WAIT 100

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

START_TEST(test_socket_message_clean_0) {
	client_t *client = u_client_create();
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	socket_message_clean(client, TRUE, TRUE);
	
	test_size_eq(sb->len, 0, "Socket Buffer truncated");
	test_size_eq(b->len, 0, "Buffer truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_socket_message_clean_1) {
	client_t *client = u_client_create();
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	socket_message_clean(client, TRUE, FALSE);
	
	test_size_eq(sb->len, 0, "Socket Buffer truncated");
	test_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_socket_message_clean_2) {
	client_t *client = u_client_create();
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	socket_message_clean(client, FALSE, TRUE);
	
	test_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer truncated");
	test_size_eq(b->len, 0, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_socket_message_clean_3) {
	client_t *client = u_client_create();
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	socket_message_clean(client, FALSE, FALSE);
	
	test_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer not truncated");
	test_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

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
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
}
END_TEST

START_TEST(test_socket_timeout) {
	int sock = u_connect();
	
	test(sock, "Connection established");
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	send(sock, frame, frame_len-5, MSG_NOSIGNAL);
	
	// Wait for the timeout, and then some to avoid race conditions
	usleep(SEC_TO_USEC(option_timeout()) + MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_messages, 0, "Server didn't process message");
	test_size_eq(utils_stats()->socket_timeouts, 1, "Client timed out");
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
	
	test_size_eq(utils_stats()->socket_messages, 1, "Server processed 1 message");
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
}
END_TEST

START_TEST(test_socket_too_much_data) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	char buff[1024];
	memset(&buff, 'a', sizeof(buff));
	
	// Don't buffer anything
	int flag = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	// Flood the socket with tons of data and make sure it gets closed
	for (int i = 0; i < 5; i++) {
		send(sock, buff, sizeof(buff), MSG_NOSIGNAL);
	}
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_int32_eq(send(sock, buff, sizeof(buff), MSG_NOSIGNAL), -1, "Socket closed");
	
	close(sock);
	
	test_size_eq(utils_stats()->socket_messages, 0, "Server accepted no messages");
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
	test_size_eq(utils_stats()->socket_hups, 0, "Client didn't close");
	test_size_eq(utils_stats()->socket_bad_clients, 1, "Client was forcibly closed");
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
	test_int64_eq(send(sock, f->str, f->len, MSG_NOSIGNAL), f->len, "Messages sent");
	
	// The server should respond with two pings, make sure it does
	// +2 -> for the headers
	// *2 -> the two messages
	char buff[sizeof(PING_DOUBLE_RESPONSE)];
	memset(buff, 0, sizeof(buff));
	
	gsize got = 0, i = 3;
	while (i-- && (got += read(sock, buff + got, sizeof(buff))) < (sizeof(buff)-1));
	
	test_str_eq(buff, PING_DOUBLE_RESPONSE, "Two messages recieved");
	
	g_string_free(f, TRUE);
	close(sock);
	
	test_size_eq(utils_stats()->socket_messages, 2, "Server processed 2 messages");
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
}
END_TEST

START_TEST(test_socket_abort_before_response) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	// Send a bunch of messages so that tons of writes will be done to the socket
	GString *f = g_string_new_len(frame, frame_len);
	g_string_append_len(f, frame, frame_len);
	g_string_append_len(f, frame, frame_len);
	g_string_append_len(f, frame, frame_len);
	free(frame);
	
	// Make sure the messages are sent as close together as possible
	test_lock_acquire();
	test_int64_eq(send(sock, f->str, f->len, MSG_NOSIGNAL), f->len, "Messages sent");
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	g_string_free(f, TRUE);
	close(sock);
	test_lock_release();
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
	test_size_eq(utils_stats()->socket_bad_clients, 1, "Client was bad for closing before getting message");
	test_size_eq(utils_stats()->socket_hups, 0, "Client HUP not recieved");
}
END_TEST

START_TEST(test_socket_close_after_message) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Don't buffer anything
	int flag = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	test_lock_acquire();
	send(sock, frame, frame_len, MSG_NOSIGNAL);
	test_lock_release();
	
	// Make sure the epoll has time to respond
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	// Send a message and a close, this message should not go through
	test_lock_acquire();
	send(sock, frame, frame_len, MSG_NOSIGNAL);
	close(sock);
	test_lock_release();
	
	free(frame);
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_messages, 1, "Second message not processed");
	test_size_eq(utils_stats()->socket_hups, 1, "Client HUPed");
	test_size_eq(utils_stats()->socket_bad_clients, 0, "Server left client open");
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
}
END_TEST

START_TEST(test_socket_close_partial_message) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Don't buffer anything
	int flag = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	test_lock_acquire();
	send(sock, frame, frame_len-5, MSG_NOSIGNAL);
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	close(sock);
	test_lock_release();
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_messages, 1, "Partial message recieved");
	test_size_eq(utils_stats()->socket_hups, 1, "Client HUPed");
	test_size_eq(utils_stats()->socket_bad_clients, 0, "Server left client open");
	test_size_eq(utils_stats()->socket_client_wait, 1, "Client was waited for");
}
END_TEST

START_TEST(test_socket_close_bad_message) {
	// Steps:
	//    1) Client sends a bad message
	//    2) Before the server can close the client, client closes
	//    3) Server closes the client
	//    4) epoll SHOULD NOT send a HUP, thus double-freeing the client
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Don't buffer anything
	int flag = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	char buff[100];
	memset(&buff, 0, sizeof(buff));
	
	test_lock_acquire();
	send(sock, buff, sizeof(buff), MSG_NOSIGNAL);
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	close(sock);
	test_lock_release();
	
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_messages, 1, "Bad message accepted");
	test_size_eq(utils_stats()->socket_hups, 0, "Client did not HUP");
	test_size_eq(utils_stats()->socket_bad_clients, 1, "Server left client open");
	test_size_eq(utils_stats()->socket_client_wait, 0, "Client was not waited for");
}
END_TEST

START_TEST(test_socket_two_partial_messages) {
	int sock = u_ws_connect();
	
	test(sock, "Connection established");
	
	// Don't buffer anything
	int flag = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	gsize frame_len = 0;
	char *frame = rfc6455_prepare_frame(op_text, TRUE, PING, sizeof(PING)-1, &frame_len);
	
	gsize loc = 0;
	send(sock, frame, frame_len - 10, MSG_NOSIGNAL);
	loc += frame_len - 10;
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	send(sock, frame + loc, 5, MSG_NOSIGNAL);
	loc += 5;
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	send(sock, frame + loc, 5, MSG_NOSIGNAL);
	usleep(MS_TO_USEC(TEST_EPOLL_WAIT));
	
	test_size_eq(utils_stats()->socket_messages, 3, "Partial messages recieved");
	test_size_eq(utils_stats()->socket_bad_clients, 0, "Server left client open");
	test_size_eq(utils_stats()->socket_client_wait, 2, "Client was waited for");
}
END_TEST

Suite* socket_suite() {
	TCase *tc;
	Suite *s = suite_create("Socket");
	
	tc = tcase_create("Utilities");
	tcase_add_test(tc, test_socket_message_clean_0);
	tcase_add_test(tc, test_socket_message_clean_1);
	tcase_add_test(tc, test_socket_message_clean_2);
	tcase_add_test(tc, test_socket_message_clean_3);
	suite_add_tcase(s, tc);
	
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
	tcase_add_test(tc, test_socket_abort_before_response);
	tcase_add_test(tc, test_socket_close_after_message);
	tcase_add_test(tc, test_socket_close_partial_message);
	tcase_add_test(tc, test_socket_close_bad_message);
	tcase_add_test(tc, test_socket_two_partial_messages);
	suite_add_tcase(s, tc);
	
	return s;
}