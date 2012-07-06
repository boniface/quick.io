#pragma once
#include <stdio.h>

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)

#ifdef COMPILE_DEBUG
	#define DEBUG(out) _LOG("DEBUG", out)
	#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#else
	#define DEBUG(out) 
	#define DEBUGF(format, ...) 
#endif

#define WARN(out) _LOG("WARN", out)
#define WARNF(format, ...) _LOGF("WARN", format, __VA_ARGS__)

#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)

/**
 * Handles segmentation faults with nice backtraces.
 */
void debug_handle_signals(void);