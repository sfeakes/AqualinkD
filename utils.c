/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */


#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <time.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef AD_DEBUG
#include <sys/time.h>
#endif

#ifdef AQ_MANAGER
#include <systemd/sd-journal.h>
#endif

#ifndef _UTILS_C_
#define _UTILS_C_
#endif

#include "utils.h"

#define DEFAULT_LOG_FILE "/tmp/aqualinkd-inline.log"
//#define MAXCFGLINE 265
#define TIMESTAMP_LENGTH 30

// Since this get's compiled without net_services for serial_logger
// pre-define this here rather than include netservices.h
void broadcast_log(char *msg);

static bool _daemonise = false;

#ifndef AQ_MANAGER
static bool _log2file = false;
static char *_log_filename = NULL;
static bool _cfg_log2file;
static int _cfg_log_level;
#endif
static int _log_level = LOG_WARNING;

static char *_loq_display_message = NULL;
int16_t _logforcemask = 0;

//static char _log_filename[256];

#ifdef AQ_MANAGER
void setLoggingPrms(int level , bool deamonized, char *error_messages)
{
	_log_level = level;
  _daemonise = deamonized;
  _loq_display_message = error_messages;

  //_cfg_log_level = _log_level; 
}
#else
void setLoggingPrms(int level , bool deamonized, char* log_file, char *error_messages)
{
	_log_level = level;
  _daemonise = deamonized;
  _loq_display_message = error_messages;

  _cfg_log_level = _log_level;
  _cfg_log2file = _log2file;
  
  if (log_file == NULL || strlen(log_file) <= 0) {
    _log2file = false;
  } else {
    _log2file = true;
    _log_filename = log_file;
    //strcpy(_log_filename, log_file);
  }  
}
#endif // AQ_MANAGER

void setSystemLogLevel( int level)
{
  _log_level = level;
}
int getSystemLogLevel()
{
  return _log_level;
}
int getLogLevel(int16_t from)
{

  // RSSD_LOG should default to INFO unless the mask is explicitly set.
  // IE Even if DEBUG is set, (Note ignored for the moment)

  if ( from == RSSD_LOG && ((_logforcemask & from) == from ) && _log_level < LOG_DEBUG_SERIAL)
    return LOG_DEBUG_SERIAL;
  else if ( ((_logforcemask & from) == from ) && _log_level < LOG_DEBUG_SERIAL)
    return LOG_DEBUG;
  
  return _log_level;
}

#ifdef AQ_MANAGER
/*
void startInlineLog2File()
{
   _log2file = true;
  if (_log_filename == NULL)
    _log_filename = DEFAULT_LOG_FILE;
}
void stopInlineLog2File()
{
  _log2file = _cfg_log2file;
}
void cleanInlineLog2File() {
  if (_log_filename != NULL) {
    fclose(fopen(_log_filename, "w"));
  }
}
*/
#else // AQ_MANAGER
void startInlineDebug()
{
  _log_level = LOG_DEBUG;
  _log2file = true;
  if (_log_filename == NULL)
    _log_filename = DEFAULT_LOG_FILE;
}

void startInlineSerialDebug()
{
  _log_level = LOG_DEBUG_SERIAL;
  _log2file = true;
  if (_log_filename == NULL)
    _log_filename = DEFAULT_LOG_FILE;
}

void stopInlineDebug()
{
  _log_level = _cfg_log_level;
  _log2file = _cfg_log2file;
}
void cleanInlineDebug() {
  if (_log_filename != NULL) {
    fclose(fopen(_log_filename, "w"));
  }
}
char *getInlineLogFName()
{
  return _log_filename;
}

bool islogFileReady()
{
  if (_log_filename != NULL) {   
    struct stat st;
    stat(_log_filename, &st);
    if ( st.st_size > 0)
      return true;
  } 
  return false;
}
#endif // AQ_MANAGER




