#include <inttypes.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
// determines the length  of a static array.
#define SALEN(sa) (sizeof(sa)/sizeof(*sa))

#include <time.h>
#include <unistd.h>

#include "vrb.h"

int main(int argc, const char **argv)
{
    srandom(time(NULL));
    size_t PS = sysconf(_SC_PAGESIZE);

    // A basic test of the mmap mirroring.
    {
        VRB *b = vrb_create(PS);
        memset(b->mem, ' ', b->length);
        strcpy(b->mem, "test!");
        assert(strcmp(b->mem, b->mem + b->length) == 0);
        vrb_destroy(b);
    }

    // Test the length determination code.
    {
        struct {
            size_t requested;
            size_t actual;
        } tests[] = {
            {1, PS},
            {PS - 1, PS},
            {PS, PS},
            {PS + 1, 2*PS},
            {7*PS, 7*PS},
            {7*PS + PS/3, 8*PS}
        };
        for (unsigned i = 0; i < SALEN(tests); i++)
        {
            VRB *b = vrb_create(tests[i].requested);
            assert(b->length == tests[i].actual);
            vrb_destroy(b);
        }
    }

    // Test present writing and past reading.
    {
        VRB *b = vrb_create(2*PS);
        unsigned N = b->length / sizeof(u32);
        unsigned C[] = {91, N - 45, N - 1, N, N/2, N/2 - 1};
        for (unsigned i = 0; i < SALEN(C); i++)
        {
            u32 Y = random();

            for (u32 x = 0; x < C[i]; x++)
            {
                u32 *p = b->present;
                p[x] = Y + x;
            }
            vrb_advance(b, C[i]*sizeof(u32));

            u32 *p = vrb_past(b, C[i]*sizeof(u32));
            for (u32 x = 0; x < C[i]; x++)
                assert(p[x] == Y + x);
        }
        vrb_destroy(b);
    }

    return 0;
}
