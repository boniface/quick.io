/**
 * For accessing options from the configuration file.
 * @file option.h
 */
#ifndef QIO_OPTION_H
#define QIO_OPTION_H

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
enum opt_entry {
	/**
	 * The entry is a 32bit integer.
	 */
	e_int,

	/**
	 * The entry is a string.
	 */
	e_string,

	/**
	 * The entry is a NULL-terminated vector of strings (ie. {"one", "two", NULL})
	 */
	e_stringv,

	/**
	 * The entry is an unsigned 64bit integer.
	 */
	e_uint64,

	/**
	 * The entry is a boolean
	 */
	e_boolean,
};

/**
 * The struct for the config file directive definitions.
 */
struct config_file_entry {
	/**
	 * The name of the option in the configuration file.
	 */
	gchar *name;

	/**
	 * The type of the option in the configuration file.
	 */
	enum opt_entry arg;

	/**
	 * Where the value from the configration file should be stored.
	 */
	gpointer arg_data;

	/**
	 * If an array, the number of items read from the configuration file.
	 */
	gpointer len;
};

/**
 * Applications have two pieces of information: their path and prefix
 */
struct opt_app {
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
};

/**
 * Gets the apps that should run.
 *
 * @return
 * An array of null-terminated strings, containing the names of the apps from the
 * configuration file.
 */
const struct opt_app** option_apps();

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
 * Gets the hostname of the running instance.
 *
 * @ingroup AppFunctions
 */
APP_EXPORT const gchar* option_hostname();

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
 * If the server should only run the app's unit tests and exit.
 */
const gboolean option_run_app_test();

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
 * If you are ever going to use any static strings in your default config, you MUST
 * call this function before calling option_parse_config_file(). Because options can
 * be overriden all over the place, it is necessary to move all options into the data
 * segment so that they can be free'd at will.
 *
 * @param opts The options to make safe to free.
 * @param opts_len The number of options being passed in.
 */
APP_EXPORT void option_setup_config(struct config_file_entry opts[], gsize opts_len);

/**
 * Loads the config file and populates all the options.
 *
 * @ingroup AppFunctions
 *
 * @param group_name The name of the group that should be read and processed.
 * @param opts The options that should be populated.
 * @param opts_len The number of options being passed in.
 */
APP_EXPORT gboolean option_parse_config_file(
	gchar *group_name,
	struct config_file_entry opts[],
	gsize opts_len);

/**
 * Parses all the command line options.
 *
 * @param argc The number of arguments being passed in
 * @param argv The arguments to parse.
 * @param[out] error Any error that might happen when parsing the command line args.
 */
gboolean option_parse_args(int argc, gchar *argv[], GError **error);

#ifdef TESTING
#include "../test/test_option.h"
#endif

#endif
