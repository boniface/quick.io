#include <stdio.h>

#define _LOG(level, out) fprintf(stderr, level " : %s:%d : %s\n", __FILE__, __LINE__, out)
#define DEBUG(out) _LOG("Debug", out)
#define ERROR(out) _LOG("Error", out)