/*
* This function reports the last error 
*/
void LOGSystemError (int errnum, int16_t from, const char *on_what)
{
  fputs (strerror (errno), stderr);
  fputs (": ", stderr);
  fputs (on_what, stderr);
  fputc ('\n', stderr);

  if (_daemonise == TRUE)
  {
    //logMessage (LOG_ERR, "%d : %s", errno, on_what);
    LOG(AQUA_LOG, LOG_ERR, "%s (%d) : %s\n", strerror (errno), errno, on_what);
    closelog ();
  }
}
void displayLastSystemError (const char *on_what)
{
  LOGSystemError(errno, AQUA_LOG, on_what);
  /*
  fputs (strerror (errno), stderr);
  fputs (": ", stderr);
  fputs (on_what, stderr);
  fputc ('\n', stderr);

  if (_daemonise == TRUE)
  {
    //logMessage (LOG_ERR, "%d : %s", errno, on_what);
    LOG(AQUA_LOG, LOG_ERR, "%s (%d) : %s", strerror (errno), errno, on_what);
    closelog ();
  }
  */
}

/*
From -- syslog.h --
#define	LOG_EMERG	0	// system is unusable 
#define	LOG_ALERT	1	// action must be taken immediately 
#define	LOG_CRIT	2	// critical conditions 
#define	LOG_ERR		3	// error conditions 
#define	LOG_WARNING	4	// warning conditions 
#define	LOG_NOTICE	5	// normal but significant condition 
#define	LOG_INFO	6	// informational 
#define	LOG_DEBUG	7	// debug-level messages 
*/

char *elevel2text(int level) 
{
  switch(level) {
    case LOG_ERR:
      return "Error:";
      break;
    case LOG_WARNING:
      return "Warning:";
      break;
    case LOG_NOTICE:
      return "Notice:";
      break;
    case LOG_INFO:
      return "Info:";
      break;
    case LOG_DEBUG:
    default:
      return "Debug:";
      break;
  }
  
  return "";
}

int text2elevel(char* level)
{
  if (strcmp(level, "DEBUG_SERIAL") == 0) {
    return LOG_DEBUG_SERIAL;
  }
  if (strcmp(level, "DEBUG") == 0) {
    return LOG_DEBUG;
  }
  else if (strcmp(level, "INFO") == 0) {
    return LOG_INFO;
  }
  else if (strcmp(level, "WARNING") == 0) {
    return LOG_WARNING;
  }
  else if (strcmp(level, "NOTICE") == 0) {
    return LOG_NOTICE;
  }
  
  return  LOG_ERR; 
}

const char* loglevel2name(int level)
{
  if (level == LOG_DEBUG_SERIAL)
    return "Debug Serial:";

  return elevel2text(level);
}

const char* logmask2name(int16_t from)
{
  switch (from) {
    case NET_LOG:
      return "NetService:";
    break;
    case ALLB_LOG:
      return "AllButton: ";
    break;
    case RSSA_LOG:
      return "RS SAdptr: ";
    break;
    case ONET_LOG:
      return "One Touch: ";
    break;
    case IAQT_LOG:
      return "iAQ Touch: ";
    break;
    case PDA_LOG:
      return "PDA:       ";
    break;
    case DJAN_LOG:
      return "JandyDvce: ";
    break;
    case DPEN_LOG:
      return "PentaDvce: ";
    break;
    case RSSD_LOG:
      return "RS Serial: ";
    break;
    case PROG_LOG:
      return "Panl&Prog: ";
    break;
    case DBGT_LOG:
      return "DebugTimer:";
    break;
    case SCHD_LOG:
      return "Sched/Timr:";
    break;
    case RSTM_LOG:
      return "RS Timings:";
    break;
    case SIM_LOG:
      return "Simulator: ";
    break;
    case AQUA_LOG:
    default:
      return "AqualinkD: ";
    break;
  }
}

void timestamp(char* time_string)
{
    time_t now;
    struct tm *tmptr;

    time(&now);
    tmptr = localtime(&now);
    strftime(time_string, TIMESTAMP_LENGTH, "%b-%d-%y %H:%M:%S %p ", tmptr);
}

