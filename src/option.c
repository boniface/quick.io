#include "qio.h"

// Command line options
static gchar* _config_file = NULL;

static GOptionEntry command_options[] = {
	{"config-file", 'c', 0, G_OPTION_ARG_STRING, &_config_file, "Configuration file to load; if not specified, will look in the current directory", "./quickio.ini"},
	{NULL}
};

// Config file options
static gchar *_bind_address = NULL;
static gchar *_bind_address_ssl = NULL;
static gint _bind_port = 80;
static gint _bind_port_ssl = 443;
static gchar *_log_file = NULL;
static gint _max_mess_size = 1024;
static guint64 _max_subs = 4;
static gchar *_ssl_private_key = NULL;
static gchar *_ssl_cert_chain = NULL;
static gchar *_stats_graphite_address = NULL;
static gint _stats_graphite_port = 2003;
static gchar *_stats_graphite_prefix = "qio";
static gint _support_flash = 0;
static gint _threads = 2;
static gint _timeout = 5;
static gchar *_user = NULL;

static config_file_entry_t _config_options[] = {
	{"bind-address", e_string, &_bind_address},
	{"bind-address-ssl", e_string, &_bind_address_ssl},
	{"bind-port", e_int, &_bind_port},
	{"bind-port-ssl", e_int, &_bind_port_ssl},
	{"log-file", e_string, &_log_file},
	{"max-message-len", e_uint64, &_max_mess_size},
	{"max-subs", e_uint64, &_max_subs},
	{"ssl-private-key", e_string, &_ssl_private_key},
	{"ssl-cert-chain", e_string, &_ssl_cert_chain},
	{"stats-graphite-address", e_string, &_stats_graphite_address},
	{"stats-graphite-port", e_int, &_stats_graphite_port},
	{"stats-graphite-prefix", e_string, &_stats_graphite_prefix},
	{"support-flash", e_int, &_support_flash},
	{"threads", e_int, &_threads},
	{"timeout", e_int, &_timeout},
	{"user", e_string, &_user},
};

// The processed list of apps and their prefixes
static opt_app_t **_apps = NULL;
static guint16 _apps_count = 0;

static gboolean _option_parse_apps(GKeyFile *conf, GError **error) {
	gsize key_len = 0;
	gchar **keys = g_key_file_get_keys(conf, OPT_APPS_GROUP_NAME, &key_len, error);
	
	if (keys == NULL) {
		return FALSE;
	}
	
	// Allow this function to be called multiple times
	// Mainly for testing
	_apps_count = 0;
	
	GList *apps = NULL;
	while (key_len--) {
		gchar *key = *(keys + key_len);
		if (g_strstr_len(key, -1, ":") == NULL) {
			gchar *path = g_key_file_get_string(conf, OPT_APPS_GROUP_NAME, key, NULL);
			
			// There ABSOLUTELY DOES NOT have to be a prefix for an application,
			// so it's fine to use the NULL value to denote no prefix
			gchar key_prefix[strlen(key) + sizeof(OPT_APP_PREFIX_SUFFIX)];
			g_snprintf(key_prefix, sizeof(key_prefix), "%s"OPT_APP_PREFIX_SUFFIX, key);
			gchar *prefix = g_key_file_get_string(conf, OPT_APPS_GROUP_NAME, key_prefix, NULL);
			
			_apps_count++;
			
			opt_app_t *app = g_malloc0(sizeof(*app));
			app->config_group = g_strdup(key);
			app->path = path;
			app->prefix = prefix;
			
			apps = g_list_prepend(apps, app);
		}
	}
	
	if (_apps_count > 0) {
		apps = g_list_first(apps);
		
		// Plop all the apps into an array
		gsize i = 0;
		_apps = g_malloc0(_apps_count * sizeof(*_apps));
		
		// This is kinda disgusting...
		do {
			*(_apps + i++) = apps->data;
		} while ((apps = g_list_next(apps)) != NULL);
	}
	
	// Man, this is just gross
	g_list_free(g_list_first(apps));
	g_strfreev(keys);
	
	return TRUE;
}

const opt_app_t** option_apps() {
	return (const opt_app_t**)_apps;
}

const guint16 option_apps_count() {
	return _apps_count;
}

const gchar* option_bind_address() {
	return _bind_address;
}

