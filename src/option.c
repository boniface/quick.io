#include "debug.h"
#include "option.h"

// Command line options
static gchar* _config_file = "csocket.ini";

static GOptionEntry command_options[] = {
	{"config-file", 'f', 0, G_OPTION_ARG_STRING, &_config_file, "Configuration file to load", "./csocket.ini"},
	{NULL}
};

// Config file options
static gchar** _apps = NULL;
static int _apps_count;
static gchar* _bind_address = "127.0.0.1";
static gint _port = 5000;
static gchar* _gossip_address = "127.0.0.1";
static gint _gossip_port = 43172;
static gint _max_subs = 4;
static gint _processes = 8;
static gint _timeout = 5;

static ConfigFileEntry config_options[] = {
	{"app", G_OPTION_ARG_STRING_ARRAY, &_apps, &_apps_count},
	{"bind-address", G_OPTION_ARG_STRING, &_bind_address},
	{"gossip-address", G_OPTION_ARG_STRING, &_gossip_address},
	{"gossip-port", G_OPTION_ARG_INT, &_gossip_port},
	{"port", G_OPTION_ARG_INT, &_port},
	{"processes", G_OPTION_ARG_INT, &_processes},
	{"max-subs", G_OPTION_ARG_INT, &_max_subs},
	{"timeout", G_OPTION_ARG_INT, &_timeout},
};

gchar** option_apps() {
	return _apps;
}

int option_apps_count() {
	return _apps_count;
}

gchar* option_bind_address() {
	return _bind_address;
}

gint option_port() {
	return _port;
}

gchar* option_gossip_address() {
	return _gossip_address;
}

gint option_gossip_port() {
	return _gossip_port;
}

gint option_max_subscriptions() {
	return _max_subs;
}

gint option_timeout() {
	return _timeout;
}

gint option_processes() {
	return _processes;
}

gboolean option_parse_config_file(gchar *group_name, GError **error) {
	GKeyFile *conf = g_key_file_new();
	
	if (group_name == NULL) {
		group_name = GROUP_NAME;
	}
	
	if (!g_key_file_load_from_file(conf, _config_file, 0, error)) {
		ERRORF("Could not load config file: %s", _config_file);
		g_key_file_free(conf);
		return FALSE;
	}
	
	for (size_t i = 0; i < G_N_ELEMENTS(config_options); i++) {
		ConfigFileEntry e = config_options[i];
		
		if (e.arg == G_OPTION_ARG_STRING) {
			gchar *opt = g_key_file_get_string(conf, group_name, e.name, NULL);
			if (opt != NULL) {
				e.arg_data = opt;
			}
		} else if (e.arg == G_OPTION_ARG_STRING_ARRAY) {
			gchar **opt = g_key_file_get_string_list(conf, group_name, e.name, e.len, error);
			if (opt != NULL) {
				*((gchar***)e.arg_data) = opt;
			}
		} else if (e.arg == G_OPTION_ARG_INT) {
			gint opt = g_key_file_get_integer(conf, group_name, e.name, NULL);
			if (opt != 0) {
				*((gint*)e.arg_data) = opt;
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