
#ifndef AQUALINK_H_
#define AQUALINK_H_

#include <pthread.h>
#include "aq_serial.h"
#include "aq_programmer.h"

#define TIME_CHECK_INTERVAL  3600
//#define TIME_CHECK_INTERVAL  600
#define ACCEPTABLE_TIME_DIFF 120

#define MAX_ZERO_READ_BEFORE_RECONNECT 500

#define TOTAL_BUTTONS     12

#define TEMP_UNKNOWN    -999
#define DATE_STRING_LEN   30

enum {
 FAHRENHEIT,
 CELSIUS,
 UNKNOWN
};

typedef struct aqualinkkey
{
  //int number;
  //aqledstate *state;
  aqled *led;
  char *label;
  char *name;
  unsigned char code;
  int dz_idx;
} aqkey;


//typedef struct ProgramThread ProgramThread;  // Definition is later

struct programmingthread {
  pthread_t *thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  program_type ptype;
  //void *thread_args;
};

typedef enum action_type {
  NO_ACTION = -1,
  POOL_HTR_SETOINT,
  SPA_HTR_SETOINT,
  FREEZE_SETPOINT,
  SWG_SETPOINT
} action_type;

struct action {
  action_type type;
  time_t requested;
  int value;
  //char value[10];
};

struct aqualinkdata
{
  //char crap[AQ_MSGLEN];
  char version[AQ_MSGLEN];
  char date[AQ_MSGLEN];
  char time[AQ_MSGLEN];
  //char datestr[DATE_STRING_LEN];
  char last_message[AQ_MSGLONGLEN+1]; // NSF just temp for PDA crap
  //char *last_message; // Be careful using this, can get core dumps.
  //char *display_message;  
  unsigned char raw_status[AQ_PSTLEN];
  aqled aqualinkleds[TOTAL_LEDS];
  aqkey aqbuttons[TOTAL_BUTTONS];
  int air_temp;
  int pool_temp;
  int spa_temp;
  int temp_units;
  int battery;
  //int freeze_protection;
  int frz_protect_set_point;
  int pool_htr_set_point;
  int spa_htr_set_point;
  //unsigned char aq_command;
  struct programmingthread active_thread;
  struct action unactioned;
  int swg_percent;
  int swg_ppm;
  unsigned char ar_swg_status;
  int swg_delayed_percent;
  //bool ar_swg_connected;
};


#endif 
