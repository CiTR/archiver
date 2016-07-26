CC=gcc 
CFLAGS=-std=gnu99 -Wall -O2 -g
LIBS=-lasound -lsndfile -lmp3lame -pthread

all: citrlog

citrlog:
	$(CC) $(CFLAGS) -o citrlog citrlog.c ini.c $(LIBS)

clean:
	rm citrlog
