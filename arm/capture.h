#pragma once


struct CaptureContextTag;
typedef struct CaptureContextTag CaptureContext;


typedef int16_t sample_t;


CaptureContext *capture_init(void);
void capture_deinit(CaptureContext*);

void capture_period(
    CaptureContext *ctx,
    void (*consume)(
        const sample_t *samples,
        int n_samples,
        int rate
    )
);

