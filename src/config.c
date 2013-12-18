/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static void _validate_port(
	enum qev_cfg_type type G_GNUC_UNUSED,
	union qev_cfg_val val,
	GError **error)
{
	if (val.ui64 == 0 || val.ui64 > G_MAXUINT16) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Invalid port number: %"G_GUINT64_FORMAT" is too large, must "
					"be between 1 and %d.", val.ui64, G_MAXUINT16);
	}
}

static struct qev_cfg _cfg[] = {
	{	.name = "bind-address",
		.description = "Address to bind to for plaintext connections. "
						"NULL to disable plaintext",
		.type = qev_cfg_str,
		.val.str = &cfg_bind_address,
		.defval.str = "0.0.0.0",
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-port",
		.description = "Port to bind to for plaintext connections.",
		.type = qev_cfg_uint64,
		.val.ui64 = &cfg_bind_port,
		.defval.ui64 = 8080,
		.validate = _validate_port,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-address-ssl",
		.description = "Address to bind to for SSL connections. "
						"NULL to disable SSL.",
		.type = qev_cfg_str,
		.val.str = &cfg_bind_address_ssl,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-port-ssl",
		.description = "Port to bind to for SSL connections.",
		.type = qev_cfg_uint64,
		.val.ui64 = &cfg_bind_port_ssl,
		.defval.ui64 = 4433,
		.validate = _validate_port,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "max-subscriptions",
		.description = "Maximum number of subscriptions a client may have.",
		.type = qev_cfg_uint64,
		.val.ui64 = &cfg_max_subscriptions,
		.defval.ui64 = 255,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-cert-path-0",
		.description = "The first certificate to use for the SSL connection. "
						"This MUST be a full certificate chain in PEM format, "
						"starting from _your_ certificate on top, all "
						"intermediaries, then the root CA. Typically, this is "
						"an RSA certificate.",
		.type = qev_cfg_str,
		.val.str = &cfg_ssl_cert_path_0,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-key-path-0",
		.description = "The key that corresponds to certificate 0.",
		.type = qev_cfg_str,
		.val.str = &cfg_ssl_key_path_0,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-cert-path-1",
		.description = "The second certificate to use for the SSL connection. "
						"This MUST be a full certificate chain in PEM format, "
						"starting from _your_ certificate on top, all "
						"intermediaries, then the root CA. Typically, this is "
						"an ECDSA certificate.",
		.type = qev_cfg_str,
		.val.str = &cfg_ssl_cert_path_1,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-key-path-1",
		.description = "The key that corresponds to certificate 1.",
		.type = qev_cfg_str,
		.val.str = &cfg_ssl_key_path_1,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
};

void config_init()
{
	qev_cfg_add("quickio", _cfg, G_N_ELEMENTS(_cfg));
}
