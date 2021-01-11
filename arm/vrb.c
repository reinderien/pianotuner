#include <inttypes.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "vrb.h"


/*
Make a new virtual ring buffer. Its length will be at least length bytes,
rounded upward to the nearest multiple of the page size. (You should never need
to know how big the pages are in order to use this code, but if for some reason
you need to know the page size, use sysconf(_SC_PAGESIZE) from unistd.h to get
it in bytes, as is standard practice on the unices.)
*/
VRB *vrb_create(size_t length)
{
    assert(length != 0);

    VRB *b = malloc(sizeof(VRB));
    assert(b != NULL);

    size_t PS = sysconf(_SC_PAGESIZE);
    b->length = length - 1 + PS - (length - 1)%PS;

    /*
    Getting two independent mmaps, A and B, right next to each other is tricky.
    We don't care where A:B goes in memory, as long as A is right next to B.
    MAP_FIXED could be used to place B right after A, but there needs to be
    room for it. So, A must be placed such that there is this room for B. But
    mmap doesn't allow you to reserve extra space without also mapping it. So,
    how do we place A? You could manually check all of the mmaps your process
    has already made, pick a region yourself that is big enough for A and B,
    and mmap A (and B) to that region using MAP_FIXED, but that's crazy. A
    better way is to use a MAP_ANONYMOUS mmap to find a free region the size of
    A and B combined, then map A and B inside that free region using two
    MAP_FIXED mmap calls. MAP_FIXED automatically unmaps existing mmaps
    overlapping with the requested region, so the original map will be gone. As
    long as you never write to the initial MAP_ANONYMOUS mmap, it will never
    actually get allocated.

    So, that's what we're going to do. Do a dummy allocation A:B in one
    contiguous region just to figure out where A:B can go, then allocate A and
    B inside that region. After that, the original dummy allocation will have
    been totally deallocated, but will have served its purpose, which was to
    find a place where A:B can go.
    */
    b->mem = mmap(
        NULL,  // requested starting address
        2*b->length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,  // fd
        0  // offset within file
    );
    assert(b->mem != MAP_FAILED);

    /*
    mmap can not arbitrarily remap memory regions to other regions by address.
    It can only produce shared mappings through the fd of a shared file. So, we
    need to make a fake file backed by memory only and use its fd. This is
    memfd_create's purpose.
    CLOEXEC is not on by default, but is a good default, lol. Just look it up
    in man 2 memfd_create for more.
    */
    int fd = memfd_create("VRB", MFD_CLOEXEC);
    ftruncate(fd, b->length);

    void *maddr = mmap(
        b->mem,
        b->length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FIXED,
        fd,
        0
    );
    assert(maddr == b->mem);
    maddr = mmap(
        b->mem + b->length,
        b->length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FIXED,
        fd,
        0
    );
    assert(maddr == b->mem + b->length);

    /*
    Now that the memory has been mapped, the fd can be closed. The
    memory-backed file will continue to exist until the mmaps are unmapped.
    */
    assert(close(fd) == 0);

    b->present = b->mem;

    return b;
}


void vrb_destroy(VRB *b)
{
    assert(munmap(b->mem, 2*b->length) == 0);
    free(b);
}


/*
Move b->present ahead by length bytes. Meant to be done after writing length
bytes to b->present, so advances of more than b->length are considered invalid.
*/
void vrb_advance(VRB *b, size_t length)
{
    assert(length <= b->length);
    size_t i = b->present - b->mem;
    b->present = b->mem + (i + length)%b->length;
}


/*
Access the buffer's past. Returns a pointer to the buffer at length bytes in
the past. The returned pointer is always in the first mapped region, so it is
guaranteed that length bytes may be accessed contiguously after the returned
pointer. (length's max is the buffer's length, b->length.)
*/
void *vrb_past(VRB *b, size_t length)
{
    assert(length <= b->length);
    size_t i = b->present - b->mem;
    /*
    Avoid negatives, to avoid narrowing an unsigned type by one bit and because
    the behaviour of x%m for negative x is very unhelpful.
    */
    size_t i_past = (i + b->length - length)%b->length;
    return b->mem + i_past;
}
