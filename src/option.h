#pragma once
#include <glib.h>

#define DEFAULT_GROUP_NAME "csocketio"

typedef enum entry_t {
	e_int,
	e_string,
	e_string_array,
	e_uint64,
} entry_t;

typedef struct ConfigFileEntry {
	gchar *name;
	entry_t arg;
	gpointer arg_data;
	gpointer len;
} ConfigFileEntry;

/**
 * Gets the apps that should run.
 */
gchar** option_apps(void);

/**
 * The number of apps specififed.
 */
int option_apps_count(void);

/**
 * Gets the address we're supposed to bind to.
 */
gchar* option_bind_address(void);

/**
 * Gets the port that we're supposed to run on.
 */
gint option_port(void);

/**
 * Gets the gossip bind address.
 */
gchar* option_gossip_address(void);

/**
 * Gets the gossip bind port.
 */
gint option_gossip_port(void);

/**
 * The number of rooms a client may be subscribed to.
 */
gint option_max_subscriptions(void);

/**
 * Gets the number of processes that should be spawned.
 */
gint option_processes(void);

/**
 * Gets the client timeout.
 */
gint option_timeout(void);

/**
 * Loads the config file and populates all the options
 */
gboolean option_parse_config_file(gchar*, ConfigFileEntry[], size_t, GError**);

/**
 * Parses all the command line options.
 */
gboolean option_parse_args(int, char*[], GError**);