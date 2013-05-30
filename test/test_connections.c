#include "test.h"

/**
 * A lot of the test cases that are associated with Connections are part of test_qsys
 * because the two rely so heavily on each other, so go there if you don't see the
 * cases that you are looking for.
 */

#define MESSAGE "\x81\x92""abcd""N""\x13""\n""\x0b""N""\x12""\n""\n""\x06""X""R""^""\x11""\x0e""\x02""\r""\x0f""_"

#define PING "/ping:123:plain=pingeth"
#define CLOSE_FRAME "\x88" "\x00"

#define MOVE "\x81\x16""/qio/move:0:plain=test" CLOSE_FRAME
#define MOVE2 "\x81\x17""/qio/move:0:plain=test2" CLOSE_FRAME

void _test_conns_setup()
{
	qev_init();
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	evs_server_init();
	conns_init();
	apps_run();
	check(stats_init());
}

START_TEST(test_conns_client_closed_0)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	client->state = cstate_dead;

	g_string_append(client->message->socket_buffer, MESSAGE);
	check_not(conns_client_data(client), "Client dead");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_client_closed_1)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	g_string_append(client->message->socket_buffer, MESSAGE);
	conns_client_data(client);

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_0)
{
	struct client *client = u_client_create(NULL);

	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;

	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, TRUE, TRUE);

	check_size_eq(sb->len, 0, "Socket Buffer truncated");
	check_size_eq(b->len, 0, "Buffer truncated");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_1)
{
	struct client *client = u_client_create(NULL);

	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;

	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, TRUE, FALSE);

	check_size_eq(sb->len, 0, "Socket Buffer truncated");
	check_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_2)
{
	struct client *client = u_client_create(NULL);

	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;

	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, FALSE, TRUE);

	check_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer truncated");
	check_size_eq(b->len, 0, "Buffer not truncated");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_message_clean_3)
{
	struct client *client = u_client_create(NULL);

	GString *b = client->message->buffer;
	GString *sb = client->message->socket_buffer;

	g_string_append(b, PING);
	g_string_append(sb, PING);
	conns_message_clean(client, FALSE, FALSE);

	check_size_eq(sb->len, sizeof(PING)-1, "Socket Buffer not truncated");
	check_size_eq(b->len, sizeof(PING)-1, "Buffer not truncated");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_clients_foreach)
{
	guint32 i = 0;

	// Remove a ton of clients to make sure the iterator keeps up
	gboolean _callback(struct client *client) {
		static gboolean remove = TRUE;

		if (remove) {
			for (int i = 0; i < CONNS_YIELD/2; i++) {
				_conns_clients_remove(_clients[0]);
				_conns_clients_remove(_clients[_clients_len - 1]);
			}

			remove = FALSE;
		}

		i++;

		return TRUE;
	}

	for (int i = 0; i < CONNS_YIELD*2; i++) {
		struct client *client = u_client_create(NULL);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;
	}

	conns_clients_foreach(_callback);
	check_uint32_eq(i, CONNS_YIELD+1, "Correct number of clients called");
}
END_TEST

START_TEST(test_conns_clients_foreach_race)
{
	struct client *client = u_client_create(NULL);
	conns_client_new(client);

	int calls = 0;

	gboolean _callback(struct client *client) {
		calls++;
		return TRUE;
	}

	gpointer _race(gpointer nothing) {
		conns_clients_foreach(_callback);
		return NULL;
	}

	qev_lock_write_lock(&_clients_lock);

	g_thread_new("_race", _race, NULL);

	usleep(MS_TO_USEC(10));

	_clients[0] = _clients[_clients_len - 1];
	__sync_fetch_and_sub(&_clients_len, 1);
	__sync_fetch_and_sub(&_clients_len_next, 1);

	qev_lock_write_unlock(&_clients_lock);

	check_int32_eq(calls, 0, "No callbacks");
}
END_TEST

START_TEST(test_conns_clients_foreach_null)
{
	struct client *client = u_client_create(NULL);
	conns_client_new(client);
	client->state = cstate_running;

	struct client *client2 = u_client_create(NULL);
	conns_client_new(client2);
	client2->state = cstate_running;

	_conns_clients_remove(client2);

	// It's sossible for _clients_len to be incremented before the client
	// is inserted into the correct slot: two threads are going, one increments
	// _clients_len_next then is stopped by the OS; the other thread adds the client
	// entirely and increments _clients_len.  Now it thinks that there is a
	// client there, but there doesn't have to be: that can cause a segfault
	_clients_len += 5;

	int calls = 0;
	gboolean _callback(struct client *client) {
		calls++;
		return TRUE;
	}

	conns_clients_foreach(_callback);

	check_int32_eq(calls, 1, "Ignored empty client");
}
END_TEST

START_TEST(test_conns_clients_remove_0)
{
	for (guint i = 0; i < 10; i++) {
		struct client *client = u_client_create(NULL);
		client->handler = h_rfc6455;
		conns_client_new(client);
		client->state = cstate_running;

		check_uint64_eq(client->clients_pos, i + 1, "Correct position");
	}

	for (guint i = 10; i > 0; i--) {
		struct client *client = _clients[i - 1];
		_conns_clients_remove(client);

		check_uint64_eq(client->clients_pos, 0, "Correct position");
	}
}
END_TEST

