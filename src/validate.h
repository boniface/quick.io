/**
 * Various validation functions used around.
 * @file validate.h
 */

#pragma once
#include "qio.h"

/**
 * Determines if an event name is valid.
 *
 * @param event The name of the event to validate.
 *
 * @return If the event name is valid.
 */
gboolean validate_event(gchar *event);