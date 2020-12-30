#define _GNU_SOURCE  // for linux-only features, like O_DIRECT
#include <inttypes.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "util.h"

int main(int argc, const char **argv)
{
    assert(argc == 2);
    float x;
    int nmatches = sscanf(argv[1], "%f", &x);
    assert(nmatches == 1);
    printf("%f\n", mod1rd(x));
    return 0;
}
