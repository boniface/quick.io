#include "test.h"

/**
 * A lot of the test cases that are associated with Connections are part of test_qsys
 * because the two rely so heavily on each other, so go there if you don't see the
 * cases that you are looking for.
 */

#define PING "/ping:123:plain=pingeth"

#define MOVE "\x81\x16""/qio/move:0:plain=test"
#define MOVE2 "\x81\x17""/qio/move:0:plain=test2"

void _test_conns_setup() {
	apps_init();
	evs_server_init();
	conns_init();
}

START_TEST(test_conns_message_clean_0) {
	client_t *client = u_client_create(NULL);
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, TRUE, TRUE);
	
	test_size_eq(sb->len, 0, "Socket Buffer truncated");
	test_size_eq(b->len, 0, "Buffer truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_1) {
	client_t *client = u_client_create(NULL);
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, TRUE, FALSE);
	
	test_size_eq(sb->len, 0, "Socket Buffer truncated");
	test_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_2) {
	client_t *client = u_client_create(NULL);
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, FALSE, TRUE);
	
	test_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer truncated");
	test_size_eq(b->len, 0, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_3) {
	client_t *client = u_client_create(NULL);
	
	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;
	
	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, FALSE, FALSE);
	
	test_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer not truncated");
	test_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");
	
	u_client_free(client);
}
END_TEST

START_TEST(test_conns_balance_0) {
	conns_balance(100, "test");
	
	test_int64_eq(_balances->len, 1, "One balance request");
}
END_TEST

START_TEST(test_conns_balance_1) {
	conns_balance(100, "test");
	
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	conns_client_new(client);
	
	test_int64_eq(_balances->len, 1, "One balance request");
	_conns_balance();
	test_int64_eq(_balances->len, 0, "Requests cleared");
	
	char buff[sizeof(MOVE)+128];
	memset(buff, 0, sizeof(buff));
	test_int32_eq(read(socket, buff, sizeof(buff)-1), sizeof(MOVE)-1, "Got MOVE length");
	test_bin_eq(buff, MOVE, sizeof(MOVE), "Correct MOVE sent");
}
END_TEST

START_TEST(test_conns_balance_2) {
	conns_balance(1, "test");
	conns_balance(1, "test2");
	
	int socket1 = 0;
	client_t *client1 = u_client_create(&socket1);
	client1->handler = h_rfc6455;
	conns_client_new(client1);
	
	int socket2 = 0;
	client_t *client2 = u_client_create(&socket2);
	client2->handler = h_rfc6455;
	conns_client_new(client2);
	
	test_int64_eq(_balances->len, 2, "Two requests");
	_conns_balance();
	test_int64_eq(_balances->len, 0, "Requests cleared");
	
	char buff1[sizeof(MOVE)+128];
	char buff2[sizeof(MOVE2)+128];
	memset(buff1, 0, sizeof(buff1));
	memset(buff2, 0, sizeof(buff2));
	
	test(read(socket1, buff1, sizeof(buff1)-1) != -1, "Got MOVE length");
	test(read(socket2, buff2, sizeof(buff2)-1) != -1, "Got MOVE length");
	
	if (strcmp(buff1, MOVE) == 0) {
		test_bin_eq(buff1, MOVE, sizeof(MOVE), "Correct MOVE sent");
		test_bin_eq(buff2, MOVE2, sizeof(MOVE2), "Correct MOVE sent");
	} else {
		test_bin_eq(buff1, MOVE2, sizeof(MOVE2), "Correct MOVE sent");
		test_bin_eq(buff2, MOVE, sizeof(MOVE), "Correct MOVE sent");
	}
}
END_TEST

Suite* conns_suite() {
	TCase *tc;
	Suite *s = suite_create("Connections");
	
	tc = tcase_create("Utilities");
	tcase_add_test(tc, test_conns_message_clean_0);
	tcase_add_test(tc, test_conns_message_clean_1);
	tcase_add_test(tc, test_conns_message_clean_2);
	tcase_add_test(tc, test_conns_message_clean_3);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Balance");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_balance_0);
	tcase_add_test(tc, test_conns_balance_1);
	tcase_add_test(tc, test_conns_balance_2);
	suite_add_tcase(s, tc);
	
	return s;
}