#include <libgen.h>

#include "test.h"

int main(int argc, char *argv[]) {
	// Move into the directory holding this binary
	chdir(dirname(argv[0]));

	int number_failed = 0;
	
	SRunner *sr = srunner_create(suite_create("Master"));
	srunner_add_suite(sr, client_suite());
	srunner_add_suite(sr, events_server_suite());
	srunner_add_suite(sr, main_suite());
	srunner_add_suite(sr, option_suite());
	srunner_add_suite(sr, rfc6455_suite());

#if TEST_OUTPUT_XML
	srunner_set_xml(sr, "../test_unit.xml");
#endif

	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
