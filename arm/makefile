#!/usr/bin/make -f

export

sdl_inc = /usr/include/SDL2
gcc = /usr/bin/gcc

objs = main.o capture.o

cflags=$(shell pkg-config --cflags sdl2) -Wall -std=c18
ldflags=$(shell pkg-config --libs sdl2)


all: pianotuner

$(gcc):
	apt install gcc

$(sdl_inc):
	apt install libsdl2-dev

pianotuner: $(objs)
	$$gcc $$ldflags -o $@ $^

%.o: %.c $(gcc) $(sdl_inc)
	$$gcc $$cflags -c -o $@ $<

clean:
	rm -rf *.o pianotuner

