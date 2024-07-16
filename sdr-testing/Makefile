CC=gcc
###CFLAGS=-O -Wall
CFLAGS=-g -Wall -Wl,-rpath=/usr/local/lib64
LDLIBS=-lsdrplay_api

downsamplemake: downsample.c filters.h
	gcc -o downsample downsample.c filters.h -lsdrplay_api

clean:
	rm -f *.o sdrplay_api_sample_app
