/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
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
		guint16 port = cfg_bind_port;
		qev_listen(cfg_bind_address, port);
	}

	if (cfg_bind_address_ssl != NULL) {
		guint16 port = cfg_bind_port_ssl;
		qev_listen_ssl(cfg_bind_address_ssl, port,
					cfg_ssl_cert_path_0, cfg_ssl_key_path_0,
					cfg_ssl_cert_path_1, cfg_ssl_key_path_1);
	}

	if (cfg_bind_path != NULL && strlen(cfg_bind_path) > 0) {
		qev_listen_unix(cfg_bind_path);
	}

	if (cfg_support_flash) {
		qev_listen("0.0.0.0", 843);
	}

	if (cfg_user != NULL) {
		qev_chuser(cfg_user);
	}

	qev_run();
}
