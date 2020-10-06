#pragma once


struct CaptureContextTag;
typedef struct CaptureContextTag CaptureContext;

CaptureContext *capture_init(void);
void capture_deinit(CaptureContext*);

void capture_period(
    CaptureContext *ctx,
    void (*consume)(
        const int16_t *samples,
        int n_samples
    )
);

