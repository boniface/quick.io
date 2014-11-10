/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "quickio.h"

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

static void _validate_public_address(
	const gchar *name G_GNUC_UNUSED,
	union qev_cfg_val *val,
	GError **error)
{
	gint err;
	struct addrinfo *res = NULL;

	if (val->str == NULL || strlen(val->str) == 0) {
		g_free(val->str);
		val->str = NULL;
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

static struct qev_cfg _overrides[] = {
	{	.name = "listen",
		.type = QEV_CFG_STRV,
		.defval.strv = (gchar*[]){
			"[::]:8080",
		},
	},
};

static struct qev_cfg _cfg[] = {
	{	.name = "broadcast-threads",
		.description = "Number of threads used to pump out each broadcasted message.",
		.type = QEV_CFG_UINT64,
		.val.ui64 = &cfg_broadcast_threads,
		.defval.ui64 = 2,
		.validate = NULL,
		.cb = NULL,
		.read_only = FALSE,
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
};

void config_init()
{
	qev_cfg_override_defaults(_overrides, G_N_ELEMENTS(_overrides));
	qev_cfg_add("quick.io", _cfg, G_N_ELEMENTS(_cfg));
}
