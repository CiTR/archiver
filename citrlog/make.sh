#!/bin/sh -x
gcc -o citrlog -O3 -lasound -lsndfile -lmp3lame ini.c citrlog.c
strip citrlog
