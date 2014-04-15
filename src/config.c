/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static void _validate_client_cb_max_age(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	if (val->ui64 == 0) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Setting callback max age to 0 will result in callbacks "
					"being freed on every run. That's a bad idea.");
	}
}

static void _update_client_subs_total(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_valptr curr_val G_GNUC_UNUSED,
	union qev_cfg_val new_val)
{
	client_update_subs_config(
		new_val.ui64,
		cfg_clients_subs_pressure,
		cfg_clients_subs_min);
}

static void _update_client_subs_pressure(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_valptr curr_val G_GNUC_UNUSED,
	union qev_cfg_val new_val)
{
	client_update_subs_config(
		cfg_clients_subs_total,
		new_val.ui64,
		cfg_clients_subs_min);
}

static void _update_client_subs_min(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_valptr curr_val G_GNUC_UNUSED,
	union qev_cfg_val new_val)
{
	client_update_subs_config(
		cfg_clients_subs_total,
		cfg_clients_subs_pressure,
		new_val.ui64);
}

static void _validate_periodic_interval(
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

static void _validate_public_address(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	gint err;
	struct addrinfo *res = NULL;

	if (val->str == NULL) {
		return;
	}

	err = QEV_MOCK(gint, getaddrinfo, val->str, NULL, NULL, &res);
	if (err != 0 || res == NULL) {
		*error = g_error_new(G_OPTION_ERROR, 0,
					"Could not resolve public address %s: %s",
					val->str, gai_strerror(err));
	}

	freeaddrinfo(res);
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
		.validate = qev_cfg_validate_port,
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
		.validate = qev_cfg_validate_port,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "bind-path",
		.description = "Path to create a Unix domain socket at.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_bind_path,
		.defval.str = NULL,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "clients-cb-max-age",
		.description = "How long a callback should be allowed to live on the "
						"server before being killed (measured in seconds).",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_cb_max_age,
		.defval.ui64 = 15,
		.validate = _validate_client_cb_max_age,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "clients-subs-total",
		.description = "Total number of event subscriptions that may exist. "
						"This is used to control memory limits on the server. "
						"As an approximation, every 2^21 subscriptions use 1GB "
						"of RAM.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_subs_total,
		.defval.ui64 = 4194304,
		.validate = NULL,
		.cb = _update_client_subs_total,
		.read_only = FALSE,
	},
	{	.name = "clients-subs-pressure",
		.description = "At what percent of `clients-subs-total` memory pressure "
						"should be applied.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_subs_pressure,
		.defval.ui64 = 60,
		.validate = NULL,
		.cb = _update_client_subs_pressure,
		.read_only = FALSE,
	},
	{	.name = "clients-subs-min",
		.description = "How many subscriptions each client may have, even when "
						"under memory pressure.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_clients_subs_min,
		.defval.ui64 = 128,
		.validate = NULL,
		.cb = _update_client_subs_min,
		.read_only = FALSE,
	},
	{	.name = "periodic-interval",
		.description = "How often periodic tasks should be polled (heartbeats, "
						"callback cleanup, etc). Measured in seconds.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_periodic_interval,
		.defval.ui64 = 10,
		.validate = _validate_periodic_interval,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "periodic-threads",
		.description = "Number of threads used to run periodic tasks.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_periodic_threads,
		.defval.ui64 = 8,
		.validate = NULL,
		.cb = NULL,
		.read_only = FALSE,
	},
	{	.name = "public-address",
		.description = "Where the server lives on the internet.",
		.type = QEV_CFG_STR,
		.val.str = &cfg_public_address,
		.defval.str = NULL,
		.validate = _validate_public_address,
		.cb = NULL,
		.read_only = TRUE,
	},
	{	.name = "run-app-tests",
		.description = "If QuickIO should run app tests instead of running "
						"the server.",
		.type = QEV_CFG_BOOL,
		.val.bool = &cfg_run_app_tests,
		.defval.bool = FALSE,
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
	{	.name = "support-flash",
		.description = "If flash cross domain requests should be serviced "
						"(port 843).",
		.type = QEV_CFG_BOOL,
		.val.bool = &cfg_support_flash,
		.defval.bool = FALSE,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
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
