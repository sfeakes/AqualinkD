#include <syslog.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifndef UTILS_H_
#define UTILS_H_

#define LOG_DEBUG_SERIAL LOG_DEBUG+1

#ifndef EXIT_SUCCESS
  #define EXIT_FAILURE 1
  #define EXIT_SUCCESS 0
#endif

#ifndef TRUE
  #define TRUE 1
  #define FALSE 0
#endif

#define LOGBUFFER 256

//#define MAXLEN 256

//#define round(a) (int) (a+0.5) // 0 decimal places (doesn't work for negative numbers)
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))  
#define roundf(a) (float) ((a*100)/100) // 2 decimal places
#define roundf3(a) (float) ((a*1000)/1000) // 3 decimal places

// Defined as int16_t so 16 bits to mask
#define AQUA_LOG (1 << 0) // Aqualink Generic / catchall
#define NET_LOG  (1 << 1) // Network
// Control protocols
#define ALLB_LOG (1 << 2) // Allbutton RS Keypad
#define ONET_LOG (1 << 3) // OneTouch
#define IAQT_LOG (1 << 4) // iAqualinkTouch
#define PDA_LOG  (1 << 5) // PDA
#define RSSA_LOG (1 << 6) // Serial Adapter
// Message PRotocols
#define DJAN_LOG (1 << 7) // Jange Device
#define DPEN_LOG (1 << 8) // Pentair Device
// misc
#define RSSD_LOG (1 << 9) // RS485 Connection /dev/ttyUSB?
#define PROG_LOG (1 << 10) // Programmer
#define SCHD_LOG (1 << 11) // Scheduler Timer
#define RSTM_LOG (1 << 12) // RS packet Time
#define SIM_LOG (1 << 13) // Simulator

#define DBGT_LOG (1 << 14) // Debug Timer

#define TIMR_LOG SCHD_LOG
#define PANL_LOG PROG_LOG

/*
#define INT_MAX +2147483647
#define INT_MIN -2147483647
*/

#define AQ_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define AQ_MIN(x, y) (((x) < (y)) ? (x) : (y))


/*
typedef enum
{
  false = FALSE, true = TRUE
} bool;
*/
//void setLoggingPrms(int level , bool deamonized, char* log_file);
#ifdef AQ_MANAGER
void setLoggingPrms(int level , bool deamonized, char *error_messages);
#else
void setLoggingPrms(int level , bool deamonized, char* log_file, char *error_messages);
#endif
int getLogLevel(int16_t from);
int getSystemLogLevel();
void setSystemLogLevel( int level);
void daemonise ( char *pidFile, void (*main_function)(void) );
//void debugPrint (char *format, ...);



void addDebugLogMask(int16_t flag);
void removeDebugLogMask(int16_t flag);
void clearDebugLogMask();
bool isDebugLogMaskSet(int16_t flag);
const char* logmask2name(int16_t mask);
const char* loglevel2name(int level);
//#define logMessage(msg_level, format, ...) LOG (1, msg_level, format, ##__VA_ARGS__)


//void logMessage(int level, const char *format, ...);


//void LOG(int from, int level, char *format, ...);
void LOG(int16_t from, int msg_level, const char *format, ...);
void LOGSystemError (int errnum, int16_t from, const char *on_what);
void displayLastSystemError (const char *on_what);

int count_characters(const char *str, char character);
//void readCfg (char *cfgFile);
int text2elevel(char* level);
char *elevel2text(int level);
char *cleanwhitespace(char *str);
//char *cleanquotes(char *str);
char *chopwhitespace(char *str);
char *trimwhitespace(char *str);
char *stripwhitespace(char *str);
int cleanint(char*str);
bool text2bool(char *str);
bool request2bool(char *str);
char *bool2text(bool val);
void delay (unsigned int howLong);
//void ndelay (float howLong) 
float degFtoC(float degF);
float degCtoF(float degC);
char* stristr(const char* haystack, const char* needle);
//int ascii(char *destination, char *source);
char *prittyString(char *str);
//void writePacketLog(char *buff);
//void closePacketLog();
float timespec2float(const struct timespec *elapsed);

#ifdef AQ_MANAGER
//void startInlineLog2File();
//void stopInlineLog2File();
//void cleanInlineLog2File();
#else
void startInlineDebug();
void stopInlineDebug();
void startInlineSerialDebug();
void cleanInlineDebug();
char *getInlineLogFName();
bool islogFileReady();
#endif


//const char *logmask2name(int16_t from);

//#ifndef _UTILS_C_
  extern bool _daemon_;
  extern bool _debuglog_;
  extern bool _debug2file_;
//#endif

#endif /* UTILS_H_ */
