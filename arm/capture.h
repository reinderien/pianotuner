#pragma once


struct CaptureContextTag;
typedef struct CaptureContextTag CaptureContext;


typedef int16_t sample_t;


CaptureContext *capture_init(void);
void capture_deinit(CaptureContext**);

void capture_do_capture(
    CaptureContext *ctx,
    void (*consume)(
        CaptureContext *cc,
        const sample_t *restrict samples,
        void *p
    ),
    void *p
);

unsigned capture_period(CaptureContext *c);
unsigned capture_rate(CaptureContext *c);
