#include "check.h"
#include "test.h"

START_TEST(test_client_bad_headers) {
	client_t *client = u_client_create(NULL);
	
	const char *headers = "GET /qio HTTP/1.1\r\n"
		"Host: server.example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Origin: ""\x00""\r\n\r\n";
	
	g_string_overwrite_len(client->message->socket_buffer, 0, headers, strlen(headers)+5);
	test_status_eq(client_handshake(client), CLIENT_WRITE, "Path rejected");
	
	u_client_free(client);
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
	tcase_add_test(tc, test_client_bad_headers);
	suite_add_tcase(s, tc);
	
#if TEST_OUTPUT_XML
	srunner_set_xml(sr, "test_xunit_valgrind.xml");
#endif
	
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}