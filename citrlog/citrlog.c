/** 
 * citrlog
 * Bugs, comments: robin@ubicee.net 
 */

/**
Copyright (c) 2013, Robin Tavender
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the Student Radio Society of The University of British Columbia nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/**
 *TODO:
 *
 * modularize so that easy output to multiple locations possible.
 */

// Includes

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
#include <signal.h>
#include <pthread.h>

// google ini lib
#include "ini.h"

// Macros

#define DEBUG(x) logs(x);

#define MP3_BUFFER_SIZE 65536

//definitions of bit-lengths of various spans of time.
#define BITS_IN_SECOND (long)(16 * 44100)
#define BITS_IN_MINUTE (long)(BITS_IN_SECOND * 60)
#define BITS_IN_HOUR (long)(BITS_IN_MINUTE * 60)
#define BITS_IN_DAY (long)(BITS_IN_HOUR * 24)

// Enumerations

typedef enum _term_t 
  {
    SESSION_TERM = 1,
    RUN_TERM = 2
  } term_t;

typedef enum _thread_t
  {
    DATA_THREAD = 1,
    PROCESS_THREAD = 2
  } thread_t;

//data structures
typedef struct _data_t
{
  short *buffer;
  struct timespec *ts;
  struct _data_t *pNext;
} data_t;

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
  char *pConfigLocation;
  term_t terminated;
  data_t *pDataRoot;
  data_t *pDataHead;
  //thread stuff
  pthread_mutex_t dataMutex;
  pthread_mutex_t logMutex;
  pthread_mutex_t threadMutex;
  pthread_cond_t dataCond;
  pthread_cond_t threadCond;
  thread_t threadStatus;
  pthread_t dataThread;
  pthread_t processThread;
} conf_t;

conf_t configuration;
char dirbuf[1024];


//function prototypes

char* mkyeardir(int year);
char* mkmonthdir(int year, int month);
char* mkdaydir(int year, int month, int day);
int start(int argc, char *argv[]);
int handler(void* user, const char* section,const char* name, const char* value);
void logs(char *str);
void logsi(char *str, int i);
int setAttribute(char **pPAttr, const char *str);
void logBits(long long bytes);
int isDetached();
snd_pcm_t* initCapture(char * dev);
lame_t initLAME();
void initConfiguration(conf_t *pConf);
void createThreads(conf_t *pConf);
//thread functions
void *dataThreadFunction(void *threadid);
void *processThreadFunction(void *threadid);

void creatProcessThread(conf_t *pConf);
void creatDataThread(conf_t *pConf);
void createThreads(conf_t *pConf);

//signal handlers
void sigterm_handler(int sig);
void sigusr1_handler(int sig);
void sighup_handler(int sig);

//function implementations

void dataPush(short *buf, struct timespec *ts){
  data_t *pTemp;
  data_t *pNew = (data_t *)malloc(sizeof(data_t));

  pNew->buffer = buf;
  pNew->ts = ts;
  pNew->pNext = NULL;

  if(configuration.pDataRoot == NULL) {
    configuration.pDataRoot = pNew;
    configuration.pDataHead = pNew; //head and root are same
  }
  else {
    configuration.pDataHead->pNext = pNew;
  }
  configuration.pDataHead = pNew;
}

data_t *dataPop()
{
  data_t *pRet = NULL;
  data_t * pTemp;

  if(configuration.pDataRoot == NULL) {
    return NULL;
  }
  //we return the data from the current Root
  pRet = configuration.pDataRoot;
  //we store the current root in a temporary variable
  pTemp = configuration.pDataRoot;
  //we set the root to be the current root's next element.
  configuration.pDataRoot = configuration.pDataRoot->pNext;
  //we free the old root.
  //this is freed later on now.
  //free(pTemp);
  return pRet;
}

void sigterm_handler(int sig)
{
  if(sig == SIGTERM) {
    DEBUG("Received SIGTERM, ending session and run.");
    configuration.terminated = RUN_TERM | SESSION_TERM;
  }
}

void sigusr1_handler(int sig)
{
  //restart session.
  if(sig == SIGUSR1) {
    DEBUG("Received SIGUSR1, restarting session.");
    configuration.terminated = SESSION_TERM;
  }
}
void rereadConfig()
{
  DEBUG("Rereading config.");
  kill(getpid(), SIGHUP);
}

void restartSession()
{
  kill(getpid(), SIGUSR1);
}