START_TEST(test_conns_clients_remove_1)
{
	struct client *client1 = u_client_create(NULL);
	conns_client_new(client1);

	struct client *client2 = u_client_create(NULL);
	conns_client_new(client2);

	struct client *client3 = u_client_create(NULL);
	conns_client_new(client3);

	_conns_clients_remove(client2);

	check_uint64_eq(client3->clients_pos, 2, "Correct position");
}
END_TEST

START_TEST(test_conns_balance_0)
{
	conns_balance(100, "test");

	check_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
}
END_TEST

START_TEST(test_conns_balance_1)
{
	conns_balance(100, "test");

	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	check_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
	_conns_balance();
	check_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");

	char buff[sizeof(MOVE)+128];
	memset(buff, 0, sizeof(buff));
	check_int32_eq(read(socket, buff, sizeof(buff)-1), sizeof(MOVE)-1,
			"Got MOVE length");
	check_bin_eq(buff, MOVE, sizeof(MOVE), "Correct MOVE sent");
}
END_TEST

START_TEST(test_conns_balance_2)
{
	conns_balance(1, "test");
	conns_balance(1, "test2");

	int socket1 = 0;
	struct client *client1 = u_client_create(&socket1);
	conns_client_new(client1);
	client1->handler = h_rfc6455;
	client1->state = cstate_running;

	int socket2 = 0;
	struct client *client2 = u_client_create(&socket2);
	conns_client_new(client2);
	client2->handler = h_rfc6455;
	client2->state = cstate_running;

	check_int64_eq(g_async_queue_length(_balances), 2, "Two requests");
	_conns_balance();
	check_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");

	char buff1[sizeof(MOVE)+128];
	char buff2[sizeof(MOVE2)+128];
	memset(buff1, 0, sizeof(buff1));
	memset(buff2, 0, sizeof(buff2));

	check(read(socket1, buff1, sizeof(buff1)-1) != -1, "Got MOVE length");
	check(read(socket2, buff2, sizeof(buff2)-1) != -1, "Got MOVE length");

	if (strcmp(buff1, MOVE) == 0) {
		check_bin_eq(buff1, MOVE, sizeof(MOVE), "Correct MOVE sent");
		check_bin_eq(buff2, MOVE2, sizeof(MOVE2), "Correct MOVE sent");
	} else {
		check_bin_eq(buff1, MOVE2, sizeof(MOVE2), "Correct MOVE sent");
		check_bin_eq(buff2, MOVE, sizeof(MOVE), "Correct MOVE sent");
	}
}
END_TEST

START_TEST(test_conns_balance_yield)
{
	conns_balance(200, "test");

	int socket = 0;
	for (int i = 0; i < 200; i++) {
		struct client *client = u_client_create(&socket);
		conns_client_new(client);
		client->handler = h_rfc6455;
		client->state = cstate_running;
	}

	check_int64_eq(g_async_queue_length(_balances), 1, "One balance request");
	_conns_balance();
	check_int64_eq(g_async_queue_length(_balances), 0, "Requests cleared");

	char buff[sizeof(MOVE)+128];
	memset(buff, 0, sizeof(buff));
	check_int32_eq(read(socket, buff, sizeof(buff)-1),
			sizeof(MOVE)-1, "Got MOVE length");
	check_bin_eq(buff, MOVE, sizeof(MOVE), "Correct MOVE sent");
}
END_TEST

START_TEST(test_conns_max_clients)
{
	int socket = 0;
	for (int i = 0; i < 510; i++) {
		struct client *client = u_client_create(&socket);
		conns_client_new(client);
		client->handler = h_rfc6455;
		client->state = cstate_running;
	}

	check_uint64_eq(_clients_len, 500, "Only 500 accepted");

	for (int i = 0; i < 100; i++) {
		conns_client_close(_clients[0]);
	}

	for (int i = 0; i < 10; i++) {
		struct client *client = u_client_create(&socket);
		conns_client_new(client);
		client->handler = h_rfc6455;
		client->state = cstate_running;
	}

	check_uint64_eq(_clients_len, 410, "Accepted after going back down");
}
END_TEST

START_TEST(test_conns_client_close_free)
{
	// Tests the case when an app has client_ref'd a client: it MUST still receive a close
	// callback when the client leaves

	struct client *client = u_client_create(NULL);
	client_ref(client);
	client_ref(client);
	client_ref(client);

	// The notification should be fired immediately
	conns_client_close(client);

	check_size_eq(utils_stats()->apps_client_close, 2, "Single client disconnected, 2 apps fired");

	u_client_free(client);
}
END_TEST

Suite* conns_suite()
{
	TCase *tc;
	Suite *s = suite_create("Connections");

	tc = tcase_create("Client Writing");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_client_closed_0);
	tcase_add_test(tc, test_conns_client_closed_1);
	suite_add_tcase(s, tc);

	tc = tcase_create("Utilities");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_message_clean_0);
	tcase_add_test(tc, test_conns_message_clean_1);
	tcase_add_test(tc, test_conns_message_clean_2);
	tcase_add_test(tc, test_conns_message_clean_3);
	tcase_add_test(tc, test_conns_clients_foreach);
	tcase_add_test(tc, test_conns_clients_foreach_race);
	tcase_add_test(tc, test_conns_clients_foreach_null);
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

	tc = tcase_create("Limits");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_max_clients);
	suite_add_tcase(s, tc);

	tc = tcase_create("App Callbacks");
	tcase_add_checked_fixture(tc, _test_conns_setup, NULL);
	tcase_add_test(tc, test_conns_client_close_free);
	suite_add_tcase(s, tc);

	return s;
}