//Move existing pointer
char *cleanwhitespace(char *str)
{
  char *end;

  if (str == NULL)
    return str;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

// Return new pointer
char *stripwhitespace(char *str)
{
  // Should probably just call Trim and Chop functions.
  char *end;
  char *start = str;

  if(*start == 0 || strlen(str) <= 0 )  // All spaces?
    return str;


  // Trim leading space
  while(isspace(*start)) start++;

  if(*start == 0)  // All spaces?
    return start;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return start;
}

// Trim whispace (return new pointer) leave trailing whitespace
char *trimwhitespace(char *str)
{
  char *start = str;

  // Trim leading space
  while(isspace(*start)) start++;

  if(*start == 0)  // All spaces?
    return start;

  return start;
}


char *chopwhitespace(char *str)
{
  char *end;
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  
  // Write new null terminator
  *(end+1) = 0;

  return str;
}


/*
char *cleanquotes(char *str)
{
  char *end;
  // Trim leading whitespace
  //while(isspace(*str)) str++;
  //if(*str == 0)  // All spaces?
  //  return str;
syslog(LOG_INFO, "String to clean %s\n", str);

  while(*str=='"' || *str== '\'' || *str==' ') str++;
  if(*str == 0)  // All spaces
    return str;

  end = str + strlen(str) - 1;
  while(end > str && (*end=='"' || *end== '\'' || *end==' ')) end--;

  // Write new null terminator
  *(end+1) = 0;

syslog(LOG_INFO, "String cleaned %s\n", str);

  return str;
}
*/
int cleanint(char*str)
{
  if (str == NULL)
    return 0;
    
  str = cleanwhitespace(str);
  return atoi(str);
}

/*
void test(int msg_level, char *msg)
{
  char buffer[256];
  
  sprintf(buffer,"Level %d | MsgLvl %d | Dmn %d | LF %d | %s - %s",_log_level,msg_level,_daemonise,_log2file,_log_filename,msg);
  if ( buffer[strlen(buffer)-1] != '\n') {
    strcat(buffer, "\n");
  } 
  
  int fp = open("/var/log/aqualink.log", O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fp != -1) {
    write(fp, buffer, strlen(buffer) );
    close(fp);  
  } else {
    syslog(LOG_ERR, "Can't open file /var/log/aqualink.log/n");
  }
}
*/

void addDebugLogMask(int16_t flag)
{
  _logforcemask |= flag;
}

void removeDebugLogMask(int16_t flag)
{
  _logforcemask &= ~flag;
}

void clearDebugLogMask()
{
  _logforcemask = 0;
}

bool isDebugLogMaskSet(int16_t flag)
{
  return _logforcemask & flag;
}

void _LOG(int16_t from, int msg_level, char * message);

/*
void logMessage(int msg_level, const char *format, ...)
{
  if (msg_level > _log_level) {
    return;
  }

  char buffer[1024];
  va_list args;
  va_start(args, format);
  strncpy(buffer, "         ", 20);
  vsprintf (&buffer[20], format, args);
  va_end(args);

  _LOG(AQUA_LOG, msg_level, buffer);
}
*/

void LOG(int16_t from, int msg_level, const char * format, ...)
{
  //printf("msg_level=%d _log_level=%d mask=%d\n",msg_level,_log_level,(_logforcemask & from));
  /*
  if (msg_level > _log_level && ((_logforcemask & from) == 0) ) { // from NOT set in _logforcemask
    return;
  }
  */
  if ( msg_level > getLogLevel(from))
    return;

  char buffer[LOGBUFFER];
  va_list args;
  va_start(args, format);
  strncpy(buffer, "         ", 20);
  //vsprintf (&buffer[20], format, args);
  int size = vsnprintf (&buffer[20], LOGBUFFER-30, format, args);
  va_end(args);
  if (size >= LOGBUFFER-30 ) {
    sprintf(&buffer[LOGBUFFER-11], ".........\n");
  }

  _LOG(from, msg_level, buffer);
}


void _LOG(int16_t from, int msg_level,  char *message)
{
  int i;

  // Make all printable chars
  for(i = 8; i+8 < strlen(&message[8]) && i < LOGBUFFER; i++) {
    if ( (message[i] < 32 || message[i] > 125) && message[i] != 10 ) {
      //printf ("Change %c to %c in %s\n",message[i], ' ', message);
      message[i] = ' ';
    }
  }
  // Add return to end of string if not already their.
  // NSF need to come back to this, doesn;t always work
  if (message[i] != '\n') {
    message[i] = '\n';
    message[i+1] = '\0';
  }

  //strncpy(&message[9], logmask2name(from), 11); // Will give compiller warning as doesn;t realise we are copying into middle of string.
  memcpy(&message[9], logmask2name(from), 11);

  // Logging has not been setup yet, so STD error & syslog
  if (_log_level == -1) {
    fprintf (stderr, "%s\n", message);
    syslog (msg_level, "%s", &message[9]);
    closelog ();
  }
  
  #ifdef AQ_MANAGER // Always use systemd journel with aqmanager
    //sd_journal_print()
    //openlog("aqualinkd", 0, LOG_DAEMON);
    if (msg_level > LOG_DEBUG)  // Let's not confuse syslog with custom levels
      sd_journal_print (LOG_DEBUG, "%s", &message[9]);
      //sd_journal_print_with_location(LOG_DEBUG, "aqualinkd", "%s", &message[9]);
    else
      sd_journal_print (msg_level, "%s", &message[9]);
      //sd_journal_print_with_location(msg_level, "aqualinkd", "%s", &message[9]);
    //closelog ();
  #else
  if (_daemonise == TRUE)
  {
    if (msg_level > LOG_DEBUG)  // Let's not confuse syslog with custom levels
      syslog (LOG_DEBUG, "%s", &message[9]);
    else
      syslog (msg_level, "%s", &message[9]);
    closelog ();
    //return;
  }
  #endif //AQ_MANAGER
  
  //int len;
  message[8] = ' ';
  char *strLevel = elevel2text(msg_level);
  //strncpy(message, strLevel, strlen(strLevel)); // Will give compiler warning, so use memcpy instead
  memcpy(message, strLevel, strlen(strLevel));
  //len = strlen(message); 
  /*
  if ( message[len-1] != '\n') {
    strcat(message, "\n");
  }
  */

  // Superceded systemd/sd-journal
  //with Send logs to any websocket that's interested.
  //broadcast_log(message);

  // Sent the log to the UI if configured.
  if (msg_level <= LOG_ERR && _loq_display_message != NULL) {
    snprintf(_loq_display_message, 127, "%s\n",message);
  }

#ifndef AQ_MANAGER
  if (_log2file == TRUE && _log_filename != NULL) {   
    char time[TIMESTAMP_LENGTH];
    int fp = open(_log_filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fp != -1) {
      timestamp(time);
      if ( write(fp, time, strlen(time) ) == -1 ||
           write(fp, message, strlen(message) ) == -1 ) 
      {
        syslog(LOG_ERR, "Can't write to log file %s\n %s", _log_filename, message);
        fprintf (stderr, "Can't write to log file %s\n %s", _log_filename, message);
      }
      //write(fp, time, strlen(time) );
      //write(fp, message, strlen(message) );
      close(fp);
    } else {
      if (_daemonise == TRUE)
        syslog(LOG_ERR, "Can't open log file %s\n %s", _log_filename, message);
      else
        fprintf (stderr, "Can't open debug log %s\n %s", _log_filename, message);
    }
  }
#endif //AQ_MANAGER

  if (_daemonise == FALSE) {
    if (msg_level == LOG_ERR) {
      fprintf(stderr, "%s", message);
    } else {
#ifndef AD_DEBUG
      printf("%s", message);
#else
      struct timespec tspec;
      struct tm localtm;
      clock_gettime(CLOCK_REALTIME, &tspec);
      char timeStr[TIMESTAMP_LENGTH];
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime_r(&tspec.tv_sec, &localtm));
      printf("%s.%03ld %s", timeStr, tspec.tv_nsec / 1000000L, message);
#endif
    }
  }
}

