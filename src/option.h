#pragma once
#include <glib.h>

/**
 * Gets the address we're supposed to bind to.
 */
gchar* option_address(void);

/**
 * Gets the port that we're supposed to run on.
 */
gint option_port(void);

/**
 * Gets the client timeout.
 */
gint option_timeout(void);

/**
 * Parses all the command line options.
 */
gboolean option_parse_args(int, char*[], GError*);