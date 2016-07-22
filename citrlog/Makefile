CC=gcc 
CFLAGS=--pedantic -Wall -std=c99 -O2
LIBS=-lasound -lsndfile -lmp3lame

all: citrlog

citrlog: citrlog.o ini.o
	$(CC) $(LIBS) citrlog.o ini.o -o citrlog

test41.o:
	$(CC) $(CFLAGS) citrlog.c 

ini.o:
	$(CC) $(CFLAGS) ini.c
clean:
	rm *.o citrlog