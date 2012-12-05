/**
 * For accessing options from the configuration file.
 * @file option.h
 */

#pragma once
#include "qio.h"

/**
 * The name of the group that we read from the configuration file.
 */
#define OPT_DEFAULT_GROUP_NAME "quick.io"

/**
 * The group name for the applications configuration
 */
#define OPT_APPS_GROUP_NAME "quick.io-apps"

/**
 * The suffix for app prefix configuration.
 * Yeah, it has a really fun name.
 */
#define OPT_APP_PREFIX_SUFFIX ":prefix"

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
 * Applications have two pieces of information: their path and prefix
 */
typedef struct opt_app_s {
	/**
	 * The name of the configuration section for the app.
	 */
	gchar *config_group;
	
	/**
	 * The path to the application's executable.
	 */
	gchar *path;
	
	/**
	 * The prefix that the app should be under.
	 */
	gchar *prefix;
} opt_app_t;

/**
 * Gets the apps that should run.
 *
 * @return
 * An array of null-terminated strings, containing the names of the apps from the
 * configuration file.
 */
const opt_app_t** option_apps();

/**
 * The number of apps specififed.
 */
const guint16 option_apps_count();

/**
 * Gets the address we're supposed to bind to.
 *
 * @return The address specified in the config file.
 */
const gchar* option_bind_address();

/**
 * Gets the SSL address we're supposed to bind to.
 *
 * @return The address specified in the config file.
 */
const gchar* option_bind_address_ssl();

/**
 * Gets the port that we're supposed to run on.
 *
 * @return The port specified in the config.
 */
const gint option_bind_port();

/**
 * Gets the SSL port that we're supposed to run on.
 *
 * @return The port specified in the config.
 */
const gint option_bind_port_ssl();

/**
 * Gets the config file being used.
 */
const gchar* option_config_file();

/**
 * The path to the log file.
 */
const gchar* option_log_file();

/**
 * The maximum number of clients allowed to be connected.
 */
const guint64 option_max_clients();

/**
 * The maximum length a message may be.
 */
const guint64 option_max_message_size();

/**
 * The number of rooms a client may be subscribed to.
 */
const guint64 option_max_subscriptions();

/**
 * The path to the SSL cert chain. NULL if none provided.
 */
const gchar* option_ssl_cert_chain();

/**
 * The path to the SSL private key. NULL if none provided.
 */
const gchar* option_ssl_private_key();

/**
 * Gets the graphite address
 */
const gchar* option_stats_graphite_address();

/**
 * Gets the graphite port
 */
const gint option_stats_graphite_port();

/**
 * Gets the graphite prefix for the stats
 */
const gchar* option_stats_graphite_prefix();

/**
 * If flash support should be enabled on port 843.
 */
const gint option_support_flash();

/**
 * Gets the number of threads that should be spawned.
 */
const gint option_threads();

/**
 * Gets the client timeout.
 */
const gint option_timeout();

/**
 * Gets which user the server should run as. Returns NULL if 
 * no user switching should be done.
 */
const gchar* option_user();

/**
 * Loads the config file and populates all the options.
 *
 * @ingroup AppFunctions
 *
 * @param group_name The name of the group that should be read and processed.
 * @param opts The options that should be populated.
 * @param opts_len The number of options being passed in.
 * @param[out] error Any GError that might happen parsing the options.
 */
APP_EXPORT gboolean option_parse_config_file(gchar *group_name, config_file_entry_t opts[], size_t opts_len, GError **error);

/**
 * Parses all the command line options.
 *
 * @param argc The number of arguments being passed in
 * @param argv The arguments to parse.
 * @param[out] error Any error that might happen when parsing the command line args. 
 */
gboolean option_parse_args(int argc, char *argv[], GError **error);

#ifdef TESTING
#include "../test/test_option.h"
#endif