void daemonise (char *pidFile, void (*main_function) (void))
{
  FILE *fp = NULL;
  pid_t process_id = 0;
  pid_t sid = 0;

  _daemonise = true;
  
  /* Check we are root */
  if (getuid() != 0)
  {
    LOG(AQUA_LOG, LOG_ERR,"Can only be run as root\n");
    exit(EXIT_FAILURE);
  }

  int pid_file = open (pidFile, O_CREAT | O_RDWR, 0666);
  int rc = flock (pid_file, LOCK_EX | LOCK_NB);
  if (rc)
  {
    //if (EWOULDBLOCK == errno)
    //; // another instance is running
    //fputs ("\nAnother instance is already running\n", stderr);
    LOG(AQUA_LOG, LOG_ERR,"\nAnother instance is already running\n");
    exit (EXIT_FAILURE);
  }

  process_id = fork ();
  // Indication of fork() failure
  if (process_id < 0)
  {
    displayLastSystemError ("fork failed!");
    // Return failure in exit status
    exit (EXIT_FAILURE);
  }
  // PARENT PROCESS. Need to kill it.
  if (process_id > 0)
  {
    fp = fopen (pidFile, "w");

    if (fp == NULL)
      LOG(AQUA_LOG, LOG_ERR,"can't write to PID file %s",pidFile);
    else
      fprintf(fp, "%d", process_id);

    fclose (fp);
    LOG(AQUA_LOG, LOG_DEBUG, "process_id of child process %d \n", process_id);
    // return success in exit status
    exit (EXIT_SUCCESS);
  }
  //unmask the file mode
  umask (0);
  //set new session
  sid = setsid ();
  if (sid < 0)
  {
    // Return failure
    displayLastSystemError("Failed to fork process");
    exit (EXIT_FAILURE);
  }
  // Change the current working directory to root.
  if ( chdir ("/") == -1) {
    LOG(AQUA_LOG, LOG_ERR,"Can't set working dir to /");
  }
  // Close stdin. stdout and stderr
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);

  // this is the first instance
  (*main_function) ();

  return;
}

