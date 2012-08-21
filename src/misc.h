/**
 * A set of misc. tools to allow simpler code.  These also help with code
 * coverage by not injecting IFs into the code everywhere, but the overall
 * effect is the same.
 *
 * These functions exist ONLY to check error conditions and return without
 * bumping up the number of uncovered lines reported.
 *
 * @attention If you are going to use these in place of proper branches,
 * you're an idiot.  Put down the text editor.
 *
 * @file misc.h
 */

#pragma once

/**
 * Log an error message and return a value if the expression is false.
 */
#define qio_ret_val_if(expr, val, message) \
	if (expr) { \
		ERROR(message); \
		return val; \
	}

/**
 * Log a formatted error message and return a value if the expression is false.
 */
#define qio_ret_val_if_f(expr, val, message, ...) \
	if (expr) { \
		ERRORF(message, __VA_ARGS__); \
		return val; \
	}

/**
 * Perform an operation if the expression is false, and omit an error.
 */
#define qio_error_and_do_if(expr, message, stmt) \
	if (expr) { \
		ERROR(message); \
		stmt \
	}
