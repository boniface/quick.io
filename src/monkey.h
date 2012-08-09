/**
 * Contains some extra utility functions added to libraries for quicker access.
 * @file monkey.h
 */

/**
 * Determines if the string ends with suffix.
 *
 * @param gstring The string to check
 * @param suffix The suffix to check string for
 *
 * @return If the string ends with the suffix.
 */
#define g_string_ends_with(gstring, suffix) \
	strncmp(gstring->str + gstring->len - (sizeof(suffix) - 1), suffix, sizeof(suffix)-1) == 0
