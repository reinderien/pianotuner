#pragma once
#include <stddef.h>

// This ring buffer isn't being used as a FIFO. It's being used to save length
// bytes of a stream's immediate past, so that it can be recalled for various
// operations requiring this information. The past can be accessed
// contiguously, without ever having to worry about the underlying memory's
// bounds, because an exact copy of the underlying memory is mapped right after
// it.
typedef struct {
    size_t length;
    void *mem;
    // The next location to be written to. Will always be in the first of the
    // two mirrors.
    void *present;
} VRB;

VRB *vrb_create(size_t length);
void vrb_destroy(VRB *b);
void vrb_advance(VRB *b, size_t length);
void *vrb_past(VRB *b, size_t length);
