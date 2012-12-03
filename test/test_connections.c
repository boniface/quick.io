#include "test.h"

/**
 * A lot of the test cases that are associated with Connections are part of test_qsys
 * because the two rely so heavily on each other, so go there if you don't see the
 * cases that you are looking for.
 */

#define MESSAGE "\x81\x92""abcd""N""\x13""\n""\x0b""N""\x12""\n""\n""\x06""X""R""^""\x11""\x0e""\x02""\r""\x0f""_"

#define PING "/ping:123:plain=pingeth"

#define MOVE "\x81\x16""/qio/move:0:plain=test"
#define MOVE2 "\x81\x17""/qio/move:0:plain=test2"

void _test_conns_setup() {
	qev_init();
	utils_stats_setup();
	evs_server_init();
	conns_init();
	apps_run();
	test(stats_init());
}

START_TEST(test_conns_client_closed) {
	client_t *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	client->state = cstate_running;
	
	g_string_append(client->message->socket_buffer, MESSAGE);
	conns_client_data(client);
	
	u_client_free(client);
}
END_TEST

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

START_TEST(test_conns_clients_foreach) {
	guint32 i = 0;
	
	// Remove a ton of clients to make sure the iterator keeps up
	gboolean _callback(client_t *client) {
		static gboolean remove = TRUE;
		
		if (remove) {
			for (int i = 0; i < CONNS_YIELD/2; i++) {
				_conns_clients_remove(g_ptr_array_index(_clients, 0));
				_conns_clients_remove(g_ptr_array_index(_clients, _clients->len - 1));
			}
			
			remove = FALSE;
		}
		
		i++;
		
		return TRUE;
	}
	
	for (int i = 0; i < CONNS_YIELD*2; i++) {
		client_t *client = u_client_create(NULL);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;
	}
	
	conns_clients_foreach(_callback);
	test_uint32_eq(i, CONNS_YIELD+1, "Correct number of clients called");
}
END_TEST

START_TEST(test_conns_clients_remove_0) {
	for (guint i = 0; i < 10; i++) {
		client_t *client = u_client_create(NULL);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;
		
		test_uint64_eq(client->clients_pos, i + 1, "Correct position");
	}
	
	for (guint i = 10; i > 0; i--) {
		client_t *client = g_ptr_array_index(_clients, i - 1);
		_conns_clients_remove(client);
		
		test_uint64_eq(client->clients_pos, 0, "Correct position");
	}
}
END_TEST

START_TEST(test_conns_clients_remove_1) {
	client_t *client1 = u_client_create(NULL);
	conns_client_new(client1);
	
	client_t *client2 = u_client_create(NULL);
	conns_client_new(client2);
	
	client_t *client3 = u_client_create(NULL);
	conns_client_new(client3);
	
	_conns_clients_remove(client2);
	
	test_uint64_eq(client3->clients_pos, 2, "Correct position");
}
END_TEST

START_TEST(test_conns_balance_0) {
	conns_balance(100, "test");
	
	test_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
}
END_TEST

START_TEST(test_conns_balance_1) {
	conns_balance(100, "test");
	
	int socket = 0;
	client_t *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	conns_client_new(client);
	client->state = cstate_running;
	
	test_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
	_conns_balance();
	test_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");
	
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
	client1->state = cstate_running;
	
	int socket2 = 0;
	client_t *client2 = u_client_create(&socket2);
	client2->handler = h_rfc6455;
	conns_client_new(client2);
	client2->state = cstate_running;
	
	test_int64_eq(g_async_queue_length(_balances), 2, "Two requests");
	_conns_balance();
	test_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");
	
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

START_TEST(test_conns_balance_yield) {
	conns_balance(200, "test");
	
	int socket = 0;
	for (int i = 0; i < 200; i++) {
		client_t *client = u_client_create(&socket);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;
	}
	
	test_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
	_conns_balance();
	test_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");
	
	char buff[sizeof(MOVE)+128];
	memset(buff, 0, sizeof(buff));
	test_int32_eq(read(socket, buff, sizeof(buff)-1), sizeof(MOVE)-1, "Got MOVE length");
	test_bin_eq(buff, MOVE, sizeof(MOVE), "Correct MOVE sent");
}
END_TEST

Suite* conns_suite() {
	TCase *tc;
	Suite *s = suite_create("Connections");
	
	tc = tcase_create("Client Writing");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_client_closed);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Utilities");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_message_clean_0);
	tcase_add_test(tc, test_conns_message_clean_1);
	tcase_add_test(tc, test_conns_message_clean_2);
	tcase_add_test(tc, test_conns_message_clean_3);
	tcase_add_test(tc, test_conns_clients_foreach);
	tcase_add_test(tc, test_conns_clients_remove_0);
	tcase_add_test(tc, test_conns_clients_remove_1);
	suite_add_tcase(s, tc);
	
	tc = tcase_create("Balance");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_balance_0);
	tcase_add_test(tc, test_conns_balance_1);
	tcase_add_test(tc, test_conns_balance_2);
	tcase_add_test(tc, test_conns_balance_yield);
	suite_add_tcase(s, tc);
	
	return s;
}