CC=gcc 
CFLAGS=-std=gnu99 -Wall -O2
LIBS=-lasound -lsndfile -lmp3lame

all: citrlog

citrlog: citrlog.o ini.o
	$(CC) $(LIBS) citrlog.o ini.o -o citrlog

citrlog.o:
	$(CC) $(CFLAGS) citrlog.c 

ini.o:
	$(CC) $(CFLAGS) ini.c
clean:
	rm *.o citrlog
