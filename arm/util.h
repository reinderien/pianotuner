#include <stdlib.h>

// determines the length  of a static array.
#define SALEN(sa) (sizeof(sa)/sizeof(*sa))

float mod1rd(float x);
float clip(float x);
void save(void *mem, size_t size, const char *fn);
