#include "test.h"

void _setup() {
	char *argv[] = {"./server", "-c", "../quickio.valgrind.ini"};
	int argc = G_N_ELEMENTS(argv);

	qev_init();
	utils_stats_setup();
	option_parse_args(argc, argv, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	evs_server_init();
	conns_init();
	apps_run();
	check(stats_init());
}

START_TEST(test_client_bad_headers) {
	client_t *client = u_client_create(NULL);

	const char headers[] = "GET /qio HTTP/1.1\r\n"
		"Host: server.example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Origin: ""\x00""\r\n\r\n";

	g_string_overwrite_len(client->message->socket_buffer, 0, headers, sizeof(headers)-1);
	check_status_eq(client_handshake(client), CLIENT_WRITE, "Null byte ignored");

	u_client_free(client);
}
END_TEST

START_TEST(test_rfc6455_messages) {
	client_t *client = u_client_create(NULL);

	const char msg[] = "\x81\x84""abcd""\x15";

	// Make sure it handles a new message on the end
	const char msg2[] = "\x07""\x10""\x10""\x81\x84""abcd";

	g_string_overwrite_len(client->message->socket_buffer, 0, msg, strlen(msg));
	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT, "Waiting for more");

	g_string_overwrite_len(client->message->socket_buffer, 0, msg2, strlen(msg2));
	check_status_eq(h_rfc6455_continue(client), CLIENT_GOOD, "Read the entire message");

	g_string_truncate(client->message->buffer, 0);
	check_status_eq(h_rfc6455_incoming(client), CLIENT_WAIT, "Waiting for more");

	u_client_free(client);
}
END_TEST

START_TEST(test_conns_clients) {
	for (int i = 0; i < 510; i++) {
		client_t *client = u_client_create(NULL);
		conns_client_new(client);
	}

	check_uint64_eq(stats.clients, 500, "Only 500 accepted");
}
END_TEST

int main(int argc, char *argv[]) {
	if (!log_init()) {
		CRITICAL("Could not init log");
		return 1;
	}

	// Move into the directory holding this binary
	chdir(dirname(argv[0]));

	int number_failed = 0;

	Suite *s = suite_create("Valgrind");
	SRunner *sr = srunner_create(s);

	TCase *tc;
	tc = tcase_create("Memchecks");
	tcase_add_checked_fixture(tc, _setup, NULL);
	tcase_add_test(tc, test_client_bad_headers);
	tcase_add_test(tc, test_rfc6455_messages);
	tcase_add_test(tc, test_conns_clients);
	suite_add_tcase(s, tc);

#if TEST_OUTPUT_XML
	srunner_set_xml(sr, "test_xunit_valgrind.xml");
#endif

	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