const gchar* option_bind_address_ssl() {
	return _bind_address_ssl;
}

const gint option_bind_port() {
	return _bind_port;
}

const gint option_bind_port_ssl() {
	return _bind_port_ssl;
}

const gchar* option_config_file() {
	return _config_file;
}

const gchar* option_log_file() {
	return _log_file;
}

const guint64 option_max_message_size() {
	return _max_mess_size;
}

const guint64 option_max_subscriptions() {
	return _max_subs;
}

const gchar* option_ssl_cert_chain() {
	return _ssl_cert_chain;
}

const gchar* option_ssl_private_key() {
	return _ssl_private_key;
}

const gchar* option_stats_graphite_address() {
	return _stats_graphite_address;
}

const gint option_stats_graphite_port() {
	return _stats_graphite_port;
}

const gchar* option_stats_graphite_prefix() {
	return _stats_graphite_prefix;
}

const gint option_support_flash() {
	return _support_flash;
}

const gint option_threads() {
	return _threads;
}

const gint option_timeout() {
	return _timeout;
}

const gchar* option_user() {
	return _user;
}

gboolean option_parse_config_file(gchar *group_name, config_file_entry_t opts[], size_t opts_len, GError **error) {
	GKeyFile *conf = g_key_file_new();
	
	if (!g_key_file_load_from_file(conf, _config_file, 0, error)) {
		ERROR("Could not load config file: %s", _config_file);
		g_key_file_free(conf);
		return FALSE;
	}
	
	// If parsing the defaults, then also parse up the app configuration
	if (group_name == NULL) {
		group_name = OPT_DEFAULT_GROUP_NAME;
		opts = _config_options;
		opts_len = G_N_ELEMENTS(_config_options);
		
		if (!_option_parse_apps(conf, error)) {
			return FALSE;
		}
	}
	
	for (size_t i = 0; i < opts_len; i++) {
		config_file_entry_t e = opts[i];
		
		if (e.arg == e_string) {
			gchar *opt = g_key_file_get_string(conf, group_name, e.name, NULL);
			if (opt != NULL && strlen(opt) > 0) {
				*((gchar**)e.arg_data) = opt;
			}
		} else if (e.arg == e_string_array) {
			gchar **opt = g_key_file_get_string_list(conf, group_name, e.name, e.len, NULL);
			if (opt != NULL) {
				*((gchar***)e.arg_data) = opt;
			}
		} else if (e.arg == e_int) {
			gint opt = g_key_file_get_integer(conf, group_name, e.name, NULL);
			if (opt != 0) {
				*((gint*)e.arg_data) = opt;
			}
		} else if (e.arg == e_uint64) {
			gint opt = g_key_file_get_uint64(conf, group_name, e.name, NULL);
			if (opt != 0) {
				*((guint64*)e.arg_data) = opt;
			}
		}
	}
	
	// Determine if the number is a power of 2
	// The bitmask of any power of 2 is, for example: 0b01000000
	// Subtracting 1 from this number will make it so that it is 0 with an &
	// Conversely: 01100000 - 1 = 1011111; 01100000 & 1011111 = 1000000
	// This is the same check used in malloc.c
	if ((_max_subs & (_max_subs - 1)) != 0) {
		ERROR("Option `max-subs` must be a power of 2.");
		g_key_file_free(conf);
		return FALSE;
	}
	
	g_key_file_free(conf);
	return TRUE;
}

gboolean option_parse_args(int argc, char *argv[], GError **error) {
	gboolean success = TRUE;
	
	GOptionContext *context = g_option_context_new("");
	g_option_context_add_main_entries(context, command_options, NULL);
	
	if (!g_option_context_parse(context, &argc, &argv, error)) {
		success = FALSE;
	}
	
	// So that there doesn't have to be any crazy free() logic
	if (_config_file == NULL) {
		_config_file = g_strdup("quickio.ini");
	}
	
	// Resolve the absolute path to the configuration file
	char path[PATH_MAX+1];
	memset(&path, 0, sizeof(path));
	if (realpath(_config_file, path) == NULL) {
		ERROR("Could not locate configuration file: %s", _config_file);
		success = FALSE;
	} else {
		g_free(_config_file);
		_config_file = g_strdup(path);
	}
	
	g_option_context_free(context);
	
	return success;
}

#ifdef TESTING
#include "../test/test_option.c"
#endif