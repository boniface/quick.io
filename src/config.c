/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static void _validate_client_subs_fairness(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	if (val->ui64 > 100) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Invalid subs fairness: must be between 0 and 100. "
					"%" G_GUINT64_FORMAT " is invalid.", val->ui64);
	}
}

static void _validate_heartbeat_interval(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	if (val->ui64 < 5 || val->ui64 > 60) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Invalid heartbeat time: must be between 5 and 60. "
					"%" G_GUINT64_FORMAT " is invalid.", val->ui64);
	}
}

static void _validate_port(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	if (val->ui64 == 0 || val->ui64 > G_MAXUINT16) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Invalid port number: %" G_GUINT64_FORMAT " doesn't work, must "
					"be between 1 and %d.", val->ui64, G_MAXUINT16);
	}
}

static void _validate_sub_min_size(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	if (val->ui64 == 0 || val->ui64 > qev_cfg_get_max_clients()) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Invalid min sub size: must be greater than 0 and less than "
					"%" G_GUINT64_FORMAT " (the maximum number of clients "
					"allowed to connect).", qev_cfg_get_max_clients());
	}
}

static struct qev_cfg _cfg[] = {
	{	.name = "bind-address",
		.description = "Address to bind to for plaintext connections. "
						"NULL to disable plaintext",
		.type = QEV_CFG_STR,
		.val.str = &cfg_bind_address,
		.defval.str = "0.0.0.0",
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-port",
		.description = "Port to bind to for plaintext connections.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_bind_port,
		.defval.ui64 = 8080,
		.validate = _validate_port,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-address-ssl",
		.description = "Address to bind to for SSL connections. "
						"NULL to disable SSL.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_bind_address_ssl,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-port-ssl",
		.description = "Port to bind to for SSL connections.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_bind_port_ssl,
		.defval.ui64 = 4433,
		.validate = _validate_port,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "broadcast-threads",
		.description = "Number of threads used to pump out each broadcasted message.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_broadcast_threads,
		.defval.ui64 = 2,
		.validate = NULL,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "clients-max-subs",
		.description = "Maximum number of event subscriptions that may exist. "
						"This is used to control memory limits on the server. "
						"As an approximation, every 2^21 subscriptions use 1GB "
						"of RAM.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_max_subs,
		.defval.ui64 = 4194304,
		.validate = NULL,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "clients-subs-fairness",
		.description = "How subscriptions should be distributed to clients. 1 "
						"indicates that subscriptions should be completely "
						"evenly distributed amongst clients "
						"(MAX(1, num_subscriptions / num_clients)), whereas 0 is "
						"first-come, first-serve. A value in the middle balances "
						"the two extremes.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_subs_fairness,
		.defval.ui64 = 80,
		.validate = _validate_client_subs_fairness,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "heartbeat-threads",
		.description = "Number of threads used to pump out heartbeats.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_heartbeat_threads,
		.defval.ui64 = 8,
		.validate = NULL,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "heartbeat-interval",
		.description = "How often the clients should be polled to see if "
						"they need heartbeats.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_heartbeat_interval,
		.defval.ui64 = 10,
		.validate = _validate_heartbeat_interval,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-cert-path-0",
		.description = "The first certificate to use for the SSL connection. "
						"This MUST be a full certificate chain in PEM format, "
						"starting from _your_ certificate on top, all "
						"intermediaries, then the root CA. Typically, this is "
						"an RSA certificate.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_ssl_cert_path_0,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-key-path-0",
		.description = "The key that corresponds to certificate 0.",
		.type = QEV_CFG_STR,
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
		.type = QEV_CFG_STR,
		.val.str = &cfg_ssl_cert_path_1,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "ssl-key-path-1",
		.description = "The key that corresponds to certificate 1.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_ssl_key_path_1,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "sub-min-size",
		.description = "The minimum size for subscription arrays. Should be "
						"a power of 2. Any changes at runtime are only applied "
						"to new subscriptions.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_sub_min_size,
		.defval.ui64 = 8192,
		.validate = _validate_sub_min_size,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "user",
		.description = "Drop all permissions and run as this user.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_user,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
};

void config_init()
{
	qev_cfg_add("quick.io", _cfg, G_N_ELEMENTS(_cfg));
}
