#include "option.h"

static gchar* _address = "127.0.0.1";
static gint _port = 5000;
static gint _timeout = 5;

static GOptionEntry options[] = {
	{"address", 'a', 0, G_OPTION_ARG_STRING, &_address, "Address to bind to", "127.0.0.1"},
	{"port", 'p', 0, G_OPTION_ARG_INT, &_port, "Port to run the server on", "5000"},
	{"timeout", 't', 0, G_OPTION_ARG_INT, &_timeout, "Bad client timeout", "5"},
	{NULL}
};

gchar* option_address() {
	return _address;
}

gint option_port() {
	return _port;
}

gint option_timeout() {
	return _timeout;
}

gboolean option_parse_args(int argc, char *argv[], GError *error) {
	gboolean success = TRUE;
	
	GOptionContext *context = g_option_context_new("");
	g_option_context_add_main_entries(context, options, NULL);
	
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		success = FALSE;
	}
	
	g_option_context_free(context);
	
	return success;
}