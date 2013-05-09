/**
 * Make sure that all of the app testing functions work properly.
 */
#define APP_TESTING
#include "app/qio_app.h"

static gint _some_val = 0;
static config_file_entry_t _config_options[] = {
	{"some-val", e_int, &_some_val},
};

gboolean app_init(app_on on) {
	APP_PARSE_CONFIG(_config_options);
	return TRUE;
}

/**
 * Allows you to override configuration options when using APP_PARSE_CONFIG
 */
void app_test_config() {
	_some_val = 9999;
}

/**
 * The function called to run all the tests
 */
gboolean app_test() {
	return _some_val == 9999;
}
