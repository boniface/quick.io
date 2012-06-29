#include <stdio.h>

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define _LOGF(level, format, ...) fprintf(stderr, level " : %s:%d : " format "\n", __FILE__, __LINE__, __VA_ARGS__)
#define DEBUG(out) _LOG("DEBUG", out)
#define DEBUGF(format, ...) _LOGF("DEBUG", format, __VA_ARGS__)
#define ERROR(out) _LOG("ERROR", out)
#define ERRORF(format, ...) _LOGF("ERROR", format, __VA_ARGS__)