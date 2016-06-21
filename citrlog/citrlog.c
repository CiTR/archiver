/** 
 * audio logger
 * (C) 2013, for the university of british columbia radio society.
 * Bugs, comments: robin@ubicee.net 
 */


/**
 *TODO:
 *
 * ** Logs 10 seconds of audio at a time, to hopefully reduce the effect of lag to 7.5s a day
 * General cleanup
 * make sure time is synchronized nicely
 * modularize so that easy output to multiple locations possible.
 */

// Includes
#define _GNU_SOURCE  
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
#include <sched.h>

// google ini lib
#include "ini.h"

// Macros
#define DEBUG(x) logs(x);
#define MP3_BUFFER_SIZE 65536
#define SECONDS_TO_RECORD 20

//definitions of bit-lengths of various spans of time.
#define BITS_IN_SECOND (long)(16 * 44100)
#define BITS_IN_MINUTE (long)(BITS_IN_SECOND * 60)
#define BITS_IN_HOUR (long)(BITS_IN_MINUTE * 60)
#define BITS_IN_DAY (long)(BITS_IN_HOUR * 24)

// Termination Status Enumeration
typedef enum _term_t 
{
 	SESSION_TERM = 1,
 	RUN_TERM = 2
} term_t;

// Thread Type Enumeration
typedef enum _thread_t
{
 	DATA_THREAD = 1,
 	PROCESS_THREAD = 2
} thread_t;

// Data holder for current time segment
typedef struct _data_t
{
 	short *buffer;
 	struct timespec *ts;
 	time_t time;
 	struct _data_t *pNext;
 	int seconds;
} data_t;

