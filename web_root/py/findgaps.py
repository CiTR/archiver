from datetime import date
from datetime import datetime
from datetime import timedelta
from time import strftime
from time import mktime
from time import localtime
import sys
import tempfile
import os

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


class GapFinder:
	def getBase(self):
		return "/home/rtav/audio_log/v2"
	def getPath(self,year, month, day):
		return "%s/%d/%d/%d" % (self.getBase(), year, month, day)
	def getFile(self, time):
#		seconds = mktime(strptime("%d-%d-%d %d:%d:%d" % (year, month, day, hour, minute, second),
#					     "%d-%m-%Y %H:%M:%S"))
		seconds = int(mktime(time.timetuple()))
		return "%s/%d-second.mp3" % (self.getPath(time.year, time.month, time.day), seconds)
	def __init__(self, date, sTime = "00:00:00", eTime="23:59:59"):
		#if empty has a value, insert that for every blank second
		#should be an mp3 of 1s length
		startTime = datetime(date.year, date.month, date.day, 0, 0, 0)
		endTime = datetime(date.year, date.month, date.day, 23, 59, 59)
		ctime = startTime
		delta = timedelta(0,1)
		self.__gaps = []
		while ctime <= endTime:
			f = self.getFile(ctime)
			try:
				if os.stat(f):
					pass
				#do nothing if found
			except:
				t = mktime(ctime.timetuple())
				self.__gaps.append(t)
				#otherwise we just leave it without any mention of silence.
			ctime = ctime + delta

	def getGaps(self):
		return self.__gaps
	

def main():
	gf = GapFinder(date(2013, 5, 28))
	last = 0
	for gap in gf.getGaps():
		lt = localtime(gap)
		last = gap
		if !(gap == (last+1)):
			s = strftime("%a, %d %b %Y %H:%M:%S +0000", lt)
			print "Gap for %s (%d)" % (s,gap)
		else:
			#otherwise, print nothing
	

if __name__ == "__main__":
    main()
			
