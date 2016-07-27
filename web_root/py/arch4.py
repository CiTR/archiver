import archstdlib
from mod_python import apache
from mod_python import psp
import os
from datetime import datetime
from datetime import timedelta
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

def outputPSP(req, n, variables):
	req.content_type = "text/html"
        p = psp.PSP(req, 
		    "psp/%s.psp" % (n),
		    None,
		    variables)
        p.run()

def download(req,fname):
	req.content_type = "audio/mpeg"
	req.sendfile("/tmp/%s.mp3" % (fname))

def arch2(req,startTime,endTime):
	try:
		__startTime = datetime.strptime(startTime, "%d-%m-%Y %H:%M:%S")
		__endTime = datetime.strptime(endTime, "%d-%m-%Y %H:%M:%S")
	except:
		#start time or end time invalid
		outputPSP(req, "index", {"timeError": "true"})
		return
		
	aj = archstdlib.ArchiveJoiner(__startTime, __endTime, "/home/rtav/audio_log/v1")
	mp3 = aj.getMP3()
	f = os.stat(mp3)
	req.content_type = "audio/mpeg"
	req.set_content_length(f.st_size)
	req.headers_out['Content-Disposition'] = "attachment; filename=archive.mp3"
	req.sendfile(mp3)

def stream(req,startTime=-1):
	req.content_type = "audio/mpeg"
	if startTime == '' or int(startTime) == -1:
		#current drag is about 250, but why not be accurate?
		startTime = archstdlib.ArchiveStreamer.getMostRecentTime()
	ars = archstdlib.ArchiveStreamer(startTime)
	while(1):
		f = ars.getNextFile()
		req.write(open(f, "rb").read())
		
	
def archive(req,starTime,hours,minutes,seconds):
	arch = ArchiveRequest(startTime, hours, minutes, seconds)

def index(req,startTime="17-04-2013 00:00:00"):
	outputPSP(req, "index", {})


