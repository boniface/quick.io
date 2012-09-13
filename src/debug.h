/**
 * Various, useful macros for printing out debug information and handling segfaults while debugging.
 * @file debug.h
 */
#pragma once
#include <stdio.h>
#include "qio.h"

/**
 * A generic log, to stderr.
 */
#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)

/**
 * A generic, formatted log, to stderr.
 */
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#ifdef COMPILE_DEBUG
	#define DEBUG(out) _LOG("DEBUG", out)
	#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#else
	/**
	 * Output debug info to the console, only when not built for prod.
	 */
	#define DEBUG(out) 
	
	/**
	 * Output formatted debug info to the console, only when not built for prod.
	 */
	#define DEBUGF(format, ...) 
#endif

/**
 * Output warning to the console.
 */
#define WARN(out) _LOG("WARN", out)

/**
 * Output formatted warning to the console.
 */
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)

/**
 * Output error to the console.
 */
#define ERROR(out) _LOG("ERROR", out)

/**
 * Output formatted error to the console.
 */
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

/**
 * Handles segmentation faults with nice backtraces.
 */
void debug_handle_signals(void);