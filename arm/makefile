#!/usr/bin/make -f

export

alsa_inc = /usr/include/alsa
gsl_inc = /usr/include/gsl
gcc = $(shell which gcc)

objs = main.o capture.o

cflags = $(shell pkg-config --cflags alsa) $\
    -I/usr/include/gsl $\
    -D_GNU_SOURCE -DGSL_DISABLE_DEPRECATED $\
    -Wall -std=c18 -ggdb

ldflags = $(shell pkg-config --libs alsa) $\
          $(shell pkg-config --libs gsl) -L:libatlas.so.3

$(info gcc=${gcc})
$(info cflags=${cflags})
$(info ldflags=${ldflags})


all: pianotuner

$(gcc):
	apt install gcc

$(alsa_inc):
	apt install libasound2-dev

$(gsl_inc):
	apt install libgsl-dev libatlas3-base

pianotuner: $(objs)
	$$gcc $$ldflags -o $@ $^

%.o: %.c $(gcc) $(alsa_inc) makefile
	$$gcc $$cflags -c -o $@ $<

clean:
	git clean -Xf

