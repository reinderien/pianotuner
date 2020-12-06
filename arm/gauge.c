#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "gauge.h"


struct GaugeContextTag
{
};


GaugeContext *gauge_init(void)
{
    GaugeContext *ctx = malloc(sizeof(GaugeContext));
    assert(ctx);

    return ctx;
}

void gauge_deinit(GaugeContext *ctx)
{
    free(ctx);

    puts("Gauge deinitialized");
}


