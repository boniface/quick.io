#include "qio.h"

// Command line options
static gchar* _config_file = "quickio.ini";

static GOptionEntry command_options[] = {
	{"config-file", 'f', 0, G_OPTION_ARG_STRING, &_config_file, "Configuration file to load", "./quickio.ini"},
	{NULL}
};

// Config file options
static gchar **_apps = NULL;
static gint32 _apps_count = 0;
static gchar *_bind_address = "127.0.0.1";
static gint32 _port = 5000;
static gint32 _max_mess_size = 1024;
static guint64 _max_subs = 4;
static gint32 _processes = 8;
static gint32 _timeout = 5;

static config_file_entry_t _config_options[] = {
	{"apps", e_string_array, &_apps, &_apps_count},
	{"bind-address", e_string, &_bind_address},
	{"port", e_int, &_port},
	{"processes", e_int, &_processes},
	{"max-message-len", e_uint64, &_max_mess_size},
	{"max-subs", e_uint64, &_max_subs},
	{"timeout", e_int, &_timeout},
};

gchar** option_apps() {
	return _apps;
}

gint32 option_apps_count() {
	return _apps_count;
}

gchar* option_bind_address() {
	return _bind_address;
}

gint32 option_port() {
	return _port;
}

guint64 option_max_message_size() {
	return _max_mess_size;
}

guint64 option_max_subscriptions() {
	return _max_subs;
}

gint32 option_processes() {
	return _processes;
}

gint32 option_timeout() {
	return _timeout;
}

gboolean option_parse_config_file(gchar *group_name, config_file_entry_t opts[], size_t opts_len, GError **error) {
	GKeyFile *conf = g_key_file_new();
	
	if (group_name == NULL) {
		group_name = DEFAULT_GROUP_NAME;
		opts = _config_options;
		opts_len = G_N_ELEMENTS(_config_options);
	}
	
	if (!g_key_file_load_from_file(conf, _config_file, 0, error)) {
		ERRORF("Could not load config file: %s", _config_file);
		g_key_file_free(conf);
		return FALSE;
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
	
	g_option_context_free(context);
	
	return success;
}