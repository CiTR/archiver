#!/bin/bash

SECOND_IN_DAY = "86500"

while true; do 
    OUTPUT_DATE = $(date +%D)
    OUTPUT_MP3_FILE = "/home/rtav/audio_log/$OUTPUT_DATE/recording.mp3"
    REMAINING_SECOND = $(expr $SECONDS_IN_DAY - $(expr $(date +%k) * 60) - $(expr ($date +%)
    arecord -f cd -d $SECOND_IN_DAY | lame - $OUTOUT_MP3_FILE 
done
