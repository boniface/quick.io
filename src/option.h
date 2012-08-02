/**
 * For accessing options from the configuration file.
 * @file option.h
 */

#pragma once
#include "qio.h"

/**
 * The name of the group that we read from the configuration file.
 */
#define DEFAULT_GROUP_NAME "quick.io"

/**
 * The types of entries that are in the configuration file.
 */
typedef enum entry_t {
	/**
	 * The entry is a 32bit integer.
	 */
	e_int,
	
	/**
	 * The entry is a string.
	 */
	e_string,
	
	/**
	 * The entry is an array of string.
	 */
	e_string_array,
	
	/**
	 * The entry is an unsigned 64bit integer.
	 */
	e_uint64,
} entry_t;

/**
 * The struct for the config file directive definitions.
 */
typedef struct config_file_entry_s {
	/**
	 * The name of the option in the configuration file.
	 */
	gchar *name;
	
	/**
	 * The type of the option in the configuration file.
	 */
	entry_t arg;
	
	/**
	 * Where the value from the configration file should be stored.
	 */
	gpointer arg_data;
	
	/**
	 * If an array, the number of items read from the configuration file.
	 */
	gpointer len;
} config_file_entry_t;

/**
 * Gets the apps that should run.
 *
 * @return
 * An array of null-terminated strings, containing the names of the apps from the
 * configuration file.
 */
gchar** option_apps();

/**
 * The number of apps specififed.
 */
gint32 option_apps_count();

/**
 * Gets the apps that should run.
 *
 * @return
 * An array of null-terminated strings, containing the event prefixes the apps should have (returned in the same order as option_apps()).
 */
gchar** option_apps_prefixes();

/**
 * The number of apps specififed.
 */
gint32 option_apps_prefixes_count();

/**
 * Gets the address we're supposed to bind to.
 */
gchar* option_bind_address();

/**
 * Gets the port that we're supposed to run on.
 */
gint32 option_port();

/**
 * Gets the gossip bind address.
 */
gchar* option_gossip_address();

/**
 * Gets the gossip bind port.
 */
gint32 option_gossip_port();

/**
 * The maximum length a message may be.
 */
guint64 option_max_message_size();

/**
 * The number of rooms a client may be subscribed to.
 */
guint64 option_max_subscriptions();

/**
 * Gets the number of processes that should be spawned.
 */
gint32 option_processes();

/**
 * Gets the client timeout.
 */
gint32 option_timeout();

/**
 * Loads the config file and populates all the options.
 *
 * @ingroup ModuleFunctions
 *
 * @param group_name The name of the group that should be read and processed.
 * @param opts The options that should be populated.
 * @param opts_len The number of options being passed in.
 * @param[out] error Any GError that might happen parsing the options.
 */
MODULE_EXPORT gboolean option_parse_config_file(gchar *group_name, config_file_entry_t opts[], size_t opts_len, GError **error);

/**
 * Parses all the command line options.
 *
 * @param argc The number of arguments being passed in
 * @param argv The arguments to parse.
 * @param[out] error Any error that might happen when parsing the command line args. 
 */
gboolean option_parse_args(int argc, char *argv[], GError **error);