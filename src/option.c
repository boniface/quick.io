#include <wordexp.h>
#include "qio.h"

/**
 * Absolute paths to all of the known configuration files
 */
static GPtrArray *_config_files = NULL;

static gchar **_config_filenames = NULL;
static gchar *_config_dir = NULL;

static GOptionEntry command_options[] = {
	{"config-file", 'c', 0, G_OPTION_ARG_FILENAME_ARRAY, &_config_filenames,
		"Configuration file to load; if not specified, will look in the current directory. "
			"This option may be given multiple times to load multiple files.",
		"./quickio.ini"},
	{"config-dir", 'd', 0, G_OPTION_ARG_FILENAME, &_config_dir,
		"Configuration directory to scan for other config files",
		"/etc/quickio"},
	{NULL}
};

static struct opt_app **_apps = NULL;
static guint _apps_count = 0;
static gchar *_bind_address = NULL;
static gchar *_bind_address_ssl = NULL;
static gint _bind_port = 80;
static gint _bind_port_ssl = 443;
static gchar *_hostname = NULL;
static gchar *_log_file = NULL;
static guint64 _max_clients = 4194304;
static gint _max_mess_size = 1024;
static guint64 _max_subs = 4;
static gboolean _run_app_test = FALSE;
static gchar *_ssl_private_key = NULL;
static gchar *_ssl_cert_chain = NULL;
static gchar *_stats_graphite_address = NULL;
static gint _stats_graphite_port = 2003;
static gchar *_stats_graphite_prefix = "qio";
static gboolean _support_flash = FALSE;
static gint _threads = 2;
static gint _timeout = 5;
static gchar *_user = NULL;

static struct config_file_entry _config_options[] = {
	{"bind-address", e_string, &_bind_address},
	{"bind-address-ssl", e_string, &_bind_address_ssl},
	{"bind-port", e_int, &_bind_port},
	{"bind-port-ssl", e_int, &_bind_port_ssl},
	{"hostname", e_string, &_hostname},
	{"log-file", e_string, &_log_file},
	{"max-clients", e_uint64, &_max_clients},
	{"max-message-len", e_uint64, &_max_mess_size},
	{"max-subs", e_uint64, &_max_subs},
	{"run-app-test", e_boolean, &_run_app_test},
	{"ssl-private-key", e_string, &_ssl_private_key},
	{"ssl-cert-chain", e_string, &_ssl_cert_chain},
	{"stats-graphite-address", e_string, &_stats_graphite_address},
	{"stats-graphite-port", e_int, &_stats_graphite_port},
	{"stats-graphite-prefix", e_string, &_stats_graphite_prefix},
	{"support-flash", e_boolean, &_support_flash},
	{"threads", e_int, &_threads},
	{"timeout", e_int, &_timeout},
	{"user", e_string, &_user},
};

const struct opt_app** option_apps()
{
	return (const struct opt_app**)_apps;
}

const guint16 option_apps_count()
{
	return _apps_count;
}

const gchar* option_bind_address()
{
	return _bind_address;
}

const gchar* option_bind_address_ssl()
{
	return _bind_address_ssl;
}

const gint option_bind_port()
{
	return _bind_port;
}

const gint option_bind_port_ssl()
{
	return _bind_port_ssl;
}

const gchar* option_hostname()
{
	return _hostname;
}

const gchar* option_log_file()
{
	return _log_file;
}

const guint64 option_max_clients()
{
	return _max_clients;
}

const guint64 option_max_message_size()
{
	return _max_mess_size;
}

const guint64 option_max_subscriptions()
{
	return _max_subs;
}

const gboolean option_run_app_test()
{
	return _run_app_test;
}

const gchar* option_ssl_cert_chain()
{
	return _ssl_cert_chain;
}

const gchar* option_ssl_private_key()
{
	return _ssl_private_key;
}

const gchar* option_stats_graphite_address()
{
	return _stats_graphite_address;
}

const gint option_stats_graphite_port()
{
	return _stats_graphite_port;
}

const gchar* option_stats_graphite_prefix()
{
	return _stats_graphite_prefix;
}

