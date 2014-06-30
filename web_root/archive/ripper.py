#!/usr/bin/python

import time
import datetime
import os
import subprocess
import daemon

#rips stream from line  input

#constants

cmd = "arecord -f cd -d %d | lame - %s/%s"

def getRecordingFile(day,month,year):
    for n in range(0,100):
        dirname = getDirectory(day, month, year)
        try:
            stat("%s/recording-%d.mp3" % (dirname, n))
        except:
            return "recording-%d.mp3" % (n)
    return "recording-NNN.mp3"

# functions
def getDayDirectory(day, month, year):
    return getDirectory(day, month, year)

def getMonthDirectory(month, year):
    return "%s/%s/%s" %(getAudioBasePath(), year, month)

def getYearDirectory(year):
    return "%s/%s" %(getAudioBasePath(), year)

def getDirectory(day, month, year):
    return "%s/%s/%s/%s" %(getAudioBasePath(), year, month, day)

def getMP3(day, month, year):
    return "%s/%s" %(getDirectory(day, month, year), getFileName())

def getAudioBasePath():
    return "/home/rtav/audio_log"

def insureDirectory(d):
    try:
        os.stat(d)
    except:
        os.mkdir(d)

def writeStartTime(d):
    now = datetime.datetime.now()
    f = open("%s/starttime.log" % (d), "w")
    f.write(str(now))
    f.close()

def makeDirectoriesAndLogs():
    t = datetime.date.today()
    #insure directories exist
    insureDirectory(getYearDirectory(t.year))
    insureDirectory(getMonthDirectory(t.month, t.year))
    insureDirectory(getDirectory(t.day, t.month, t.year))
    writeStartTime(getDirectory(t.day, t.month, t.year))

def getSecondsLeftInDay():
    t = datetime.datetime.now()
    seconds = t.hour * 60 * 60
    seconds = seconds + t.minute * 60
    seconds = seconds + t.second
    return (86400 - seconds)

def getTodayAudioPath():
    t = datetime.datetime.now()
    return "/home/rtav/audio_log/%s/%s/%s" % (t.year, t.month, t.day)

def getCmd():
    return cmd % (getSecondsLeftInDay(), getTodayAudioPath())


def doMain():
    while True:
        cmd = getCmd()
        makeDirectoriesAndLogs()
        out = subprocess.check_call(cmd, shell=True)

### daemon. 
print "CiTR Ripper v. 0.2"
print "April, 2013"
print "Detaching..."
with daemon.DaemonContext():
    doMain()

