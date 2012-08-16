#include <unistd.h>
#include <stdio.h>

#include "test.h"

static pid_t _server;

static void _main_setup() {
	int out[2];
	test_int32_eq(pipe(out), 0, "Pipes ready");
	
	_server = fork();
	if (_server) {
		close(out[1]);
		
		// The server needs some time to get started
		sleep(1);
		
		// Wait for the server to emit "READY", then we can run our tests
		char buff[200];
		memset(&buff, 0, sizeof(buff));
		read(out[0], buff, sizeof(buff));
		test_str_eq(buff, "READY\n", "Server inited");
	} else {
		close(out[0]);
		// Setup the child's stdin/stdout to go to the pipes in the parent
		dup2(out[1], STDOUT_FILENO);
		
		char *args[] = {"."};
		init_main(1, args);
	}
}

static void _main_teardown() {
	kill(_server, SIGTERM);
}

START_TEST(test_main_init) {
	
}
END_TEST

Suite* main_suite() {
	TCase *tc;
	Suite *s = suite_create("Option");
	
	tc = tcase_create("Client Connection");
	tcase_add_checked_fixture(tc, _main_setup, _main_teardown);
	tcase_add_test(tc, test_main_init);
	suite_add_tcase(s, tc);
	
	return s;
}