int count_characters(const char *str, char character)
{
    const char *p = str;
    int count = 0;

    do {
        if (*p == character)
            count++;
    } while (*(p++));

    return count;
}

bool text2bool(char *str)
{
  str = cleanwhitespace(str);
  if (strcasecmp (str, "YES") == 0 || strcasecmp (str, "ON") == 0)
    return TRUE;
  else
    return FALSE;
}

bool request2bool(char *str)
{
  str = cleanwhitespace(str);
  if (strcasecmp (str, "YES") == 0 || strcasecmp (str, "ON") == 0 || atoi(str) == 1)
    return TRUE;
  else
    return FALSE;
}

char *bool2text(bool val)
{
  if(val == TRUE)
    return "YES";
  else
    return "NO";
}

// (50°F - 32) x .5556 = 10°C
float degFtoC(float degF)
{
  return ((degF-32) / 1.8);
}
// 30°C x 1.8 + 32 = 86°F 
float degCtoF(float degC)
{
  return (degC * 1.8 + 32);
}


float timespec2float(const struct timespec *elapsed) {
  return ((float)elapsed->tv_nsec / 1000000000L) + elapsed->tv_sec;
}

#include <time.h>

void delay (unsigned int howLong) // Microseconds (1000000 = 1 second) miliseconds
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}
/*
// Same as above but can pass 0.5
void ndelay (float howLong) // Microseconds (1000000 = 1 second) 
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)((howLong*10) / 10000) ;
  sleeper.tv_nsec = (long)( ((int)(howLong*10)) % 10000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}
*/
char* stristr(const char* haystack, const char* needle) {
  do {
    const char* h = haystack;
    const char* n = needle;
    while (tolower((unsigned char) *h) == tolower((unsigned char ) *n) && *n) {
      h++;
      n++;
    }
    if (*n == 0) {
      return (char *) haystack;
    }
  } while (*haystack++);
  return 0;
}
/*
int ascii(char *destination, char *source) {
  unsigned int i;
  for(i = 0; i < strlen(source); i++) {
    //if ( source[i] >= 0 && source[i] < 128 )
    if ( source[i] >= 0 && source[i] < 127 )
      destination[i] = source[i];
    else
      destination[i] = ' ';

    if ( source[i]==126 ) {
      destination[i-1] = '<';
      destination[i] = '>';
    } 

  }
  destination[i] = '\0';
  return i;
}
*/
char *prittyString(char *str)
{
  char *ptr = str;
  char *end;
  bool lastspace=true;

  end = str + strlen(str) - 1;
  while(end >= ptr){
    //printf("%d %s ", *ptr, ptr);
    if (lastspace && *ptr > 96 && *ptr < 123) {
      *ptr = *ptr - 32;
      lastspace=false;
      //printf("to upper\n");
    } else if (lastspace == false && *ptr > 54 && *ptr < 91) {
      *ptr = *ptr + 32;
      lastspace=false;
      //printf("to lower\n");
    } else if (*ptr == 32) {
      lastspace=true;
      //printf("space\n");
    } else {
      lastspace=false;
      //printf("leave\n");
    }
    ptr++;
  }

  //printf("-- %s --\n", str);

  return str;
}

