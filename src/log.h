/**
 * Various, useful macros for printing out debug information and handling segfaults while debugging.
 * @file debug.h
 */
#pragma once
#include <stdio.h>
#include "qio.h"

#if defined(COMPILE_DEBUG) || defined(COMPILE_PROFILE) || defined(COMPILE_PROFILE)
	#define DEBUG(format, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "DEBUG : %s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
	/**
	 * Output debug info to the console, only when not built for prod.
	 */
	#define DEBUG(format, ...)
#endif

/**
 * Output warning to the console.
 */
#define WARN(format, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "WARN : %s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Output error to the console.
 */
#define ERROR(format, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "ERROR : %s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * A fatal error, after which nothing can continue.
 */
#define FATAL(format, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "FATAL : %s:%d : " format, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * Sets up all the log handlers and such.
 */
gboolean log_init(void);