#!/usr/bin/python

###this program records all audio for a day in the appropriate directory
## have cron kill it and restart once daily, at 00:00'10 or something.
## once killed we will have audio.mp3 in the appropriate directory, and if we sleep until the next day, everything
## should work OK
import datetime
import os
import subprocess

def getFileName():
    return "audio.mp3"

def getDirectory(day, month, year):
    return "%s/%s/%s/%s" %(getAudioBasePath(), year, month, day)

def getMP3(day, month, year):
    return "%s/%s" %(getDirectory(day, month, year), getFileName())

def getAudioBasePath():
    return "/home/rtav/audio_log"

def getRecordCommand():
    return ["/usr/bin/wget", "live.citr.ca:8000/stream.mp3", "-O"];

def doRecord():
    t = datetime.date.today()
    cmd = "%s %s" % (getRecordCommand(),
                     getMP3(t.day, t.month, t.year))
    #check that output directory exists.  Make it if it doesn't
    directory = getDirectory(t.day, t.month, t.year)
    try:
        print "Searching for directory..."
        os.stat(directory)
    except:
        print "Directory not found, mking"
        os.mkdir(directory)
        
    #execute cmd
    cmdl = getRecordCommand() + [getMP3(t.day, t.month, t.year)]
    print cmdl
    subprocess.call(cmdl)

doRecord()
