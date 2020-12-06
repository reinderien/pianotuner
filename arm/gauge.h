#pragma once

#include <stdint.h>


struct GaugeContextTag;
typedef struct GaugeContextTag GaugeContext;


GaugeContext *gauge_init(void);
void gauge_deinit(GaugeContext*);

void gauge_message(
    uint16_t v1,
    uint16_t v2,
    uint8_t v4,
    uint16_t v5
);

