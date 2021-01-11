#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "gauge.h"


#define DEV_FILENAME "/dev/spidev0.0"
const unsigned
    TARGET_SPEED = 1000000,
    INDEX_POS = 5;
const uint16_t
    CH1_MASK = (1 << 10) - 1,
    CH2_MASK = CH1_MASK,
    CH5_MASK = CH1_MASK;
const uint8_t CH4_MASK = (1 << 5) - 1;


struct GaugeContextTag
{
    uint32_t mode, speed;
    uint8_t word_bits;
    int fd;
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
        ) == -1,
        "failed to set SPI word bits"
    );

    uint32_t orig_speed;
    check_c(
        ioctl(
            ctx->fd, SPI_IOC_RD_MAX_SPEED_HZ, &orig_speed
        ) == -1,
        "failed to get SPI max speed"
    );

    printf("Current SPI speed %u; changing to %u\n", orig_speed, TARGET_SPEED);
    ctx->speed = TARGET_SPEED;
    if (warn_c(
        ioctl(
            ctx->fd, SPI_IOC_WR_MAX_SPEED_HZ, &ctx->speed
        ) == -1,
        "failed to set SPI max speed"
    )) {
        ctx->speed = orig_speed;
    }

    return ctx;
}


void gauge_message(
    const GaugeContext *ctx,
    float db,
    float octave,
    float semitone,
    float deviation
)
{
    uint16_t u1 = octave*CH1_MASK,
             u2 = db*CH2_MASK,
             u5 = deviation*CH5_MASK;
    uint8_t u4 = semitone*CH4_MASK;

    const uint8_t message[] = {
        (1 << INDEX_POS) | (u1 >> 8),
        u1,
        (2 << INDEX_POS) | (u2 >> 8),
        u2,
        (5 << INDEX_POS) | (u5 >> 8),
        u5,
        (4 << INDEX_POS) | u4
    };

    struct spi_ioc_transfer transfer =
    {
        .tx_buf = (uint64_t)message,
        .len = sizeof(message)
    };

    const int n_messages = 1;

    warn_c(
        ioctl(
            ctx->fd,
            SPI_IOC_MESSAGE(n_messages),
            &transfer
        ) == -1,
        "Failed to transfer SPI message"
    );
}


void gauge_deinit(GaugeContext **ctx)
{
    warn_c(close((*ctx)->fd) == -1, "Failed to close SPI handle");

    free(*ctx);
    *ctx = NULL;

    puts("Gauges deinitialized");
}


void gauge_demo(const GaugeContext *ctx)
{
    const struct timespec sleep_time = {.tv_sec=0, .tv_nsec = 10000000};

    float f1=0.1, f2=0.3, f3=0.5, f4=0.7;
    // 0.1/s
    const float delta = 0.1 * sleep_time.tv_nsec/1e9;

    while (true)
    {
        gauge_message(ctx, f1+0.2, f2+0.2, f3+0.2, f4+0.2);
        nanosleep(&sleep_time, NULL);

        f1 = fmodf(f1, 0.7)+delta;
        f2 = fmodf(f2, 0.7)+delta;
        f3 = fmodf(f3, 0.7)+delta;
        f4 = fmodf(f4, 0.7)+delta;
    }
}

