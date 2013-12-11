/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

int main(int argc, char **argv)
{
	qio_main(argc, argv);
	qev_wait_for_exit();
	return 0;
}
