#pragma once

#include <stdint.h>


struct GaugeContextTag;
typedef struct GaugeContextTag GaugeContext;


GaugeContext *gauge_init(void);
void gauge_deinit(GaugeContext**);

void gauge_message(
    GaugeContext *ctx,
    float v1,
    float v2,
    float v4,
    float v5
);

