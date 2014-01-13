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
	evs_init();
	ASSERT(qev_init(argv, argc), "Could not init quick-event");
	config_init();
	protocols_init();
	client_init();

	if (cfg_bind_address != NULL) {
		guint16 port = cfg_bind_port;
		ASSERT(qev_listen(cfg_bind_address, port),
			"Could not listen on %s:%u", cfg_bind_address, port);
	}

	if (cfg_bind_address_ssl != NULL) {
		guint16 port = cfg_bind_port_ssl;
		ASSERT(qev_listen_ssl(cfg_bind_address_ssl, port,
					cfg_ssl_cert_path_0, cfg_ssl_key_path_0,
					cfg_ssl_cert_path_1, cfg_ssl_key_path_1),
			"Could not listen on %s:%u", cfg_bind_address_ssl, port);
	}

	// @todo stomp address + port?

	ASSERT(qev_run(), "Could not run quick-event");
}
