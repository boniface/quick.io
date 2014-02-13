/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

void qio_main(int argc, char **argv)
{
	#ifdef QIO_DEFAULT_CFG
		qev_cfg_set_default_file(QIO_DEFAULT_CFG);
	#endif

	evs_init();
	qev_init("quickio", argv, argc);
	evs_stats_init();
	sub_init();
	config_init();
	protocols_init();
	client_init();

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

	if (cfg_support_flash) {
		qev_listen("0.0.0.0", 843);
	}

	qev_setnofile(qev_cfg_get_max_clients());

	if (cfg_user != NULL) {
		qev_chuser(cfg_user);
	}

	// @todo stomp address + port?

	qev_run();
}
