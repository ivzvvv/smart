CC=gcc
###CFLAGS=-O -Wall
CFLAGS=-g -Wall -Wl,-rpath=/usr/local/lib64
LDLIBS=-lsdrplay_api

all: sdrplay_api_sample_app

run: sdrplay_api_sample_app
	./sdrplay_api_sample_app

clean:
	rm -f *.o sdrplay_api_sample_app