const gint option_support_flash()
{
	return _support_flash;
}

const gint option_threads()
{
	return _threads;
}

const gint option_timeout()
{
	return _timeout;
}

const gchar* option_user()
{
	return _user;
}

static gboolean _option_parse_apps(GKeyFile *conf)
{
	gsize key_len = 0;
	gchar **keys = g_key_file_get_keys(conf, OPT_APPS_GROUP_NAME, &key_len, NULL);

	if (keys == NULL) {
		return FALSE;
	}

	if (_apps != NULL) {
		for (guint i = 0; i < _apps_count; i++) {
			g_free(*(_apps + i));
		}

		g_free(_apps);
		_apps = NULL;
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

			struct opt_app *app = g_malloc0(sizeof(*app));
			app->config_group = g_strdup(key);
			app->path = path;
			app->prefix = prefix;

			apps = g_list_prepend(apps, app);
		}
	}

	if (_apps_count > 0) {
		apps = g_list_first(apps);

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

void option_setup_config(struct config_file_entry opts[], gsize opts_len)
{
	for (gsize i = 0; i < opts_len; i++) {
		struct config_file_entry e = opts[i];
		if (e.arg == e_string) {
			gchar **val = e.arg_data;
			*val = g_strdup(*val);
		} else if (e.arg == e_stringv) {
			gchar ***val = e.arg_data;
			*val = g_strdupv(*val);
		}
	}
}

gboolean option_parse_config_file(
	gchar *group_name,
	struct config_file_entry opts[],
	gsize opts_len)
{
	if (_config_files == NULL || _config_files->len == 0) {
		CRITICAL("Attempting to configure server without any configuration files.");
		return FALSE;
	}

	static gboolean opts_setup = FALSE;
	if (!opts_setup) {
		opts_setup = TRUE;
		option_setup_config(_config_options, G_N_ELEMENTS(_config_options));
	}

	GKeyFile *conf;
	gboolean good = TRUE;
	gboolean apps_parsed = FALSE;
	gboolean saw_group = FALSE;

	for (guint i = 0; i < _config_files->len; i++) {
		GError *error = NULL;
		gchar *config_file = g_ptr_array_index(_config_files, i);
		conf = g_key_file_new();

		if (!g_key_file_load_from_file(conf, config_file, 0, &error)) {
			CRITICAL("Could not load config file \"%s\": %s", config_file, error->message);
			g_error_free(error);
			good = FALSE;
			goto done;
		}

		// If parsing the defaults, then also parse up the app configuration
		if (group_name == NULL) {
			group_name = OPT_DEFAULT_GROUP_NAME;
			opts = _config_options;
			opts_len = G_N_ELEMENTS(_config_options);

			apps_parsed |= _option_parse_apps(conf);
		} else {
			apps_parsed = TRUE;
		}

		if (g_key_file_has_group(conf, group_name)) {
			saw_group = TRUE;
		}

		for (gsize i = 0; i < opts_len; i++) {
			GError *err = NULL;
			struct config_file_entry e = opts[i];

			if (e.arg == e_string) {
				gchar *opt = g_key_file_get_string(conf, group_name, e.name, &err);
				gchar **val = e.arg_data;
				if (err == NULL && strlen(opt) > 0) {
					if (*val != NULL) {
						g_free(*val);
					}
					*val = opt;
				}
			} else if (e.arg == e_stringv) {
				gchar **opt = g_key_file_get_string_list(conf, group_name, e.name, e.len, &err);
				gchar*** val = e.arg_data;
				if (err == NULL) {
					if (*val != NULL) {
						g_strfreev(*val);
					}
					*val = opt;
				}
			} else if (e.arg == e_int) {
				gint opt = g_key_file_get_integer(conf, group_name, e.name, &err);
				if (err == NULL) {
					*((gint*)e.arg_data) = opt;
				}
			} else if (e.arg == e_uint64) {
				gint opt = g_key_file_get_uint64(conf, group_name, e.name, &err);
				if (err == NULL) {
					*((guint64*)e.arg_data) = opt;
				}
			} else if (e.arg == e_boolean) {
				gboolean opt = g_key_file_get_boolean(conf, group_name, e.name, &err);
				if (err == NULL) {
					*((gboolean*)e.arg_data) = opt;
				}
			}

			if (err != NULL) {
				g_error_free(err);
			}
		}

		g_key_file_free(conf);
		conf = NULL;
	}

	if (!saw_group) {
		WARN("Could not find configuration section for app named \"%s\".", group_name);
	}

	if (!apps_parsed) {
		CRITICAL("Section [quick.io-apps] was not found in any config file.");
		good = FALSE;
		goto done;
	}

	// Determine if the number is a power of 2
	// The bitmask of any power of 2 is, for example: 0b01000000
	// Subtracting 1 from this number will make it so that it is 0 with an &
	// Conversely: 01100000 - 1 = 1011111; 01100000 & 1011111 = 1000000
	// This is the same check used in malloc.c
	if ((_max_subs & (_max_subs - 1)) != 0) {
		CRITICAL("Option `max-subs` must be a power of 2.");
		good = FALSE;
		goto done;
	}

	// We only have a signed 8 bit number, so the timeout may not be too large
	if (_timeout < 0 || _timeout > 127) {
		CRITICAL("Option `timeout` must be greater than 0 and less than 127.");
		good = FALSE;
		goto done;
	}

	if (_hostname == NULL) {
		gchar hostname[1024];
		memset(&hostname, 0, sizeof(hostname));

		if (gethostname(hostname, sizeof(hostname)-1) == -1) {
			FATAL("Could not determine hostname for server: %s", strerror(errno));
		}

		_hostname = g_strdup(hostname);
	}

done:
	if (conf != NULL) {
		g_key_file_free(conf);
	}

	return good;
}

gboolean option_parse_args(int argc, gchar *argv[], GError **error)
{
	gboolean good = TRUE;

	gboolean _add_path(gchar *file)
	{
		gchar rpath[PATH_MAX+1];
		memset(&rpath, 0, sizeof(rpath));
		if (realpath(file, rpath) == NULL) {
			return FALSE;
		}

		// Don't allow duplicates in the list
		for (guint i = 0; i < _config_files->len; i++) {
			if (g_strcmp0(g_ptr_array_index(_config_files, i), rpath) == 0) {
				return TRUE;
			}
		}

		g_ptr_array_add(_config_files, g_strdup(rpath));
		return TRUE;
	}

	if (_config_files == NULL) {
		_config_files = g_ptr_array_new_with_free_func(g_free);
	} else {
		g_ptr_array_set_size(_config_files, 0);
	}

	GOptionContext *context = g_option_context_new("");
	g_option_context_add_main_entries(context, command_options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, error)) {
		good = FALSE;
		goto done;
	}

	// Default: look for the config in the current dir
	if (_config_filenames == NULL) {
		_config_filenames = g_strdupv((gchar*[]){
			"quickio.ini",
			NULL,
		});
	}

	gchar **f = _config_filenames;
	while (*f != NULL) {
		if (!_add_path(*f)) {
			CRITICAL("Could not locate configuration file: %s", *f);
			good = FALSE;
			goto done;
		}

		f++;
	}

	if (_config_dir != NULL) {
		wordexp_t p;
		gchar buff[PATH_MAX];

		snprintf(buff, sizeof(buff), "%s/*.ini", _config_dir);
		if (wordexp(buff, &p, 0) == 0) {
			for (guint i = 0; i < p.we_wordc; i++) {
				gchar *path = p.we_wordv[i];

				// No real need for error checking here: the system told us the file
				// exists; barring any malicious deleting of config files, we'll be fine
				_add_path(path);
			}
		}

		wordfree(&p);
		g_free(_config_dir);
		_config_dir = NULL;
	}

done:
	g_option_context_free(context);

	if (_config_filenames != NULL) {
		g_strfreev(_config_filenames);
		_config_filenames = NULL;
	}

	if (_config_dir != NULL) {
		g_free(_config_dir);
		_config_dir = NULL;
	}

	return good;
}

#ifdef TESTING
#include "../test/test_option.c"
#endif
