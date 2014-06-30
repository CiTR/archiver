from datetime import datetime
from datetime import timedelta
from time import mktime
import sys
import tempfile
import os
import time

#####
# CiTR Radio Archive Access
# May, 2013
# for The University Radio Society.
# ###### ##### ##### ####
# ###    #####   #   ## ##
# ###      #     #   ####
# ###    #####   #   ## ##
# ###### #####   #   ## ##
#######


def joinMP3s(files):
	#output = ##find output file name
	output = tempfile.mkstemp(".mp3")
	ofh = open(output[1], 'wb')
	for f in files:
		fh = open(f, 'rb')
		ofh.write(fh.read())
		fh.close()
	ofh.close()
	return output[1]

class ArchiveStreamer:
	__base = "/home/rtav/audio_log/v1/"
	#this is the number of seconds we should lag so as to avoid jittery 
	#streaming
	LAG = 20

	@staticmethod
	def getBase():
                return ArchiveStreamer.__base

	@staticmethod
        def getPath(year, month, day):
                return "%s/%d/%d/%d" % (ArchiveStreamer.getBase(), year, month, day)

	def __init__(self, startTime):
		self.__time = int(startTime)

	@staticmethod
	def getMostRecentTime():
		dt = datetime.now()
		path = "%s" % (ArchiveStreamer.getPath(dt.year, dt.month, dt.day))

		l = os.listdir(path)
		if len(l) < ArchiveStreamer.LAG:
			dt = dt - timedelta(1,0);
			path = "%s" % (ArchiveStreamer.getPath(dt.year, dt.month, dt.day))
			l = os.listdir(path)

		l.sort()
		#remove .LAG seconds.  If we have only LAG for the day
		#well, the client can try connecting again, I guess.  W
		f = l[len(l)-ArchiveStreamer.LAG]

		return int(f[:f.index("-")])
		   
		     
	def getNextFile(self):
		dt = datetime.fromtimestamp(float(self.__time))
		path = "%s/%d-second.mp3" % (self.getPath(dt.year, dt.month, dt.day), self.__time)
		self.__time = self.__time + 1
		return path

class ArchiveJoiner:
	def getBase(self):
		return self.__base
	def getPath(self,year, month, day):
		return "%s/%d/%d/%d" % (self.getBase(), year, month, day)
	def getFile(self, time):
#		seconds = mktime(strptime("%d-%d-%d %d:%d:%d" % (year, month, day, hour, minute, second),
#					     "%d-%m-%Y %H:%M:%S"))
		seconds = int(mktime(time.timetuple()))
		return "%s/%d-second.mp3" % (self.getPath(time.year, time.month, time.day), seconds)
	def __init__(self, startTime, endTime, base, empty=None):
		#if empty has a value, insert that for every blank second
		#should be an mp3 of 1s length
		self.__base = base
		ctime = startTime
		delta = timedelta(0,1)
		self.__files = []
		while ctime <= endTime:
			f = self.getFile(ctime)
			try:
				if os.stat(f):
					self.__files.append(f)
			except:
				if empty is not None:
					self.__files.append(empty)
				#otherwise we just leave it without any mention of silence.
			ctime = ctime + delta

	def getMP3(self):
		return joinMP3s(self.__files)
	
def main():
        __startTime = datetime.strptime("21-5-2013 15:00:00", "%d-%m-%Y %H:%M:%S")
	print __startTime
        __endTime = datetime.strptime("21-5-2013 16:00:00", "%d-%m-%Y %H:%M:%S")
	print __endTime
        aj = ArchiveJoiner(__startTime, __endTime, "/home/rtav/audio_log/v1")
        mp3 = aj.getMP3()
	print "mp3 is %s" % (mp3)
	
if __name__ == "__main__":
    main()
