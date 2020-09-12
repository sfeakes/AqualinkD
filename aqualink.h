
#ifndef AQUALINK_H_
#define AQUALINK_H_

#include <pthread.h>
#include <stdbool.h>
#include "aq_serial.h"
#include "aq_programmer.h"
#include "aq_panel.h"

#define DEFAULT_POLL_SPEED -1
#define DEFAULT_POLL_SPEED_NON_THREADDED 2

#define TIME_CHECK_INTERVAL  3600
#define ACCEPTABLE_TIME_DIFF 120

// Use these settings to test time
//#define TIME_CHECK_INTERVAL  100
//#define ACCEPTABLE_TIME_DIFF 10

#define MAX_ZERO_READ_BEFORE_RECONNECT 10000 // 2k normally

void intHandler(int dummy);
void setUnits(const char *msg);

// There are cases where SWG will read 80% in allbutton and 0% in onetouch/aqualinktouch, this will compile that in or our
//#define READ_SWG_FROM_EXTENDED_ID

//#define TOTAL_BUTTONS     12
/*
#ifndef AQ_RS16
#define TOTAL_BUTTONS          12
#else
#define TOTAL_BUTTONS          20
#define RS16_VBUTTONS_START    13  // RS16 panel has 4 buttons with no LED's, so list them for manual matching to RS messages
#define RS16_VBUTTONS_END      16  // RS16 panel has 4 buttons with no LED's, so list them for manual matching to RS messages
#endif
*/
#define TEMP_UNKNOWN    -999
//#define UNKNOWN TEMP_UNKNOWN
#define DATE_STRING_LEN   30

#define MAX_PUMPS 4
#define MAX_LIGHTS 4

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
//#ifdef AQ_PDA
//  char *pda_label;
//#endif
  unsigned char code;
  int dz_idx;
  uint8_t special_mask;
} aqkey;

// special_mask
#define VS_PUMP        (1 << 0)
#define PROGRAM_LIGHT  (1 << 1)
#define TIMER_ACTIVE   (1 << 2) // Not used yet, but will need to timer

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
  SWG_SETPOINT,
  SWG_BOOST,
  PUMP_RPM,
  PUMP_VSPROGRAM,
  POOL_HTR_INCREMENT,   // Setpoint add value (can be negative)
  SPA_HTR_INCREMENT    // Setpoint add value
} action_type;

struct action {
  action_type type;
  time_t requested;
  int value;
  int id; // Only used for Pumps at the moment.
  //char value[10];
};

// Moved to aq_programmer to stop circular dependancy
/*
typedef enum pump_type {
  PT_UNKNOWN = -1,
  EPUMP,
  VSPUMP,
  VFPUMP
} pump_type;
*/

#define PUMP_PRIMING -1
#define PUMP_OFFLINE -2
#define PUMP_ERROR   -3
#define PUMP_OFF_RPM 0
#define PUMP_OFF_GPM PUMP_OFF_RPM
#define PUMP_OFF_WAT PUMP_OFF_RPM

typedef struct pumpd
{
  int rpm;
  int gpm;
  int watts;
  unsigned char pumpID;
  int pumpIndex;
  pump_type pumpType;
  //int buttonID;
  protocolType prclType;
  aqkey *button;
  //bool updated;
} pump_detail;

// color light modes (Aqualink program, Jandy, Jandy LED, SAm/SAL, Color Logic, Intellibrite)
typedef enum clight_type {
  LC_PROGRAMABLE=0, 
  LC_JANDY, 
  LC_JANDYLED, 
  LC_SAL, 
  LC_CLOGIG, 
  LC_INTELLIB
} clight_type;

typedef struct clightd
{
  clight_type lightType;
  aqkey *button;
} clight_detail;

struct aqualinkdata
{
  char version[AQ_MSGLEN*2];
  char date[AQ_MSGLEN];
  char time[AQ_MSGLEN];
  char last_message[AQ_MSGLONGLEN+1]; // Last ascii message from panel - allbutton (or PDA) protocol
  char last_display_message[AQ_MSGLONGLEN+1]; // Last message to display in web UI
  aqled aqualinkleds[TOTAL_LEDS];
  aqkey aqbuttons[TOTAL_BUTTONS];
  unsigned short total_buttons;
  int air_temp;
  int pool_temp;
  int spa_temp;
  int temp_units;
  //bool single_device; // Pool or Spa only, not Pool & Spa (Thermostat setpoints are different)
  int battery;
  int frz_protect_set_point;
  int pool_htr_set_point;
  int spa_htr_set_point;
  int swg_percent;
  int swg_ppm;
  unsigned char ar_swg_device_status; // Actual state 
  aqledstate swg_led_state; // Display state for UI's
  aqledstate service_mode_state;
  aqledstate frz_protect_state;
  int num_pumps;
  pump_detail pumps[MAX_PUMPS];
  int num_lights;
  clight_detail lights[MAX_LIGHTS];
  bool boost;
  char boost_msg[10];
  float ph;
  int orp;

  // Below this line is not state related. (Future just do a mem compare for change)
  //aqkey *orderedbuttons[TOTAL_BUTTONS]; // Future to reduce RS4,6,8,12,16 & spa buttons
  //unsigned short total_ordered_buttons;
  unsigned char last_packet_type;
  int swg_delayed_percent;
  bool simulate_panel;
  int open_websockets;
  struct programmingthread active_thread;
  struct action unactioned;
  unsigned char raw_status[AQ_PSTLEN];
  // Multiple threads update this value.
  volatile bool updated;

  #ifdef AQ_RS16
  int rs16_vbutton_start;
  int rs16_vbutton_end;
  #endif
  #ifdef AQ_PDA
  int pool_heater_index;
  int spa_heater_index;
  int solar_heater_index;
  #endif
  // Timing for DEBUG
  #ifdef AQ_DEBUG
  struct timespec last_active_time;
  struct timespec start_active_time;
  #endif
};


#endif 
