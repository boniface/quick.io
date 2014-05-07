/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

int main(int argc, char **argv)
{
	qio_main(argc, argv);

	if (cfg_run_app_tests) {
		apps_test();
	} else {
		qev_wait_for_exit();
	}

	return 0;
}