// Configuration data structure
typedef struct _conf_t
{
	//Audio Configuration
 	char *pAudioLogBase;
 	char *pLogFile;
 	FILE *logFileHandle;
 	char *pDetach;
 	char *pDevice;
 	int sampleRate;
 	int mp3Rate;
 	int verbose;
 	int bufferSize;

 	//Data pointers
 	char *pConfigLocation;
 	data_t *pDataRoot;
 	data_t *pDataHead;

 	//Thread Configuration
 	term_t terminated;
 	pthread_mutex_t dataMutex;
 	pthread_mutex_t logMutex;
 	pthread_mutex_t threadMutex;
 	pthread_cond_t dataCond;
 	pthread_cond_t threadCond;
 	thread_t threadStatus;
 	pthread_t dataThread;
 	pthread_t processThread;
 	double startTime;
 	int totalSeconds;
 	int secondsWaiting;
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

//seconds to midnight
int getSecondsToMidnight();

//debug
double getTimeInMS()
{
 	struct timeval  tv;
 	gettimeofday(&tv, NULL);
 	return  (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
}

double getTimeInUS()
{
 	struct timeval  tv;
 	gettimeofday(&tv, NULL);
 	return  (tv.tv_sec) * 1000000 + (tv.tv_usec);
}

//function implementations

int getSecondsToMidnight()
{

	time_t now;
	time_t midnight;
	struct tm ctm;
	struct tm mtm;

	//get current time in seconds
	time(&now);

	//convert current time into tm struct
	localtime_r(&now, &ctm);
	localtime_r(&now, &mtm);
	mtm.tm_sec = 0;
	mtm.tm_min = 0;
	mtm.tm_hour = 12;

	midnight = mktime(&mtm);
	//get time at midnight today in seconds
	//get difference between times
	return (int)difftime(midnight, now);
}

 void dataPush(short *buf, struct timespec *ts, time_t time, int seconds)
 {
 	data_t *pTemp;
 	data_t *pNew = (data_t *)malloc(sizeof(data_t));

 	pNew->buffer = buf;
 	pNew->ts = ts;
 	pNew->time = time;
 	pNew->pNext = NULL;
 	pNew->seconds = seconds;

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
	struct sched_param sp;
	cpu_set_t cpuset;
	res = pthread_create(&pConf->dataThread, NULL, dataThreadFunction, (void *)NULL);
	sp.sched_priority = 99;
	pthread_setschedparam(pConf->dataThread, SCHED_FIFO, &sp);
	CPU_ZERO(&cpuset);
  	CPU_SET(1, &cpuset); //use CPU 1 for data thread
  	pthread_setaffinity_np(pConf->dataThread, sizeof(cpu_set_t), &cpuset);
}

void createProcessThread(conf_t *pConf)
{
	int res;
	struct sched_param sp;
	cpu_set_t cpuset;
	res = pthread_create(&pConf->processThread, NULL, processThreadFunction, (void *)NULL);
	sp.sched_priority = 0;
	pthread_setschedparam(pConf->dataThread, SCHED_OTHER, &sp);
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset); //use CPU 0 for data thread
	pthread_setaffinity_np(pConf->dataThread, sizeof(cpuset), &cpuset);
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
 	
 	configuration.pConfigLocation = "config.ini";

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
		createThreads(&configuration);
		logSession();
		pthread_join(configuration.processThread, NULL); 
	    //never should occur.
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

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0) {
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

 	//  if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
	if ((err = snd_pcm_hw_params_set_rate (capture_handle, hw_params, rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			snd_strerror (err));
		exit (1);
	}
	DEBUG("Set Rate.");
	logsi("Set rate to ", rate);
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
	long long bitCount = 0;
	long long totalbits = 0;
	int err;
	short *buf;
	snd_pcm_t *capture_handle;
	int breakOut = 0;
	data_t *node;
	struct timespec *ts;
	struct timespec start;
	struct timespec current;
	long msdiff;
	long sdiff;
	long nsdiff;
	double dST;
	double dCT;
	time_t startTime;
	time_t currentTime;
	time_t debugTime;
	int secondsRemaining;
	int totalSeconds = 0;
	char sb[512];
	char msg[512];
	snd_pcm_sframes_t delayF;
	int i = 0;

	time(&startTime);
	dST = getTimeInUS();
	currentTime = startTime;
	secondsRemaining = getSecondsToMidnight();
	clock_gettime( CLOCK_REALTIME, &start);
	configuration.startTime = dST;
	configuration.totalSeconds = 0;
	initSignals();
	DEBUG("Data thread running...");

	// Ininiate handle from preferred device
	capture_handle = initCapture(configuration.pDevice);

	totalbits = 0;
	configuration.secondsWaiting = 0;
	//grab start time                                                                                                     
	//time(&sTime);
	//initialize buffer                                                                                    
	//new method:
	//on entry into function, get current time, t
	//therefore, if it is t seconds into the day, we should record for
	//86400 - t seconds, then another 86400 seconds for another day, etc.
	
	// Loop by the second
	while(bitCount < (BITS_IN_SECOND)) {
	    // buf = (short*)malloc(sizeof(short) * (configuration.bufferSize*2));
    	// Multiply by 10 to read 10x
		buf = (short*)malloc(sizeof(short) * (configuration.bufferSize*2) * SECONDS_TO_RECORD);
		ts = (struct timespec *)malloc(sizeof(struct timespec));
		clock_gettime( CLOCK_REALTIME, ts);

		if ((err = snd_pcm_mmap_readi(capture_handle, buf, (configuration.bufferSize*SECONDS_TO_RECORD))) != (configuration.bufferSize*SECONDS_TO_RECORD)) {
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
			snd_pcm_delay(capture_handle, &delayF);
			sprintf(msg, "Delay is: %d", (int)delayF);
			logs(msg);
			sprintf(msg, "Completed read, %d left in buffer.", (int)snd_pcm_avail_update(capture_handle));
			logs(msg);
			breakOut = 0;
			// SHORT_BUFFER_SIZE 16 bit samples added                                                                    DE   
			// bitCount = bitCount + (configuration.bufferSize * 16);
			// data to queue
			pthread_mutex_lock(&configuration.dataMutex);
			//add new node to data root.
			//process the ten seconds.
			//      for(i = 0; i < 10; i++) {
			dataPush(buf, ts, currentTime, SECONDS_TO_RECORD);
			configuration.secondsWaiting = configuration.secondsWaiting + SECONDS_TO_RECORD;
			//increment current time
			currentTime = currentTime+SECONDS_TO_RECORD;
			//after this, do not use buf.
			//add a total second
			totalSeconds = totalSeconds + SECONDS_TO_RECORD;
			configuration.totalSeconds = configuration.totalSeconds + SECONDS_TO_RECORD;
			//log total seconds
			time(&debugTime);
			dCT = getTimeInUS();
			clock_gettime( CLOCK_REALTIME, &current);
			sdiff = current.tv_sec - start.tv_sec;
			nsdiff = current.tv_nsec - start.tv_nsec;

			sprintf(sb, "Logged %d seconds in %ld us (%ld us proc per second, approx %ld us lag)",
			totalSeconds,
			(long)(dCT - dST),
			(((long)(dCT - dST))/totalSeconds),
			((((long)(dCT - dST))/totalSeconds) - 1000000) * totalSeconds);
			//   (((long)(dCT - dST) - ((long)1000000 * SECONDS_TO_RECORD))/totalSeconds));
			// - (1000000*SECONDS_TO_RECORD))/SECONDS_TO_RECORD);
			/*      sprintf(sb, "Logged %d seconds in %d s (avg of %f per second)", 
			totalSeconds, 
			(int)(debugTime - startTime),
			(float)((debugTime - startTime)/(float)totalSeconds));
			*/
			logs(sb);
			pthread_mutex_unlock(&configuration.dataMutex);
			//reset bitCount
			//bitCount = 0;
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
	sprintf(*mp3File,"%s/%d-second.mp3",mkdaydir(timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday),(int)s);
	return *mp3File;
}

void *processThreadFunction(void *threadid)
{
	short *buffer = NULL;
  	//time_t sTime;
	struct tm timeinfo;
	FILE *mp3;
	lame_t lame;
	int write;
	unsigned char mp3_buffer[MP3_BUFFER_SIZE];
	data_t *data;
	char mp3file[256];
	struct stat status;
	double FT;
	char msg[512];
	char msgg[512];
	int i;

	initSignals();
	DEBUG("Process thread running...");
	lame = initLAME();

	while(true) {		
    	//wait for data to be available
		pthread_cond_wait(&configuration.dataCond, &configuration.dataMutex);
    	//we have data
		data = dataPop();
		while(data != NULL) {
			for(i = 0; i < data->seconds; i++) {
				sprintf(msg, "we have %d seconds waiting.", configuration.secondsWaiting);
				configuration.secondsWaiting = configuration.secondsWaiting - 1;
				logs(msg);
				pthread_mutex_unlock(&configuration.dataMutex);

				mp3file[0] = (char)0;
				// parse time into useful information                                                                             
				// timeinfo = localtime(&data->ts->tv_sec);
				localtime_r(&data->time, &timeinfo);
				//increment time
  				//test each mp3 filename to see if it exists. If it does, try next.
				while(mp3file[0] == (char)0) {
					//sprintf(mp3file, "%s/%d-second.mp3", mkdaydir(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday), (int)data->time);
					//open file handle     
					if(stat(mp3file, &status) != -1) { // file exists.
						data->ts->tv_sec = data->ts->tv_sec + 1;
						logsi("WOULD HAVE OVERWRITTEN A FILE!", (int)data->ts->tv_sec);
						continue;
					}
					//otherwise, we have a good filename.   
				}
				//open file handle
				mp3 = fopen(mp3file, "wb");
				//encode
				//  write = lame_encode_buffer_interleaved(lame, data->buffer, configuration.bufferSize, mp3_buffer, MP3_BUFFER_SIZE);
				write = lame_encode_buffer_interleaved(lame, 
				data->buffer + (configuration.bufferSize*i*2), 
				configuration.bufferSize, 
				mp3_buffer, MP3_BUFFER_SIZE);
				//DEBUG: write pcm files to disk!
				//write mp3 to disk                                                                                             
				fwrite(mp3_buffer, write, 1, mp3);
				//logsi("wrote mp3 bytes: ", write);
				//encoder flush
				write = lame_encode_flush_nogap(lame, mp3_buffer, MP3_BUFFER_SIZE);
				//write flushed mp3 buffer to disk                                                                              
				fwrite(mp3_buffer, write, 1, mp3);
				//logsi("wrote mp3 bytes: ", write);
				//close mp3
				fclose(mp3);

				//debug before freeing
				//DEBUG
				// strftime(msg, sizeof(msg), "%D %T", &timeinfo);

				// sprintf(msgg, "Logged %s (%d)", msg, (int)data->time);
				logs(msgg);

				data->time = data->time + 1;
				//increment time.
				//free data
				//free(data->buffer);
				//free data->frameBuffer if we have done ten
			} //ENDFOR

			free(data->buffer);
			free(data->ts);
			free(data);

			//sTime = sTime + 1;
			//get our next data item.  If NULL, we wait for a signal.
			pthread_mutex_lock(&configuration.dataMutex);
			data = dataPop();
			pthread_mutex_unlock(&configuration.dataMutex);

		} //ENDWHILE
	}
	//This code is unreachable.
	lame_close(lame);
	//signal end
	configuration.threadStatus = PROCESS_THREAD;
	pthread_cond_signal(&configuration.threadCond);
}    