void sighup_handler(int sig)
{
  if(sig == SIGHUP) {
    DEBUG("Received SIGHUP, reread configuration");
    ini_parse(configuration.pConfigLocation,
	      handler,
	      (void*)&configuration);
  }
}


int isDetached()
{
  return strcmp(configuration.pDetach, "true") == 0;
}

void createDataThread(conf_t *pConf)
{
  int res;
  res = pthread_create(&pConf->dataThread,
                       NULL,
                       dataThreadFunction,
                       (void *)NULL);

}

void createProcessThread(conf_t *pConf)
{
  int res;
  res = pthread_create(&pConf->processThread,
                       NULL,
                       processThreadFunction,
                       (void *)NULL);

}


void createThreads(conf_t *pConf)
{
  createDataThread(pConf);
  createProcessThread(pConf);
  //add check for res.
}

void initSignals()
{
  //set up signals
  signal(SIGTERM, sigterm_handler);
  signal(SIGHUP, sighup_handler);
  signal(SIGUSR1, sigusr1_handler);
}

void initConfiguration(conf_t *pConf)
{
  memset((void*) pConf, 0, sizeof(conf_t));
  //set up signals
  initSignals();
  //set up threads
  pthread_mutex_init(&pConf->dataMutex, NULL);
  pthread_mutex_init(&pConf->logMutex, NULL);
  pthread_cond_init(&pConf->dataCond, NULL);
  pthread_cond_init(&pConf->threadCond, NULL);
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

void logSession()
{
  char buf[256];
  sprintf(buf, "Started New Session, PID: %d, PPID: %d", getpid(), getppid());
  logs(buf);
}

void logsi(char *str, int i)
{
  char *pBuf = (char *)malloc(sizeof(char) * (strlen(str) + 16));
  sprintf(pBuf, "%s%d", str, i);
  logs(pBuf);
  free(pBuf);

}

void logs(char *str)
{
  char logString[1024];
  char timeString[128];
  time_t ts = time(&ts);
  struct tm *tmm = localtime(&ts);
  strftime(timeString, sizeof(timeString), "%D %T", tmm);
  pthread_mutex_lock(&configuration.logMutex);

  if(configuration.logFileHandle != NULL) {    
    sprintf(logString, "%s: %s\n%%s", timeString, str);
    fprintf(configuration.logFileHandle, logString, "");
    fflush(configuration.logFileHandle);
    if(!isDetached()) {
      printf(logString, "");
    }
  }
  pthread_mutex_unlock(&configuration.logMutex);
}

/**
 * This function fills our configuration with data from 
 * config.ini by default, or some other file.
 * 
 */
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
  pid_t pID;
  initConfiguration(&configuration);
  if(argc >= 2) {
    configuration.pConfigLocation = argv[1];
  }
  else {
    configuration.pConfigLocation = "config-test40.ini";
  }
  rereadConfig();
  logSession();
  if(isDetached()) {
    DEBUG("Detaching...");
    //detach
    pID = fork();
    if(pID == 0) { //child
      logSession();
      createThreads(&configuration);
      while(1) {
	//wait for a thread to terminate
	pthread_cond_wait(&configuration.threadCond, &configuration.threadMutex);
	//this means we have a thread ended.
	//restart it
	if(configuration.threadStatus == DATA_THREAD) {
	  createDataThread(&configuration);
	  //relaunch Data thread
	}
	else if(configuration.threadStatus == PROCESS_THREAD) {
	  createProcessThread(&configuration);
	  //relaunch process thread
	}
      }
    }
    
    else if(pID < 0) { //failure
      DEBUG("ERROR Forking");
    }
    else { //parent
      //have parent wait for 
      DEBUG("CiTR Archiver. Detached.");
    }
  }
  else {
    //
    createThreads(&configuration);
    logSession();
    pthread_join(configuration.processThread, NULL); 
    //never should occur.
  }
  DEBUG("Exiting...");
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

void *dataThreadFunction(void *threadid)
{
  //variables                                                                                                          
  long long bitCount = 0;
  long long totalbits = 0;
  int err;
  short *buf;
  snd_pcm_t *capture_handle;
  //time_t sTime;
  int breakOut = 0;
  data_t *node;
  struct timespec *ts;

  initSignals();
  DEBUG("Data thread running...");

  capture_handle = initCapture(configuration.pDevice);

  totalbits = 0;
  //grab start time                                                                                                     
  //time(&sTime);
  //initialize buffer                                                                                                  
  while(bitCount < (BITS_IN_SECOND)) {

    buf = (short*)malloc(sizeof(short) * (configuration.bufferSize*2));
    ts = (struct timespec *)malloc(sizeof(struct timespec));
    clock_gettime( CLOCK_REALTIME, ts);
    if ((err = snd_pcm_readi(capture_handle, buf, configuration.bufferSize)) != configuration.bufferSize) {
      if(err == -EPIPE) {
	DEBUG("Buffer Overrun");
	if(snd_pcm_prepare(capture_handle) == 0) {
	  DEBUG("Prepared capture_handle for more use...");
	}
	free(buf);
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
	restartSession();
      }
    }
    else {
      //if we read data, reset breakOut                                              

      breakOut = 0;
      // SHORT_BUFFER_SIZE 16 bit samples added                                                                    DE   
      bitCount = bitCount + (configuration.bufferSize * 16);
      // data to queue
      pthread_mutex_lock(&configuration.dataMutex);
      //add new node to data root.
      dataPush(buf,ts);
      //after this, do not use buf.
      pthread_mutex_unlock(&configuration.dataMutex);
      //reset bitCount
      bitCount = 0;
      //signal that we have data
      pthread_cond_signal(&configuration.dataCond);
    }
  }
  //close pcm capture handle                                                  
  snd_pcm_close(capture_handle);
  //signal end
  configuration.threadStatus = DATA_THREAD;
  pthread_cond_signal(&configuration.threadCond);
}

char *getMP3FileName(time_t s, char **mp3File)
{
  struct tm * timeinfo;
  timeinfo = localtime(&s);
  //generate file name                                                                                                
  sprintf(*mp3File,
	  "%s/%d-second.mp3",
	  mkdaydir(timeinfo->tm_year+1900,
		   timeinfo->tm_mon+1,
		   timeinfo->tm_mday),
	  (int)s);
  return *mp3File;
}

void *processThreadFunction(void *threadid)
{
  short *buffer = NULL;
  //time_t sTime;
  struct tm * timeinfo;
  FILE *mp3;
  lame_t lame;
  int write;
  unsigned char mp3_buffer[MP3_BUFFER_SIZE];
  data_t *data;
  char mp3file[256];

  initSignals();
  DEBUG("Process thread running...");
  lame = initLAME();

  //grab start time 
  //time(&sTime);
  while(1) {
    //wait for data to be available
    pthread_cond_wait(&configuration.dataCond, &configuration.dataMutex);
    //we have data
    data = dataPop();
    pthread_mutex_unlock(&configuration.dataMutex);
    while(data != NULL) {
      //parse time into useful information                                                                             
      timeinfo = localtime(&data->ts->tv_sec);
      //generate file name                                                                                            
      sprintf(mp3file,
	      "%s/%d-second.mp3",
	      mkdaydir(timeinfo->tm_year+1900,
		       timeinfo->tm_mon+1,
		       timeinfo->tm_mday),
	      (int)data->ts->tv_sec);
      //open file handle                                                                                               
      mp3 = fopen(mp3file, "wb");
      //encode
      write = lame_encode_buffer_interleaved(lame, data->buffer, configuration.bufferSize, mp3_buffer, MP3_BUFFER_SIZE);
      //write mp3 to disk                                                                                             
      fwrite(mp3_buffer, write, 1, mp3);
      logsi("wrote mp3 bytes: ", write);
      //encoder flush
      write = lame_encode_flush_nogap(lame, mp3_buffer, MP3_BUFFER_SIZE);
      //write flushed mp3 buffer to disk                                                                              
      fwrite(mp3_buffer, write, 1, mp3);
      logsi("wrote mp3 bytes: ", write);
      //close mp3
      fclose(mp3);
      //free data
      free(data->buffer);
      free(data->ts);
      free(data);
      //sTime = sTime + 1;
      //get our next data item.  If NULL, we wait for a signal.
      pthread_mutex_lock(&configuration.dataMutex);
      data = dataPop();
      pthread_mutex_unlock(&configuration.dataMutex);
    }
  }
  //ideally we never get here.
  lame_close(lame);
  //signal end
  configuration.threadStatus = PROCESS_THREAD;
  pthread_cond_signal(&configuration.threadCond);
}    

/****
 *Version 1.0
 *Dec. 25, 2013
 ****/
