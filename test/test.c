#include <libgen.h>

#include "check.h"
#include "test.h"

int main(int argc, char *argv[]) {
	debug_handle_signals();
	
	// Setup the cross-process locking mechanism
	if (!test_lock_init()) {
		ERROR("Could not setup inter-process locking.  Failing.");
		return 1;
	}

	// Move into the directory holding this binary
	chdir(dirname(argv[0]));

	int number_failed = 0;
	
	SRunner *sr = srunner_create(suite_create("Master"));
	
#if TEST_OUTPUT_XML
	srunner_set_xml(sr, "test_unit.xml");
#endif
	
	srunner_add_suite(sr, apps_suite());
	srunner_add_suite(sr, client_suite());
	srunner_add_suite(sr, conns_suite());
	srunner_add_suite(sr, events_client_suite());
	srunner_add_suite(sr, events_server_suite());
	srunner_add_suite(sr, h_rfc6455_suite());
	srunner_add_suite(sr, h_flash_policy_suite());
	srunner_add_suite(sr, main_suite());
	srunner_add_suite(sr, monkey_suite());
	srunner_add_suite(sr, option_suite());
	srunner_add_suite(sr, stats_suite());

	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);
	
	test_lock_close();
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
