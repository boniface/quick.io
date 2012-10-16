#include "qio.h"

// Command line options
static gchar* _config_file = NULL;

static GOptionEntry command_options[] = {
	{"config-file", 'c', 0, G_OPTION_ARG_STRING, &_config_file, "Configuration file to load; if not specified, will look in the current directory", "./quickio.ini"},
	{NULL}
};

// Config file options
static gchar *_bind_address = "127.0.0.1";
static gint _max_mess_size = 1024;
static guint64 _max_subs = 4;
static gint _port = 5000;
static gint _processes = 8;
static guint64 _stats_flush = 10;
static gchar *_stats_graphite_address = NULL;
static gint _stats_graphite_port = 2003;
static gint _stats_port = 41153;
static gchar *_stats_graphite_prefix = "qio";
static gint _timeout = 5;

static config_file_entry_t _config_options[] = {
	{"bind-address", e_string, &_bind_address},
	{"max-message-len", e_uint64, &_max_mess_size},
	{"max-subs", e_uint64, &_max_subs},
	{"port", e_int, &_port},
	{"processes", e_int, &_processes},
	{"stats-flush", e_uint64, &_stats_flush},
	{"stats-graphite-address", e_string, &_stats_graphite_address},
	{"stats-graphite-port", e_int, &_stats_graphite_port},
	{"stats-graphite-prefix", e_string, &_stats_graphite_prefix},
	{"stats-port", e_int, &_stats_port},
	{"timeout", e_int, &_timeout},
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

opt_app_t** option_apps() {
	return _apps;
}

guint16 option_apps_count() {
	return _apps_count;
}

gchar* option_bind_address() {
	return _bind_address;
}

gchar* option_config_file() {
	return _config_file;
}

guint64 option_max_message_size() {
	return _max_mess_size;
}

guint64 option_max_subscriptions() {
	return _max_subs;
}

gint option_port() {
	return _port;
}

gint option_processes() {
	return _processes;
}

guint64 option_stats_flush() {
	return _stats_flush;
}

gchar* option_stats_graphite_address() {
	return _stats_graphite_address;
}

gint option_stats_graphite_port() {
	return _stats_graphite_port;
}

gchar* option_stats_graphite_prefix() {
	return _stats_graphite_prefix;
}

gint option_stats_port() {
	return _stats_port;
}

gint option_timeout() {
	return _timeout;
}

gboolean option_parse_config_file(gchar *group_name, config_file_entry_t opts[], size_t opts_len, GError **error) {
	GKeyFile *conf = g_key_file_new();
	
	if (!g_key_file_load_from_file(conf, _config_file, 0, error)) {
		ERRORF("Could not load config file: %s", _config_file);
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
			if (opt != NULL) {
				*((gchar**)e.arg_data) = opt;
			}
		} else if (e.arg == e_string_array) {
			gchar **opt = g_key_file_get_string_list(conf, group_name, e.name, e.len, error);
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
		ERRORF("Could not locate configuration file: %s", _config_file);
		success = FALSE;
	} else {
		g_free(_config_file);
		_config_file = g_strdup(path);
	}
	
	g_option_context_free(context);
	
	return success;
}

void option_set_process(guint32 process) {
	_port += process;
}

#ifdef TESTING
#include "../test/test_option.c"
#endif