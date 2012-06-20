#include <stdio.h>

#define DEBUG(out) printf("%s:%d: %s\n", __FILE__, __LINE__, out);
#define ERROR(out) printf("Error: %s:%d: %s\n", __FILE__, __LINE__, out);