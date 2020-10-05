#pragma once


struct CaptureContextTag;
typedef struct CaptureContextTag CaptureContext;

CaptureContext *capture_init(void);
void capture_deinit(CaptureContext*);

