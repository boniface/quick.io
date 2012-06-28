#include "option.h"

static gchar* _address = "127.0.0.1";
static gint _port = 5000;
static gchar* _gossip_address = "127.0.0.1";
static gint _gossip_port = 43172;
static gint _processes = 8;
static gint _timeout = 5;

static GOptionEntry options[] = {
	{"address", 'a', 0, G_OPTION_ARG_STRING, &_address, "Address to bind to", "127.0.0.1"},
	{"gossip-address", 'g', 0, G_OPTION_ARG_STRING, &_gossip_address, "Address for gossip to bind to", "127.0.0.1"},
	{"gossip-port", 'l', 0, G_OPTION_ARG_INT, &_gossip_port, "Port for gossip to bind to", "43172"},
	{"port", 'p', 0, G_OPTION_ARG_INT, &_port, "Port to run the server on", "5000"},
	{"processes", 'c', 0, G_OPTION_ARG_INT, &_processes, "The number of processes to run", "8"},
	{"timeout", 't', 0, G_OPTION_ARG_INT, &_timeout, "Bad client timeout", "5"},
	{NULL}
};

gchar* option_address() {
	return _address;
}

gint option_port() {
	return _port;
}

gint option_gossip_address() {
	return _gossip_address;
}

gint option_gossip_port() {
	return _gossip_port;
}

gint option_timeout() {
	return _timeout;
}

gint option_processes() {
	return _processes;
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