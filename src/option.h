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
opt_app_t** option_apps();

/**
 * The number of apps specififed.
 */
guint16 option_apps_count();

/**
 * Gets the address we're supposed to bind to.
 *
 * @ingroup AppFunctions
 *
 * @return The address specified in the config file.
 */
APP_EXPORT gchar* option_bind_address();

/**
 * Gets the config file being used.
 */
gchar* option_config_file();

/**
 * The maximum length a message may be.
 */
guint64 option_max_message_size();

/**
 * The number of rooms a client may be subscribed to.
 */
guint64 option_max_subscriptions();

/**
 * Gets the port that we're supposed to run on.
 *
 * @ingroup AppFunctions
 *
 * @return The port specified in the config.
 */
APP_EXPORT gint option_port();

/**
 * Gets the number of processes that should be spawned.
 */
gint option_processes();

/**
 * Gets the stats-flush time.
 */
guint64 option_stats_flush();

/**
 * Gets the graphite address
 */
gchar* option_stats_graphite_address();

/**
 * Gets the graphite port
 */
gint option_stats_graphite_port();

/**
 * Gets the graphite prefix for the stats
 */
gchar* option_stats_graphite_prefix();

/**
 * Gets the client timeout.
 */
gint option_timeout();

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

/**
 * Sets the current process number and updates any internal configuration options that depend
 * on it. 
 *
 * @attention As this modifies global settings, this should be called ONLY from a child process.
 * @note This is NOT idempotent. 
 *
 * @param process The number of the process (0-x, x being the process number)
 */
void option_set_process(guint32 process);

#ifdef TESTING
#include "../test/test_option.h"
#endif