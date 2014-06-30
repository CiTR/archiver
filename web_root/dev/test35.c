/** 
 * audio logger
 * (C) 2013, for the university radio society.
 */

/**
 *TODO:
 *
 * Add configuration file
 * Logging
 * Signal handling
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <time.h>
#include <lame/lame.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timeb.h>

//google ini lib
#include "ini.h"

#define DEBUG(x) logs(x);

#define MP3_BUFFER_SIZE 65536
#define SHORT_BUFFER_SIZE 44100

#define BITS_IN_SECOND (long)(16 * 44100)
#define BITS_IN_MINUTE (long)(BITS_IN_SECOND * 60)
#define BITS_IN_HOUR (long)(BITS_IN_MINUTE * 60)
#define BITS_IN_DAY (long)(BITS_IN_HOUR * 24)

//data structures
typedef struct _conf_t
{
  char *pAudioLogBase;
  char *pLogFile;
  FILE *logFileHandle;
  char *pDetach;
  char *pDevice;
  int sampleRate;
  int mp3Rate;
  int verbose;
  int bufferSize;
} conf_t;
conf_t configuration;

char dirbuf[1024];

//function prototypes
char* mkyeardir(int year);
char* mkmonthdir(int year, int month);
char* mkdaydir(int year, int month, int day);
int start(int argc, char *argv[]);
int handler(void* user, const char* section,
	    const char* name, const char* value);
void logs(char *str);
int setAttribute(char **pPAttr, const char *str);
void logBits(long long bytes);
int isDetached();
snd_pcm_t* initCapture(char * dev);
lame_t initLAME();

//function implementations
int isDetached()
{
  return strcmp(configuration.pDetach, "true") == 0;
}

void logBits(long long bytes)
{
  char buf[256];
  sprintf(buf, "Logged %.lld bits (%.lld bytes)", bytes, bytes/8);
  logs(buf);
}

void logSeconds(long long bits)
{
  char buf[256];
  sprintf(buf, "Logged %.lld seconds", bits/BITS_IN_SECOND);
  logs(buf);
}

void logs(char *str)
{
  char logString[1024];
  char timeString[128];
  time_t ts = time(&ts);
  struct tm *tmm = localtime(&ts);
  strftime(timeString, sizeof(timeString), "%D %T", tmm);
  if(configuration.logFileHandle != NULL) {    
    sprintf(logString, "%s: %s\n%%s", timeString, str);
    fprintf(configuration.logFileHandle, logString, "");
    fflush(configuration.logFileHandle);
    if(!isDetached()) {
      printf(logString, "");
    }
  }
}

int handler(void* user, const char* section,
            const char* name, const char* value)
{
  conf_t *pConf = (conf_t*)user;
  if(strcmp("audiologbase", name) == 0) {
    setAttribute(&pConf->pAudioLogBase, value);
  }
  else if(strcmp("logfile", name) == 0) {
    setAttribute(&pConf->pLogFile, value);
    //open log file for appending
    pConf->logFileHandle = fopen(pConf->pLogFile, "a");
  }
  else if(strcmp("detach", name) == 0) {
    setAttribute(&pConf->pDetach, value);
  }
  else if(strcmp("device", name) == 0) {
    setAttribute(&pConf->pDevice, value);
  }
  else if(strcmp("samplerate", name) == 0) {
    setAttributeInt(&pConf->sampleRate, value);
  }
  else if(strcmp("mp3rate", name) == 0) {
    setAttributeInt(&pConf->mp3Rate, value);
  }
  else if(strcmp("buffersize", name) == 0) {
    setAttributeInt(&pConf->bufferSize, value);
  }

  else if(strcmp("verbose", name) == 0) {
    if(strcmp("true", value) == 0) {
      pConf->verbose = 1;
    }
    else {
      pConf->verbose = 0;
    }
  }
  return 0;
}

int setAttributeInt(int *pAttr, const char *str)
{
  *pAttr = atoi(str);
}

int setAttribute(char **pPAttr, const char *str)
{
  *pPAttr = (char *)malloc((strlen(str)+1)*sizeof(char));
  strcpy(*pPAttr, str);
}

char* mkyeardir(int year)
{
  struct stat buf;
  sprintf(dirbuf, "%s/%d", configuration.pAudioLogBase, year);
  if(stat(dirbuf, &buf) == -1) {
    //create dir, otherwise exists
    mkdir(dirbuf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  return dirbuf;
}

char* mkmonthdir(int year, int month)
{
  struct stat buf;
  sprintf(dirbuf, "%s/%d/%d", configuration.pAudioLogBase, year, month);
  if(stat(dirbuf, &buf) == -1) {
    //create dir, otherwise exists                                                                                     
    mkyeardir(year);
    sprintf(dirbuf, "%s/%d/%d", configuration.pAudioLogBase, year, month);
    mkdir(dirbuf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  return dirbuf;

}
char* mkdaydir(int year, int month, int day)
{
  struct stat buf;
  sprintf(dirbuf, "%s/%d/%d/%d", configuration.pAudioLogBase, year, month, day);
  if(stat(dirbuf, &buf) == -1) {
    //create dir, otherwise exists                                                                                     
    mkyeardir(year);
    mkmonthdir(year, month);
    sprintf(dirbuf, "%s/%d/%d/%d", configuration.pAudioLogBase, year, month, day);
    mkdir(dirbuf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  return dirbuf;

}

int main(int argc, char *argv[]) 
{
  char *pConfig = "config.ini";
  //we might have a config file specified
  if(argc >= 2) {
    pConfig = argv[1];
  }
  ini_parse(pConfig,
	    handler,
	    (void*)&configuration);
  DEBUG("Read configuration...");
  if(isDetached()) {
    DEBUG("Detaching...");
    //detach
    pid_t pID = fork();
    if(pID == 0) { //child
      //ensure child has configuration as well
      ini_parse(pConfig,
		handler,
		(void*)&configuration);
      while(1) {
	DEBUG("Starting Child...");
	start(argc, argv);
	DEBUG("start returned.");
      }
    }
    else if(pID < 0) { //failure
      DEBUG("ERROR Forking");
    }
    else { //parent
      DEBUG("CiTR Archiver. Detached.");
    }
  }
  else {
    //
    while(1) {
      DEBUG("Not detaching...");
      start(argc, argv);
      DEBUG("start returned.");
    }
  }

}

lame_t initLAME() 
{
  lame_t lame;
  //initialize lame encoder
  DEBUG("Initializing LAME...");
  lame = lame_init();
  lame_init_params(lame);
  lame_set_in_samplerate(lame, configuration.sampleRate);
  lame_set_VBR(lame, vbr_off);
  lame_set_brate(lame, configuration.mp3Rate);
  return lame;
}
  
snd_pcm_t* initCapture(char * dev)
{
  char *pDevice;
  snd_pcm_t *capture_handle;
  snd_pcm_hw_params_t *hw_params;
  int err;
  int rate = configuration.sampleRate;

  pDevice = strdup(dev);
  if ((err = snd_pcm_open (&capture_handle, pDevice, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    fprintf (stderr, "cannot open audio device %s (%s)\n", 
	     dev,
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Opened audio device.");

  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Allocated params.");
   
  if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
    fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Initialized params.");

  if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "cannot set access type (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Set Access.");

  if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "cannot set sample format (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Set Format");  

  if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
    fprintf (stderr, "cannot set sample rate (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Set Rate.");
  
  if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) {
    fprintf (stderr, "cannot set channel count (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Set Channels");
  
  if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
    fprintf (stderr, "cannot set parameters (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Set parameters.");
  
  snd_pcm_hw_params_free (hw_params);
 
  if ((err = snd_pcm_prepare (capture_handle)) < 0) {
    fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
	     snd_strerror (err));
    exit (1);
  }
  DEBUG("Prepared Interface.");
  return capture_handle;
}


int start(int argc, char *argv[])
{
  //variables
  long long bitCount = 0;
  long long totalbits = 0;
  int err;
  short *buf;
  snd_pcm_t *capture_handle;
  time_t sTime;
  struct tm * timeinfo;
  FILE *mp3;
  lame_t lame;
  int write;
  unsigned char mp3_buffer[MP3_BUFFER_SIZE];
  char mp3file[256];
  int breakOut = 0;
  struct stat statbuf;
  
  capture_handle = initCapture(configuration.pDevice);
  lame = initLAME();
  //initialize lame encoder
  totalbits = 0;
  //grab start time
  time(&sTime);
  //initialize buffer
  buf = (short*)malloc(sizeof(short) * (configuration.bufferSize*2));
  while(1) {
    //parse time into useful information
    timeinfo = localtime(&sTime);
    //generate file name
    sprintf(mp3file, 
	    "%s/%d-second.mp3", 
	    mkdaydir(timeinfo->tm_year+1900, 
		     timeinfo->tm_mon+1, 
		     timeinfo->tm_mday), 
	    (int)sTime);
    //open file handle
    mp3 = fopen(mp3file, "wb");
    //reset bitCount to 0
    bitCount = 0;
    //loop until BITS_IN_SECOND bits read.
    //this should be a specific number of iterations, as each
    //iteration reads SHORT_BUFFER_SIZE * 16 bits
    //this should be only one iteration if buffer size is 44100
    while(bitCount < (BITS_IN_SECOND)) {
      if ((err = snd_pcm_readi (capture_handle, buf, configuration.bufferSize)) != configuration.bufferSize) {
	if(err == -EPIPE) {
	  DEBUG("Buffer Overrun");
	  if(snd_pcm_prepare(capture_handle) == 0) {
	    DEBUG("Prepared capture_handle for more use...");
	  }
	  continue;
	}
	//otherwise:
	//Correct error here, best choice is to restart.
	//this will be accomplished by returning, which will cause re-entry into this function.
	DEBUG("Error reading from audio interface");
	DEBUG((char*)snd_strerror(err));
	breakOut = breakOut + 1;
	snd_pcm_prepare(capture_handle);
	if(breakOut > 10) { //try ten times
	  break;
	}
	
      }
      else {
	// SHORT_BUFFER_SIZE 16 bit samples added
	bitCount = bitCount + (configuration.bufferSize * 16); 
	//encode mp3
	write = lame_encode_buffer_interleaved(lame, buf, configuration.bufferSize, mp3_buffer, MP3_BUFFER_SIZE);
	//write mp3 to disk
	fwrite(mp3_buffer, write, 1, mp3);
      }
    }
    if(bitCount != (BITS_IN_SECOND)) { //log anomalous bitCount
      logs("Anamalous bitCount:");
      logBits(bitCount);
    }
    //add to total bits
    totalbits = totalbits + bitCount;
    //log every minute
    if(totalbits % (BITS_IN_MINUTE) == 0) {
      logBits(totalbits);
    }
    //print out total seconds logged
    if(configuration.verbose) {
      logSeconds(totalbits);
    }
    //flush encoder buffer, gapless
    write = lame_encode_flush_nogap(lame, mp3_buffer, MP3_BUFFER_SIZE);
    //write mp3 to disk
    fwrite(mp3_buffer, write, 1, mp3);
    //close mp3 buffer
    fclose(mp3);
    if(breakOut > 10) {
      //leave this loop
      break;
    }
    //increment sTime
    sTime = sTime +1;
  }
  //ideally we never reach here, loop forever.  If we reach here, we should be in some
  //sort of script that restarts.  Archive forever.
  DEBUG("--- ENDING SESSION ---");
  //close lame
  lame_close(lame);
  //close pcm capture handle
  snd_pcm_close(capture_handle);
}
