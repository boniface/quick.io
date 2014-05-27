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
	evs_pre_init();
	qev_init("quickio", argv, argc);
	config_init();
	evs_init();
	sub_init();
	protocols_init();
	client_init();
	periodic_init();

	if (cfg_bind_address != NULL) {
		qev_listen(cfg_bind_address, cfg_bind_port);
	}

	if (cfg_bind_address_ssl != NULL) {
		qev_listen_ssl(cfg_bind_address_ssl, cfg_bind_port_ssl,
					cfg_ssl_cert_path_0, cfg_ssl_key_path_0,
					cfg_ssl_cert_path_1, cfg_ssl_key_path_1);
	}

	if (cfg_bind_path != NULL && strlen(cfg_bind_path) > 0) {
		qev_listen_unix(cfg_bind_path);
	}

	if (cfg_support_flash) {
		qev_listen("0.0.0.0", 843);
	}

	qev_setnofile(qev_cfg_get_max_clients());

	if (cfg_user != NULL) {
		qev_chuser(cfg_user);
	}

	qev_run();
}
