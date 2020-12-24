#!/usr/bin/make -f

# Requirements:
# libasound2-dev libatlas-base-dev linux-libc-dev
# Also recommended: libasound2-dbgsym

export

objs = main.o capture.o freq.o gauge.o

pkg = pkg-config --cflags alsa

armpl = /opt/arm/armpl_20.3_gcc-9.3

cflags = $(shell ${pkg} --cflags) $\
         -I${armpl}/include -D_GNU_SOURCE -Wall -std=c18
ifdef DEBUG
	cflags += -ggdb
else
	cflags += -s -O3 -flto -fomit-frame-pointer -march=native
endif

ldflags = $(shell ${pkg} --libs) -L${armpl}/lib -larmpl -lgfortran -lm -Wl,--warn-common
ifndef DEBUG
	ldflags += -Wl,--relax,-O3
endif

$(info cflags=${cflags})
$(info ldflags=${ldflags})


all: pianotuner

pianotuner: $(objs)
	gcc $$cflags $$ldflags -o $@ $^

%.o: %.c makefile
	gcc $$cflags -c -o $@ $<

clean:
	git clean -Xf

