#!/usr/bin/make -f

export

alsa_inc = /usr/include/alsa
gcc = /usr/bin/gcc
objs = main.o capture.o

cflags=$(shell pkg-config --cflags alsa) -D_GNU_SOURCE -Wall -std=c18
ldflags=$(shell pkg-config --libs alsa)
$(info cflags=${cflags})
$(info ldflags=${ldflags})


all: pianotuner

$(gcc):
	apt install gcc

$(alsa_inc):
	apt install libasound2-dev

pianotuner: $(objs)
	$$gcc $$ldflags -o $@ $^

%.o: %.c $(gcc) $(alsa_inc) makefile
	$$gcc $$cflags -c -o $@ $<

clean:
	git clean -Xf

