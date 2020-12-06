#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gauge.h"


#define DEV_FILENAME "/dev/spidev0.0"
const unsigned
    TARGET_SPEED = 1000000,
    INDEX_POS = 5,
    MESSAGE_LEN = 7;
const uint16_t
    CH1_MASK = (1 << 10) - 1,
    CH2_MASK = CH1_MASK,
    CH5_MASK = CH1_MASK;
const uint8_t CH4_MASK = (1 << 5) - 1;


struct GaugeContextTag
{
    uint32_t mode, speed;
    uint8_t word_bits;
    uint8_t message[MESSAGE_LEN];

    int fd;
    struct spi_ioc_transfer transfer;
};


static bool warn_c(bool bad, const char *message)
{
    if (bad)
    {
        fprintf(
            stderr, "Error %d: %s (%s)\n",
            errno, strerror(errno), message
        );
    }
    return bad;
}


static void check_c(bool bad, const char *message)
{
    warn_c(bad, message);
    if (bad)
        exit(1);
}


GaugeContext *gauge_init(void)
{
    GaugeContext *ctx = malloc(sizeof(GaugeContext));
    assert(ctx);

    ctx->fd = open(DEV_FILENAME, O_WRONLY);
    check_c(ctx->fd == -1, "Failed to open SPI handle for " DEV_FILENAME);

    ctx->mode = SPI_NO_CS;
    check_c(
        ioctl(
            ctx->fd, SPI_IOC_WR_MODE32, &ctx->mode
        ) == -1,
        "Failed to set SPI mode"
    );

    ctx->word_bits = 8;
    check_c(
        ioctl(
            ctx->fd, SPI_IOC_WR_BITS_PER_WORD, &ctx->word_bits
        ) == -1.
        "failed to set SPI word bits"
    );

    uint32_t orig_speed;
    check_c(
        ioctl(
            ctx->fd, SPI_IOC_RD_MAX_SPEED_HZ, &orig_speed
        ) == -1,
        "failed to get SPI max speed"
    );

    printf("Current speed %u; changing to %u", orig_speed, TARGET_SPEED);
    ctx->speed = TARGET_SPEED;
    if (warn_c(
        ioctl(
            ctx->fd, SPI_IOC_WT_MAX_SPEED_HZ, &ctx->speed
        ) == -1,
        "failed to set SPI max speed"
    )) {
        ctx->speed = orig_speed;
    }

    ctx->transfer = (spi_ioc_transfer)
    {
        .tx_buf = ctx->message,
        .len = MESSAGE_LEN
    };

    return ctx;
}


void gauge_message(
    GaugeContext *ctx,
    float v1,
    float v2,
    float v4,
    float v5
)
{
    uint16_t u1 = v1*CH1_MASK,
             u2 = v2*CH2_MASK,
             u5 = v5*CH5_MASK;
    uint8_t u4 = v4*CH4_MASK;

    ctx->message = (uint8_t[]){
        (1 << INDEX_POS) | (u1 >> 8),
        u1,
        (2 << INDEX_POS) | (u2 >> 8),
        u2,
        (5 << INDEX_POS) | (u5 >> 8),
        u5,
        (4 << INDEX_POS) | u4
    };

    const int n_messages = 1;

    warn_c(
        ioctl(
            ctx->fd,
            SPI_IOC_MESSAGE(n_messages),
            &ctx->transfer
        ) == -1,
        "Failed to transfer SPI message"
    );
}


void gauge_deinit(GaugeContext **ctx)
{
    warn_c(close(*ctx->fd) == -1, "Failed to close SPI handle");

    free(*ctx);
    *ctx = NULL;

    puts("Gauge deinitialized");
}


