#pragma once

#include <stdint.h>


struct GaugeContextTag;
typedef struct GaugeContextTag GaugeContext;


GaugeContext *gauge_init(void);
void gauge_deinit(GaugeContext**);

void gauge_message(
    const GaugeContext *ctx,
    float db,
    float octave,
    float semitone,
    float deviation
);

void gauge_demo(const GaugeContext *ctx);

