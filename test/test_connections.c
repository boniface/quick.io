#include "test.h"

/**
 * A lot of the test cases that are associated with Connections are part of test_qsys
 * because the two rely so heavily on each other, so go there if you don't see the
 * cases that you are looking for.
 */

#define PING "/ping:123:plain=pingeth"

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

Suite* conns_suite() {
	TCase *tc;
	Suite *s = suite_create("Connections");
	
	tc = tcase_create("Utilities");
	tcase_add_test(tc, test_conns_message_clean_0);
	tcase_add_test(tc, test_conns_message_clean_1);
	tcase_add_test(tc, test_conns_message_clean_2);
	tcase_add_test(tc, test_conns_message_clean_3);
	suite_add_tcase(s, tc);
	
	return s;
}