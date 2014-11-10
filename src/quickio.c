/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

void qio_main(int argc, char **argv)
{
	// @todo qev_cfg_set_default_file() when building for release
	evs_pre_init();
	qev_init("quickio", argv, argc);
	config_init();
	evs_init();
	sub_init();
	protocols_init();
	client_init();
	periodic_init();
	qev_run();
}
