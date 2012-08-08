#include "test.h"
#include "test_rfc6455.h"

char* test_status_to_str(status_t status) {
	switch (status) {
		#include "client_statuses.inc"
	}
	return "UNKNOWN";
}

int main(int argc, char *argv[]) {
	int number_failed = 0;
	
	SRunner *sr = srunner_create(rfc6455_suite());

#if TEST_OUTPUT_XML
	srunner_set_xml(sr, "../test_xunit.xml");
#endif

	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
