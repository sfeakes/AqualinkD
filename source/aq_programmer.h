
#ifndef AQ_PROGRAMMER_H_
#define AQ_PROGRAMMER_H_

#include <pthread.h>
//#include "aqualink.h"

#define PROGRAMMING_POLL_DELAY_TIME 10
//#define PROGRAMMING_POLL_DELAY_TIME 2
//#define PROGRAMMING_POLL_DELAY_TIME 5
#define PROGRAMMING_POLL_COUNTER 200

// need to get the C values from aqualink manual and add those just incase
// someone has the controller set to C.
#define HEATER_MAX_F 104
#define HEATER_MIN_F 36
#define FREEZE_PT_MAX_F 42
#define FREEZE_PT_MIN_F 34

#define HEATER_MAX_C 40
#define HEATER_MIN_C 0
#define FREEZE_PT_MAX_C 5
#define FREEZE_PT_MIN_C 1

#define SWG_PERCENT_MAX 101
#define SWG_PERCENT_MIN 0

#define PTHREAD_ARG 25
#define LIGHT_MODE_BUFER PTHREAD_ARG

typedef enum emulation_type{
  SIM_NONE = -1,
  ALLBUTTON,
  RSSADAPTER,
  ONETOUCH,
  IAQTOUCH,
  AQUAPDA,  // AQUAPALM and PDA are taken as specific type.
  JANDY_DEVICE, // Very rarley used.
  SIMULATOR
} emulation_type;

typedef enum {
  AQP_NULL = -1,
  // ********* Generic Programming options, these are Allbutton by Default
  AQ_GET_POOL_SPA_HEATER_TEMPS,
  AQ_GET_FREEZE_PROTECT_TEMP,
  AQ_SET_TIME,
  AQ_SET_POOL_HEATER_TEMP,
  AQ_SET_SPA_HEATER_TEMP,
  AQ_SET_FRZ_PROTECTION_TEMP,
  AQ_GET_DIAGNOSTICS_MODEL,
  AQ_GET_PROGRAMS,
  AQ_SET_LIGHTPROGRAM_MODE,
  AQ_SET_LIGHTCOLOR_MODE, 
  AQ_SET_SWG_PERCENT,
  AQ_GET_AUX_LABELS,
  AQ_SET_BOOST,
  AQ_SET_PUMP_RPM,
  AQ_SET_PUMP_VS_PROGRAM,
  // ******** PDA Delimiter make sure to change MAX/MIN below
  AQ_PDA_INIT,
  AQ_PDA_WAKE_INIT,
  AQ_PDA_DEVICE_STATUS,
  AQ_PDA_DEVICE_ON_OFF,
  AQ_PDA_AUX_LABELS,
  AQ_PDA_SET_BOOST,
  AQ_PDA_SET_SWG_PERCENT,
  AQ_PDA_GET_AUX_LABELS,
  AQ_PDA_SET_POOL_HEATER_TEMPS,
  AQ_PDA_SET_SPA_HEATER_TEMPS,
  AQ_PDA_SET_FREEZE_PROTECT_TEMP,
  AQ_PDA_SET_TIME,
  AQ_PDA_GET_POOL_SPA_HEATER_TEMPS,
  AQ_PDA_GET_FREEZE_PROTECT_TEMP,
  // ******** OneTouch Delimiter make sure to change MAX/MIN below
  AQ_SET_ONETOUCH_PUMP_RPM,
  AQ_SET_ONETOUCH_MACRO,
  AQ_GET_ONETOUCH_SETPOINTS,
  AQ_SET_ONETOUCH_POOL_HEATER_TEMP,
  AQ_SET_ONETOUCH_SPA_HEATER_TEMP,
  AQ_GET_ONETOUCH_FREEZEPROTECT,
  AQ_SET_ONETOUCH_FREEZEPROTECT,
  AQ_SET_ONETOUCH_TIME,
  AQ_SET_ONETOUCH_BOOST,
  AQ_SET_ONETOUCH_SWG_PERCENT,
  // ******** iAqalink Touch Delimiter make sure to change MAX/MIN below
  AQ_SET_IAQTOUCH_PUMP_RPM,
  AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM,
  AQ_GET_IAQTOUCH_VSP_ASSIGNMENT,
  AQ_GET_IAQTOUCH_SETPOINTS,
  AQ_GET_IAQTOUCH_FREEZEPROTECT,
  AQ_SET_IAQTOUCH_FREEZEPROTECT,
  AQ_GET_IAQTOUCH_AUX_LABELS,
  AQ_SET_IAQTOUCH_SWG_PERCENT,
  AQ_SET_IAQTOUCH_SWG_BOOST,
  AQ_SET_IAQTOUCH_POOL_HEATER_TEMP,
  AQ_SET_IAQTOUCH_SPA_HEATER_TEMP,
  AQ_SET_IAQTOUCH_SET_TIME,
  AQ_SET_IAQTOUCH_DEVICE_ON_OFF,
  AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE,
  // ******** RS Serial Adapter Delimiter make sure to change MAX/MIN below
  AQ_GET_RSSADAPTER_SETPOINTS,
  AQ_SET_RSSADAPTER_POOL_HEATER_TEMP,
  AQ_SET_RSSADAPTER_SPA_HEATER_TEMP,
  AQ_ADD_RSSADAPTER_POOL_HEATER_TEMP,
  AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP,
  // ******** Delimiter make sure to change MAX/MIN below
} program_type;

