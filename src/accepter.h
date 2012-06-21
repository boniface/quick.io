#pragma once
#include <glib.h>

/**
 * Get ready to accept incoming connections
 */
gboolean accepter_init(void);

/**
 * Clean up the accepter for shutdown
 */
void accepter_finish(void);