from mod_python import apache
from mod_python import psp
from datetime import datetime
from datetime import timedelta
from time import mktime
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

class ArchiveJoiner:
	def getBase(self):
		return self.__base
#		return "/home/rtav/audio_log/v1"
	def getPath(self,year, month, day):
		return "%s/%d/%d/%d" % (self.getBase(), year, month, day)
	def getFile(self, time):
		seconds = int(mktime(time.timetuple()))
		return "%s/%d-second.mp3" % (self.getPath(time.year, time.month, time.day), 
					     seconds)
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
        aj = ArchiveJoiner(__startTime, __endTime)
        mp3 = aj.getMP3()
	print "mp3 is %s" % (mp3)
	

if __name__ == "__main__":
    main()
			
		

def outputPSP(req, n, variables):
	req.content_type = "text/html"
        p = psp.PSP(req, "psp/%s.psp" % (n),variables)
        p.run()

def download(req,fname):
	req.content_type = "audio/mpeg"
	req.sendfile("/tmp/%s.mp3" % (fname))

def download(req,startTime,endTime,archive):
	__startTime = datetime.strptime(startTime, "%d-%m-%Y %H:%M:%S")
	__endTime = datetime.strptime(endTime, "%d-%m-%Y %H:%M:%S")
	aj = ArchiveJoiner(__startTime, __endTime, archive)
	mp3 = aj.getMP3()
	f = os.stat(mp3)
	req.set_content_length(f.st_size)
	req.headers_out['Content-Disposition'] = "attachment; filename=archive.mp3"
	req.sendfile(mp3)
	
def archive(req,starTime,hours,minutes,seconds):
	arch = ArchiveRequest(startTime, hours, minutes, seconds)

def index(req,startTime="17-04-2013 00:00:00"):
	outputPSP(req, "index", {})


def hello(req, say="NOTHING"):
	outputPSP(req, "hello")