#define AQP_GENERIC_MIN      AQ_GET_POOL_SPA_HEATER_TEMPS
#define AQP_GENERIC_MAX      AQ_SET_PUMP_VS_PROGRAM

#define AQP_ALLBUTTON_MIN    AQ_GET_POOL_SPA_HEATER_TEMPS
#define AQP_ALLBUTTONL_MAX   AQ_SET_BOOST

#define AQP_PDA_MIN          AQ_PDA_INIT
#define AQP_PDA_MAX          AQ_SET_ONETOUCH_SWG_PERCENT

#define AQP_ONETOUCH_MIN     AQ_SET_ONETOUCH_PUMP_RPM
#define AQP_ONETOUCH_MAX     AQ_SET_ONETOUCH_SWG_PERCENT

#define AQP_IAQTOUCH_MIN     AQ_SET_IAQTOUCH_PUMP_RPM 
#define AQP_IAQTOUCH_MAX     AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE

#define AQP_RSSADAPTER_MIN   AQ_GET_RSSADAPTER_SETPOINTS 
#define AQP_RSSADAPTER_MAX   AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP


struct programmingThreadCtrl {
  pthread_t thread_id;
  //void *thread_args;
  char thread_args[PTHREAD_ARG];
  struct aqualinkdata *aq_data;
};


typedef enum pump_type {
  PT_UNKNOWN = -1,
  EPUMP,           // = ePump AC & Jandy ePUMP
  VSPUMP,          // = Intelliflo VS
  VFPUMP           // = Intelliflo VF  (GPM)
} pump_type;


//void aq_programmer(program_type type, void *args, struct aqualinkdata *aq_data);
void aq_programmer(program_type type, char *args, struct aqualinkdata *aq_data);
//void kick_aq_program_thread(struct aqualinkdata *aq_data);
void kick_aq_program_thread(struct aqualinkdata *aq_data, emulation_type source_type);
bool in_programming_mode(struct aqualinkdata *aq_data);
bool in_ot_programming_mode(struct aqualinkdata *aq_data);
bool in_iaqt_programming_mode(struct aqualinkdata *aq_data);
bool in_swg_programming_mode(struct aqualinkdata *aq_data);
bool in_light_programming_mode(struct aqualinkdata *aq_data);
bool in_allb_programming_mode(struct aqualinkdata *aq_data);
//void aq_send_cmd(unsigned char cmd);
void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aq_data);
//void queueGetExtendedProgramData(emulation_type source_type, struct aqualinkdata *aq_data, bool labels);
//unsigned char pop_aq_cmd(struct aqualinkdata *aq_data);

void waitForSingleThreadOrTerminate(struct programmingThreadCtrl *threadCtrl, program_type type);
void cleanAndTerminateThread(struct programmingThreadCtrl *threadCtrl);

//void force_queue_delete() // Yes I want compiler warning if this is used.


//bool push_aq_cmd(unsigned char cmd);

//void send_cmd(unsigned char cmd, struct aqualinkdata *aq_data);
//void cancel_menu(struct aqualinkdata *aq_data);

//void *set_aqualink_time( void *ptr );
//void *get_aqualink_pool_spa_heater_temps( void *ptr );

//int get_aq_cmd_length();
int setpoint_check(int type, int value, struct aqualinkdata *aqdata);
int RPM_check(pump_type type, int value, struct aqualinkdata *aqdata);
//int RPM_check(int type, int value, struct aqualinkdata *aqdata);
const char *ptypeName(program_type type);
const char *programtypeDisplayName(program_type type);



#endif
