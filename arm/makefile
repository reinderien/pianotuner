#!/usr/bin/make -f

# Requirements:
# libasound2-dev libatlas-base-dev libgsl-dev
# Also recommended: libasound2-dbgsym libgsl-dbg

export

objs = main.o capture.o

pkg = pkg-config --cflags alsa blas-atlas gsl

cflags = $(shell ${pkg} --cflags) $\
		-D_GNU_SOURCE -DGSL_DISABLE_DEPRECATED $\
		-Wall -std=c18 -ggdb

ldflags = $(shell ${pkg} --libs)

$(info cflags=${cflags})
$(info ldflags=${ldflags})


all: pianotuner

pianotuner: $(objs)
	gcc $$ldflags -o $@ $^

%.o: %.c makefile
	gcc $$cflags -c -o $@ $<

clean:
	git clean -Xf

