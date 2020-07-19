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
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>


#include "aqualink.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"

#ifdef AQ_PDA
  #include "pda.h"
  #include "pda_menu.h"
  #include "pda_aq_programmer.h"
#endif

#include "aq_panel.h"
#include "onetouch_aq_programmer.h"
#include "iaqtouch_aq_programmer.h"
#include "color_lights.h"
#include "config.h"
#include "devices_jandy.h"

#ifdef AQ_DEBUG
  #include <time.h>
  #include "timespec_subtract.h"
#endif

bool select_sub_menu_item(struct aqualinkdata *aq_data, char* item_string);
bool select_menu_item(struct aqualinkdata *aq_data, char* item_string);
//void send_cmd(unsigned char cmd, struct aqualinkdata *aq_data);
void cancel_menu();


void *set_aqualink_pool_heater_temps( void *ptr );
void *set_aqualink_spa_heater_temps( void *ptr );
void *set_aqualink_freeze_heater_temps( void *ptr );
void *set_aqualink_time( void *ptr );
void *get_aqualink_pool_spa_heater_temps( void *ptr );
void *get_aqualink_programs( void *ptr );
void *get_freeze_protect_temp( void *ptr );
void *get_aqualink_diag_model( void *ptr );
void *get_aqualink_aux_labels( void *ptr );
//void *threadded_send_cmd( void *ptr );
void *set_aqualink_light_programmode( void *ptr );
void *set_aqualink_light_colormode( void *ptr );
#ifdef AQ_PDA
void *set_aqualink_PDA_init( void *ptr );
#endif
void *set_aqualink_SWG( void *ptr );
void *set_aqualink_boost( void *ptr );
/*
void *set_aqualink_onetouch_pump_rpm( void *ptr );
void *set_aqualink_onetouch_macro( void *ptr );
void *get_aqualink_onetouch_setpoints( void *ptr );
*/

//void *get_aqualink_PDA_device_status( void *ptr );
//void *set_aqualink_PDA_device_on_off( void *ptr );

bool waitForButtonState(struct aqualinkdata *aq_data, aqkey* button, aqledstate state, int numMessageReceived);

//bool waitForMessage(struct aqualinkdata *aq_data, char* message, int numMessageReceived);
bool waitForEitherMessage(struct aqualinkdata *aq_data, char* message1, char* message2, int numMessageReceived);

bool push_aq_cmd(unsigned char cmd);
void waitfor_queue2empty();
void longwaitfor_queue2empty();

#define MAX_STACK 20
int _stack_place = 0;
unsigned char _commands[MAX_STACK];
//unsigned char pgm_commands[MAX_STACK];
unsigned char _pgm_command = NUL;
//unsigned char _ot_pgm_command = NUL;

bool _last_sent_was_cmd = false;

// External view of adding to queue
void aq_send_cmd(unsigned char cmd) {
  push_aq_cmd(cmd);
}

/*
void ot_send_cmd(unsigned char cmd) {
  _ot_pgm_command = cmd;
}

unsigned char pop_ot_cmd(unsigned char receive_type)
{
  unsigned char cmd = NUL;

  if (receive_type  == CMD_STATUS) {
    cmd = _ot_pgm_command;
    _ot_pgm_command = NUL;
  } 

  return cmd;
}
*/
bool push_aq_cmd(unsigned char cmd) {
  
  //logMessage(LOG_DEBUG, "push_aq_cmd '0x%02hhx'\n", cmd);

  if (_stack_place < MAX_STACK) {
    _commands[_stack_place] = cmd;
    _stack_place++;
  } else {
    logMessage(LOG_ERR, "Command queue overflow, too many unsent commands to RS control panel\n");
    return false;
  }

  return true;
}

int get_aq_cmd_length()
{
  return _stack_place;
}

unsigned char pop_aq_cmd(struct aqualinkdata *aq_data)
{
  unsigned char cmd = NUL;
  // Only send commands on status messages 
  // Are we in programming mode and it's not ONETOUCH programming mode
  if (aq_data->active_thread.thread_id != 0 && in_ot_programming_mode(aq_data) == false ) {
  //if (aq_data->active_thread.thread_id != 0) {
    if ( _pgm_command != NUL && aq_data->last_packet_type == CMD_STATUS) {
      cmd = _pgm_command;
      _pgm_command = NUL;
      logMessage(LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx' (programming)\n", cmd);
    } else if (_pgm_command != NUL) {
      logMessage(LOG_DEBUG_SERIAL, "RS Waiting to send cmd '0x%02hhx' (programming)\n", _pgm_command);
    } else {
      logMessage(LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx' empty queue (programming)\n", cmd);
    }
  } else if (_stack_place > 0 && aq_data->last_packet_type == CMD_STATUS ) {
    cmd = _commands[0];
    _stack_place--;
    logMessage(LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
    memmove(&_commands[0], &_commands[1], sizeof(unsigned char) * _stack_place ) ;
  } else {
    logMessage(LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
  }

//printf("RSM sending cmd '0x%02hhx' in reply to '0x%02hhx'\n",cmd,aq_data->last_packet_type);

  return cmd;
}


/*

unsigned char pop_aq_cmd_OLD(struct aqualinkdata *aq_data)
{
  unsigned char cmd = NUL;
  // Only send commands on status messages 
  // Are we in programming mode
  if (aq_data->active_thread.thread_id != 0) {
    if ( (_pgm_command == KEY_MENU && aq_data->last_packet_type == CMD_STATUS) ||
    // Need to not the key_menu below
         ( _pgm_command != NUL && (aq_data->last_packet_type == CMD_STATUS || aq_data->last_packet_type == CMD_MSG_LONG) )) {
      cmd = _pgm_command;
      _pgm_command = NUL;
      logMessage(LOG_DEBUG, "RS SEND cmd '0x%02hhx' (programming)\n", cmd);
    } else if (_pgm_command != NUL) {
      logMessage(LOG_DEBUG, "RS Waiting to send cmd '0x%02hhx' (programming)\n", _pgm_command);
    } else {
      logMessage(LOG_DEBUG, "RS SEND cmd '0x%02hhx' empty queue (programming)\n", cmd);
    }
  } else if (_stack_place > 0 && aq_data->last_packet_type == CMD_STATUS ) {
    cmd = _commands[0];
    _stack_place--;
    logMessage(LOG_DEBUG, "RS SEND cmd '0x%02hhx'\n", cmd);
    memmove(&_commands[0], &_commands[1], sizeof(unsigned char) * _stack_place ) ;
  } else {
    logMessage(LOG_DEBUG, "RS SEND cmd '0x%02hhx'\n", cmd);
  }

//printf("RSM sending cmd '0x%02hhx' in reply to '0x%02hhx'\n",cmd,aq_data->last_packet_type);

  return cmd;
}
*/

int roundTo(int num, int denominator) {
  return ((num + (denominator/2) ) / denominator )* denominator;
}

//(Intelliflo VF you set GPM, not RPM)
int RPM_check(pump_type type, int value, struct aqualinkdata *aqdata)
{
  int rtn = value;
  // RPM 3450 seems to be max
  // RPM 600 min
  // GPM 130 max
  // GPM 15 min
  if (type == VFPUMP) {
    if (rtn > 130)
      rtn = 130;
    else if (rtn < 15)
      rtn = 15;
    else
      rtn = roundTo(rtn, 5);
  } else {
    if (rtn > 3450)
      rtn = 3450;
    else if (rtn < 600)
      rtn = 600;
    else
      rtn = roundTo(rtn, 5);
  }

  return rtn;
}

int setpoint_check(int type, int value, struct aqualinkdata *aqdata)
{
  int rtn = value;
  int max = 0;
  int min = 0;
  char *type_msg;

  switch(type) {
    case POOL_HTR_SETOINT:
      type_msg = (isSINGLE_DEV_PANEL?"Temp1":"Pool");
      if ( aqdata->temp_units == CELSIUS ) {
        max = HEATER_MAX_C;
        min = (isSINGLE_DEV_PANEL?HEATER_MIN_C:HEATER_MIN_C-1);
      } else {
        max = HEATER_MAX_F;
        min = (isSINGLE_DEV_PANEL?HEATER_MIN_F:HEATER_MIN_F-1);
      }
      // if single device then TEMP1 & 2 (not pool & spa), TEMP1 must be set higher than TEMP2
      if (isSINGLE_DEV_PANEL && 
          aqdata->spa_htr_set_point != TEMP_UNKNOWN &&
          min <= aqdata->spa_htr_set_point) 
      {
        min = aqdata->spa_htr_set_point + 1;
      }
    break;
    case SPA_HTR_SETOINT:
      type_msg = (isSINGLE_DEV_PANEL?"Temp2":"Spa");
      if ( aqdata->temp_units == CELSIUS ) {
        max = (isSINGLE_DEV_PANEL?HEATER_MAX_C:HEATER_MAX_C-1);
        min = HEATER_MIN_C;
      } else {
        max = (isSINGLE_DEV_PANEL?HEATER_MAX_F:HEATER_MAX_F-1);
        min = HEATER_MIN_F;
      }
      // if single device then TEMP1 & 2 (not pool & spa), TEMP2 must be set lower than TEMP1
      if (isSINGLE_DEV_PANEL && 
          aqdata->pool_htr_set_point != TEMP_UNKNOWN &&
          max >= aqdata->pool_htr_set_point) 
      {
        max = aqdata->pool_htr_set_point - 1;
      }
    break;
    case FREEZE_SETPOINT:
      type_msg = "Freeze protect";
      if ( aqdata->temp_units == CELSIUS ) {
        max = FREEZE_PT_MAX_C;
        min = FREEZE_PT_MIN_C;
      } else {
        max = FREEZE_PT_MAX_F;
        min = FREEZE_PT_MIN_F;
      }
    break;
    case SWG_SETPOINT:
      type_msg = "Salt Water Generator";
      max = SWG_PERCENT_MAX;
      min = SWG_PERCENT_MIN;
    break;
    default:
      type_msg = "Unknown";
    break;
  }

  if (rtn > max)
    rtn = max;
  else if (rtn < min)
    rtn = min;

  // If SWG make sure it's 0,5,10,15,20......
  if (type == SWG_SETPOINT) {
    rtn = roundTo(rtn, 5);
  }
  
  if (rtn != value)
    logMessage(LOG_WARNING, "Setpoint of %d for %s is outside range, using %d\n",value,type_msg,rtn);
  //else
  //  logMessage(LOG_NOTICE, "Setting setpoint of %s to %d\n",type_msg,rtn);

  return rtn;
}

void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aq_data)
{
  // Wait for onetouch if enabeled.
  //if ( source_type == ALLBUTTON && ( onetouch_enabled() == false || extended_device_id_programming() == false ) ) {
  if ( source_type == ALLBUTTON && ( isEXTP_ENABLED == false || (isONET_ENABLED == false && isIAQT_ENABLED == false)) ) {
    aq_send_cmd(NUL);
    aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, aq_data);
    aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, aq_data);
    if (_aqconfig_.use_panel_aux_labels)
      aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data);
#ifdef AQ_ONETOUCH
  } else if ( source_type == ONETOUCH) {
    aq_programmer(AQ_GET_ONETOUCH_SETPOINTS, NULL, aq_data);
    if (_aqconfig_.use_panel_aux_labels)
      aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data);
#endif
#ifdef AQ_IAQTOUCH
  } else if ( source_type == IAQTOUCH) {
    aq_programmer(AQ_GET_IAQTOUCH_SETPOINTS, NULL, aq_data);
    if (_aqconfig_.use_panel_aux_labels)
      aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data);
      //aq_programmer(AQ_GET_IAQTOUCH_AUX_LABELS, NULL, aq_data);
#endif
#ifdef AQ_PDA
  } else if ( source_type == AQUAPDA) {
    aq_programmer(AQ_PDA_INIT, NULL, aq_data);
#endif
  }
}

/*
void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aq_data)
{
  queueGetExtendedProgramData(source_type, aq_data, false);
}
*/
/*
void kick_aq_program_thread(struct aqualinkdata *aq_data)
{
  if (aq_data->active_thread.thread_id != 0) {
    logMessage(LOG_DEBUG, "Kicking thread %d,%p message '%s'\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id,aq_data->last_message);
    pthread_cond_broadcast(&aq_data->active_thread.thread_cond);
  }
}
*/
bool in_swg_programming_mode(struct aqualinkdata *aq_data)
{
  if ( ( aq_data->active_thread.thread_id != 0 ) &&
       ( aq_data->active_thread.ptype == AQ_SET_ONETOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_BOOST)
  ) {
    return true;
  }

  return false;
}

bool in_ot_programming_mode(struct aqualinkdata *aq_data)
{
  //( type != AQ_SET_PUMP_RPM || type != AQ_SET_OT_MACRO )) {

  if ( ( aq_data->active_thread.thread_id != 0 ) &&
       ( aq_data->active_thread.ptype == AQ_SET_ONETOUCH_PUMP_RPM ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_MACRO || 
         aq_data->active_thread.ptype == AQ_GET_ONETOUCH_SETPOINTS ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_TIME ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_POOL_HEATER_TEMP ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_SPA_HEATER_TEMP ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_FREEZEPROTECT)
     ) {
     return true;
  }

  return false;
}

bool in_iaqt_programming_mode(struct aqualinkdata *aq_data)
{
  //( type != AQ_SET_PUMP_RPM || type != AQ_SET_OT_MACRO )) {

  if ( ( aq_data->active_thread.thread_id != 0 ) &&
       ( aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_PUMP_RPM ||
         aq_data->active_thread.ptype == AQ_GET_IAQTOUCH_VSP_ASSIGNMENT ||
         aq_data->active_thread.ptype == AQ_GET_IAQTOUCH_SETPOINTS ||
         aq_data->active_thread.ptype == AQ_GET_IAQTOUCH_AUX_LABELS ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_POOL_HEATER_TEMP ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SPA_HEATER_TEMP ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SET_TIME ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM)
     ) {
     return true;
  }

  return false;
}

bool in_programming_mode(struct aqualinkdata *aq_data)
{
  if ( aq_data->active_thread.thread_id != 0 ) {
     return true;
  }

  return false;
}

void kick_aq_program_thread(struct aqualinkdata *aq_data, emulation_type source_type)
{
  if ( aq_data->active_thread.thread_id != 0 ) {
    if ( (source_type == ONETOUCH) && in_ot_programming_mode(aq_data))
    {
      logMessage(LOG_DEBUG, "Kicking OneTouch thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);   
    } 
    else if (source_type == ALLBUTTON && !in_ot_programming_mode(aq_data)) {
      logMessage(LOG_DEBUG, "Kicking RS thread %d,%p message '%s'\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id,aq_data->last_message);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
    else if (source_type == IAQTOUCH && in_iaqt_programming_mode(aq_data)) {
      logMessage(LOG_DEBUG, "Kicking IAQ Touch thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
#ifdef AQ_PDA
    else if (source_type == AQUAPDA && !in_ot_programming_mode(aq_data)) {
      logMessage(LOG_DEBUG, "Kicking PDA thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
#endif     
  }
}

void aq_programmer(program_type r_type, char *args, struct aqualinkdata *aq_data)
{
  struct programmingThreadCtrl *programmingthread = malloc(sizeof(struct programmingThreadCtrl));

  program_type type = r_type;

  if (r_type == AQ_SET_PUMP_RPM) {
    if (isONET_ENABLED)
      type = AQ_SET_ONETOUCH_PUMP_RPM;
    else if (isIAQT_ENABLED)
      type = AQ_SET_IAQTOUCH_PUMP_RPM;
    else {
      logMessage(LOG_ERR, "Can only change pump RPM with an extended device id\n",type);
      return;
    }
  } else if (r_type == AQ_SET_PUMP_VS_PROGRAM) {
    if (isIAQT_ENABLED)
      type = AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM;
    else {
      logMessage(LOG_ERR, "Can only change pump VS Program with an iAqualink Touch device id\n",type);
      return;
    }
  }
#ifdef AQ_ONETOUCH
  // reset any types if to onetouch if available and if one touch is quicker
  // At moment. onetouch is quicker for boost, and slower for heaters
  else if (isONET_ENABLED && isEXTP_ENABLED) {
    switch (r_type){
      case AQ_GET_POOL_SPA_HEATER_TEMPS:
      case AQ_GET_FREEZE_PROTECT_TEMP:
        type = AQ_GET_ONETOUCH_SETPOINTS;
      break;
      case AQ_SET_POOL_HEATER_TEMP:
        type = AQ_SET_ONETOUCH_POOL_HEATER_TEMP;
      break;
      case AQ_SET_SPA_HEATER_TEMP:
        type = AQ_SET_ONETOUCH_SPA_HEATER_TEMP;
      break;
      case AQ_SET_SWG_PERCENT:
        type = AQ_SET_ONETOUCH_SWG_PERCENT;
      break;
      case AQ_SET_BOOST:
        type = AQ_SET_ONETOUCH_BOOST;
      break;
      default:
        type = r_type;
      break;
    }
  }
#endif
#ifdef AQ_IAQTOUCH
  else if (isIAQT_ENABLED && isEXTP_ENABLED) {
    // IAQ Touch programming modes that should overite standard ones.
    switch (r_type){
      case AQ_GET_POOL_SPA_HEATER_TEMPS:
      case AQ_GET_FREEZE_PROTECT_TEMP:
        type = AQ_GET_IAQTOUCH_SETPOINTS;
      break;
      case AQ_SET_SWG_PERCENT:
        type = AQ_SET_IAQTOUCH_SWG_PERCENT;
      break;
      case AQ_SET_BOOST:
        type = AQ_SET_IAQTOUCH_SWG_BOOST;
      break;
      case AQ_SET_POOL_HEATER_TEMP:
        type = AQ_SET_IAQTOUCH_POOL_HEATER_TEMP;
      break;
      case AQ_SET_SPA_HEATER_TEMP:
        type = AQ_SET_IAQTOUCH_SPA_HEATER_TEMP;
      break;
      case AQ_SET_TIME:
        type = AQ_SET_IAQTOUCH_SET_TIME;
      break;
      default:
        type = r_type;
      break;
    }
  }
#endif
#ifdef AQ_PDA
  // Check we are doing something valid request
  if (isPDA_PANEL) {
    pda_reset_sleep();
    if (type != AQ_PDA_INIT && 
        type != AQ_PDA_WAKE_INIT &&
        type != AQ_PDA_DEVICE_STATUS && 
        type != AQ_SET_POOL_HEATER_TEMP &&
        type != AQ_SET_SPA_HEATER_TEMP && 
        type != AQ_SET_SWG_PERCENT && 
        type != AQ_PDA_DEVICE_ON_OFF &&
#ifdef BETA_PDA_AUTOLABEL
        type != AQ_GET_AUX_LABELS &&
#endif
        type != AQ_GET_POOL_SPA_HEATER_TEMPS &&
        type != AQ_SET_FRZ_PROTECTION_TEMP &&
        type != AQ_SET_BOOST) {
      logMessage(LOG_ERR, "Selected Programming mode '%d' not supported with PDA mode control panel\n",type);
      return;
    } 
  }
#endif

  programmingthread->aq_data = aq_data;
  programmingthread->thread_id = 0;
  //programmingthread->thread_args = args;
  if (args != NULL /*&& type != AQ_SEND_CMD*/)
    strncpy(programmingthread->thread_args, args, sizeof(programmingthread->thread_args)-1);

  switch(type) {
    /*
    case AQ_SEND_CMD:
      logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller\n", &args[0]);
     
      unsigned char cmd = (unsigned char) &args[0];
      if (cmd == NUL) {
        logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller (NEW)\n", cmd);
        push_aq_cmd( cmd );
      } else {
        logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller (OLD)\n", cmd);
        push_aq_cmd((unsigned char)*args);
      }*/
      //logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller\n", (unsigned char)*args);
      /*
      if(aq_data->active_thread.thread_id == 0) { // No need to thread a plane send if no active threads
        send_cmd( (unsigned char)*args, aq_data);
      } else if( pthread_create( &programmingthread->thread_id , NULL ,  threadded_send_cmd, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      */
      //break;
    case AQ_GET_POOL_SPA_HEATER_TEMPS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_pool_spa_heater_temps, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_FREEZE_PROTECT_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_freeze_protect_temp, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_TIME:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_time, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case  AQ_SET_POOL_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_pool_heater_temps, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case  AQ_SET_SPA_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_spa_heater_temps, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case  AQ_SET_FRZ_PROTECTION_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_freeze_heater_temps, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_DIAGNOSTICS_MODEL:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_diag_model, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_PROGRAMS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_programs, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_LIGHTPROGRAM_MODE:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_light_programmode, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_LIGHTCOLOR_MODE:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_light_colormode, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_SWG_PERCENT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_SWG, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_AUX_LABELS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_aux_labels, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_BOOST:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_boost, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
#ifdef AQ_ONETOUCH
    case AQ_SET_ONETOUCH_PUMP_RPM:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_pump_rpm, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_ONETOUCH_SETPOINTS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_onetouch_setpoints, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_TIME:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_time, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_BOOST:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_boost, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_SWG_PERCENT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_swg_percent, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_POOL_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_pool_heater_temp, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_SPA_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_spa_heater_temp, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_ONETOUCH_FREEZEPROTECT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_onetouch_freezeprotect, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
#endif
#ifdef AQ_IAQTOUCH
    case AQ_SET_IAQTOUCH_PUMP_RPM:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_pump_rpm, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_IAQTOUCH_VSP_ASSIGNMENT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_vsp_assignments, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_IAQTOUCH_SETPOINTS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_iaqtouch_setpoints, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_GET_IAQTOUCH_AUX_LABELS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_iaqtouch_aux_labels, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_SWG_PERCENT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_swg_percent, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_SWG_BOOST:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_swg_boost, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_POOL_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_pool_heater_temp, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_SPA_HEATER_TEMP:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_spa_heater_temp, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_SET_TIME:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_time, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_iaqtouch_pump_vs_program, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
#endif
#ifdef AQ_PDA
    case AQ_PDA_INIT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_PDA_init, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_PDA_WAKE_INIT:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_PDA_wakeinit, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_PDA_DEVICE_STATUS:
      if( pthread_create( &programmingthread->thread_id , NULL ,  get_aqualink_PDA_device_status, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
    case AQ_PDA_DEVICE_ON_OFF:
      if( pthread_create( &programmingthread->thread_id , NULL ,  set_aqualink_PDA_device_on_off, (void*)programmingthread) < 0) {
        logMessage (LOG_ERR, "could not create thread\n");
        return;
      }
      break;
  #endif

    default:
        logMessage (LOG_ERR, "Didn't understand programming mode type\n");
      break;
  }
  
  if ( programmingthread->thread_id != 0 ) {
    //logMessage (LOG_DEBUG, "********* DID pthread_detach %d\n",programmingthread->thread_id);
    pthread_detach(programmingthread->thread_id);
  } else {
    //logMessage (LOG_DEBUG, "********* DID NOT pthread_detach\n");
  }
}


void waitForSingleThreadOrTerminate(struct programmingThreadCtrl *threadCtrl, program_type type)
{
  //static int tries = 120;
  int tries = 120;
  static int waitTime = 1;
  int i=0;

  i = 0;
  while (get_aq_cmd_length() > 0 && ( i++ <= tries) ) {
    logMessage (LOG_DEBUG, "Thread %p (%s) sleeping, waiting command queue to empty\n", &threadCtrl->thread_id, ptypeName(type));
    sleep(waitTime);
  }
  if (i >= tries) {
    logMessage (LOG_ERR, "Thread %p (%s) timeout waiting, ending\n",&threadCtrl->thread_id,ptypeName(type));
    free(threadCtrl);
    pthread_exit(0);
  }

  while ( (threadCtrl->aq_data->active_thread.thread_id != 0) && ( i++ <= tries) ) {
    //logMessage (LOG_DEBUG, "Thread %d sleeping, waiting for thread %d to finish\n", threadCtrl->thread_id, threadCtrl->aq_data->active_thread.thread_id);
    logMessage (LOG_DEBUG, "Thread %p (%s) sleeping, waiting for thread %p (%s) to finish\n",
                &threadCtrl->thread_id, ptypeName(type),
                threadCtrl->aq_data->active_thread.thread_id, ptypeName(threadCtrl->aq_data->active_thread.ptype));
    sleep(waitTime);
  }
  
  if (i >= tries) {
    //logMessage (LOG_ERR, "Thread %d timeout waiting, ending\n",threadCtrl->thread_id);
    logMessage (LOG_ERR, "Thread %d,%p timeout waiting for thread %d,%p to finish\n",
                type, &threadCtrl->thread_id, threadCtrl->aq_data->active_thread.ptype,
                threadCtrl->aq_data->active_thread.thread_id);
    free(threadCtrl);
    pthread_exit(0);
  }
 
  // Clear out any messages to the UI.
  threadCtrl->aq_data->last_display_message[0] = '\0';
  threadCtrl->aq_data->active_thread.thread_id = &threadCtrl->thread_id;
  threadCtrl->aq_data->active_thread.ptype = type;

  #ifdef AQ_DEBUG
    clock_gettime(CLOCK_REALTIME, &threadCtrl->aq_data->start_active_time);
  #endif

  logMessage (LOG_NOTICE, "Programming: %s\n", ptypeName(threadCtrl->aq_data->active_thread.ptype));

  logMessage (LOG_DEBUG, "Thread %d,%p is active (%s)\n",
              threadCtrl->aq_data->active_thread.ptype,
              threadCtrl->aq_data->active_thread.thread_id,
              ptypeName(threadCtrl->aq_data->active_thread.ptype));
}

void cleanAndTerminateThread(struct programmingThreadCtrl *threadCtrl)
{
  #ifndef AQ_DEBUG
  logMessage(LOG_DEBUG, "Thread %d,%p (%s) finished\n",threadCtrl->aq_data->active_thread.ptype, threadCtrl->thread_id,ptypeName(threadCtrl->aq_data->active_thread.ptype));
  #else
  struct timespec elapsed;
  clock_gettime(CLOCK_REALTIME, &threadCtrl->aq_data->last_active_time);
  timespec_subtract(&elapsed, &threadCtrl->aq_data->last_active_time, &threadCtrl->aq_data->start_active_time);
  logMessage(LOG_NOTICE, "Thread %d,%p (%s) finished in %d.%03ld sec\n",
             threadCtrl->aq_data->active_thread.ptype,
             threadCtrl->aq_data->active_thread.thread_id,
             ptypeName(threadCtrl->aq_data->active_thread.ptype),
             elapsed.tv_sec, elapsed.tv_nsec / 1000000L);
  #endif

  // Quick delay to allow for last message to be sent.
  delay(500);
  threadCtrl->aq_data->active_thread.thread_id = 0;
  threadCtrl->aq_data->active_thread.ptype = AQP_NULL;
  threadCtrl->thread_id = 0;
  free(threadCtrl);
  pthread_exit(0);
}

bool setAqualinkNumericField_new(struct aqualinkdata *aq_data, char *value_label, int value, int increment);
bool setAqualinkNumericField(struct aqualinkdata *aq_data, char *value_label, int value)
{
  return setAqualinkNumericField_new(aq_data, value_label, value, 1);
}
bool setAqualinkNumericField_new(struct aqualinkdata *aq_data, char *value_label, int value, int increment)
{
  logMessage(LOG_DEBUG,"Setting menu item '%s' to %d\n",value_label, value);
  //char leading[10];  // description of the field (POOL, SPA, FRZ)
  int current_val=-1;        // integer value of the current set point
  //char trailing[10];      // the degrees and scale
  char searchBuf[20];
    
  sprintf(searchBuf, "^%s", value_label);
  int val_len = strlen(value_label);
  int i=0;
  do 
  {
    if (waitForMessage(aq_data, searchBuf, 4) != true) {
      logMessage(LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//logMessage(LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    //sscanf(aq_data->last_message, "%s %d%s", leading, &current_val, trailing);
    //sscanf(aq_data->last_message, "%*[^0123456789]%d", &current_val);
    sscanf(&aq_data->last_message[val_len], "%*[^0123456789]%d", &current_val);
    logMessage(LOG_DEBUG, "%s set to %d, looking for %d\n",value_label,current_val,value);
    
    if(value > current_val) {
      // Increment the field.
      sprintf(searchBuf, "%s %d", value_label, current_val+increment);
      send_cmd(KEY_RIGHT);
    }
    else if(value < current_val) {
      // Decrement the field.
      sprintf(searchBuf, "%s %d", value_label, current_val-increment);
      send_cmd(KEY_LEFT);
    }
    else {
      // Just send ENTER. We are at the right value.
      sprintf(searchBuf, "%s %d", value_label, current_val);
      send_cmd(KEY_ENTER);
    }

    if (i++ >= 100) {
      logMessage(LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', to '%d'\n",value_label,value);
      break;
    }
  } while(value != current_val); 
  
  return true;
}

bool OLD_setAqualinkNumericField_OLD(struct aqualinkdata *aq_data, char *value_label, int value)
{ // Works for everything but not SWG
  logMessage(LOG_DEBUG,"Setting menu item '%s' to %d\n",value_label, value);
  char leading[10];  // description of the field (POOL, SPA, FRZ)
  int current_val;        // integer value of the current set point
  char trailing[10];      // the degrees and scale
  char searchBuf[20];
    
  sprintf(searchBuf, "^%s", value_label);
  
  do 
  {
    if (waitForMessage(aq_data, searchBuf, 3) != true) {
      logMessage(LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//logMessage(LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    sscanf(aq_data->last_message, "%s %d%s", leading, &current_val, trailing);
    logMessage(LOG_DEBUG, "%s set to %d, looking for %d\n",value_label,current_val,value);
    
    if(value > current_val) {
      // Increment the field.
      sprintf(searchBuf, "%s %d", value_label, current_val+1);
      send_cmd(KEY_RIGHT);
    }
    else if(value < current_val) {
      // Decrement the field.
      sprintf(searchBuf, "%s %d", value_label, current_val-1);
      send_cmd(KEY_LEFT);
    }
    else {
      // Just send ENTER. We are at the right value.
      sprintf(searchBuf, "%s %d", value_label, current_val);
      send_cmd(KEY_ENTER);
    }
  } while(value != current_val); 
  
  return true;
}
/*
void *threadded_send_cmd( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SEND_CMD);

  send_cmd( (unsigned char)*threadCtrl->thread_args, aq_data);
  
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}
*/

void *set_aqualink_boost( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_BOOST);
  /*
  menu
<find menu>
BOOST POOL
<wait 2 messages>
PRESS ENTER* TO START BOOST POOL
<press enter>


<Menu when in boost>
BOOST POOL 23:59 REMAINING


menu
<find menu>
BOOST POOL
<find menu>
STOP BOOST POOL
<press enter>
 */
  int val = atoi((char*)threadCtrl->thread_args);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
      set_PDA_aqualink_boost(aq_data, val);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
  }
#endif

  logMessage(LOG_DEBUG, "programming BOOST to %s\n", val==true?"On":"Off");

  if ( select_menu_item(aq_data, "BOOST POOL") != true ) {
    logMessage(LOG_WARNING, "Could not select BOOST POOL menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  if (val==true) {
    waitForMessage(threadCtrl->aq_data, "TO START BOOST POOL", 5);
    send_cmd(KEY_ENTER);
    longwaitfor_queue2empty();
  } else {
    int wait_messages = 5;
    int i=0;
    while( i++ < wait_messages) 
    {
      waitForMessage(aq_data, "STOP BOOST POOL", 1);
      if (stristr(aq_data->last_message, "STOP BOOST POOL") != NULL) {
        // This is a really bad hack, message sequence is out for boost for some reason, so as soon as we see stop message, force enter key.
        //_pgm_command = KEY_ENTER;
        send_cmd(KEY_ENTER);
        logMessage(LOG_DEBUG, "**** FOUND STOP BOOST POOL ****\n");
        //waitfor_queue2empty();
        break;
      } else {
        logMessage(LOG_DEBUG, "Find item in Menu: loop %d of %d looking for 'STOP BOOST POOL' received message '%s'\n",i,wait_messages,aq_data->last_message);
        delay(200);
        if (stristr(aq_data->last_message, "STOP BOOST POOL") != NULL) {
          //_pgm_command = KEY_ENTER;
          send_cmd(KEY_ENTER);
          logMessage(LOG_DEBUG, "**** FOUND STOP BOOST POOL ****\n");
          break;
        } 
        send_cmd(KEY_RIGHT);
        //printf("WAIT\n");
        longwaitfor_queue2empty();
        //printf("FINISHED WAIT\n");
      }
      //waitfor_queue2empty();
      //waitForMessage(aq_data, NULL, 1);
    }
    if (i < wait_messages) {
      // Takes ages to see bost is off from menu, to set it here.
      setSWGboost(aq_data, false);
    }
    /*
    // Extra message overcome.
    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    if ( select_sub_menu_item(aq_data, "STOP BOOST POOL") != true ) {
      logMessage(LOG_WARNING, "Could not select STOP BOOST POOL menu\n");
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }*/
    //send_cmd(KEY_ENTER);
  }

  waitForMessage(aq_data,NULL, 1);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_aqualink_SWG( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_SWG_PERCENT);

  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SWG_SETPOINT, val, aq_data);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
      if (set_PDA_aqualink_SWG_setpoint(aq_data, val))
        setSWGpercent(aq_data, val); // Don't use chageSWGpercent as we are in programming mode.
      cleanAndTerminateThread(threadCtrl);
      return ptr;
  }
#endif 

  //logMessage(LOG_NOTICE, "programming SWG percent to %d\n", val);

  if ( select_menu_item(aq_data, "SET AQUAPURE") != true ) {
    logMessage(LOG_WARNING, "Could not select SET AQUAPURE menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // If spa is on, set SWG for spa, if not set SWG for pool
  if (aq_data->aqbuttons[SPA_INDEX].led->state != OFF) {
    if (select_sub_menu_item(aq_data, "SET SPA SP") != true) {
      logMessage(LOG_WARNING, "Could not select SWG setpoint menu for SPA\n");
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    setAqualinkNumericField_new(aq_data, "SPA SP", val, 5);
  } else {
    if (select_sub_menu_item(aq_data, "SET POOL SP") != true) {
      logMessage(LOG_WARNING, "Could not select SWG setpoint menu\n");
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    setAqualinkNumericField_new(aq_data, "POOL SP", val, 5);
  }

  // Let everyone know we set SWG, if it failed we will update on next message, unless it's 0.
  setSWGpercent(aq_data, val); // Don't use chageSWGpercent as we are in programming mode.

/*
  if (select_sub_menu_item(aq_data, "SET POOL SP") != true) {
    logMessage(LOG_WARNING, "Could not select SWG setpoint menu\n");
    cancel_menu(aq_data);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  setAqualinkNumericField_new(aq_data, "POOL SP", val, 5);
*/
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  // NSF have see the below message RS Message :-
  // 'Pool set to 20%'
  // 'POOL SP IS SET TO 20%'
  waitForMessage(threadCtrl->aq_data, "SET TO", 1);
  //waitForMessage(threadCtrl->aq_data, "POOL SP IS SET TO", 1);

  // Since we read % directly from RS message, wait for another few messages that way
  // We won't registed a SWG bounce, since we already told clients SWG was at new % before programming started
  waitForMessage(threadCtrl->aq_data, NULL, 1);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}



void *get_aqualink_aux_labels( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_AUX_LABELS);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
      get_PDA_aqualink_aux_labels(aq_data);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
  }
#endif

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    logMessage(LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "AUX LABELS") != true) {
    logMessage(LOG_WARNING, "Could not select AUX LABELS menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  waitForMessage(aq_data, NULL, 5); // Receive 5 messages

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_light_colormode( void *ptr )
{
  int i;
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_LIGHTCOLOR_MODE);

  char *buf = (char*)threadCtrl->thread_args;
  const char *mode_name;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int typ = atoi(&buf[10]);

  if (btn < 0 || btn >= aq_data->total_buttons ) {
    logMessage(LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aq_data->aqbuttons[btn];
  unsigned char code = button->code;

  //logMessage(LOG_NOTICE, "Light Programming #: %d, on button: %s, color light type: %d\n", val, button->label, typ);
  
  mode_name = light_mode_name(typ, val-1);

  if (mode_name == NULL) {
    logMessage(LOG_ERR, "Light Programming #: %d, on button: %s, color light type: %d, couldn't find mode name '%s'\n", val, button->label, typ, mode_name);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  } else {
    logMessage(LOG_INFO, "Light Programming #: %d, on button: %s, color light type: %d, name '%s'\n", val, button->label, typ, mode_name);
  }

  // Simply turn the light off if value is 0
  if (val <= 0) {
    if ( button->led->state == ON ) {
      send_cmd(code);
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // Needs to start programming sequence with light off
  if ( button->led->state == ON ) {
    logMessage(LOG_INFO, "Light Programming Initial state on, turning off\n");
    send_cmd(code);
    waitfor_queue2empty();
    if ( !waitForMessage(threadCtrl->aq_data, "OFF", 5)) // Message like 'Aux3 Off'
      logMessage(LOG_ERR, "Light Programming didn't receive OFF message\n");
  }

  // Now turn on and wait for the message "color mode name<>*"
  send_cmd(code);
  waitfor_queue2empty();
  i=0;

  do{
    if ( !waitForMessage(threadCtrl->aq_data, "~*", 3))
      logMessage(LOG_ERR, "Light Programming didn't receive color light mode message\n");

    if (strncasecmp(aq_data->last_message, mode_name, strlen(mode_name)) == 0) {
      logMessage(LOG_INFO, "Light Programming found color mode %s\n",mode_name);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    }

    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    // Just clear current message before waiting for next, since the test in the do can't distinguish
    // as both messages end in "~*"
    waitForMessage(threadCtrl->aq_data, NULL, 1); 

    i++;
  } while (i <= LIGHT_COLOR_OPTIONS);

  if (i == LIGHT_COLOR_OPTIONS) {
    logMessage(LOG_ERR, "Light Programming didn't receive color light mode message for '%s'\n",mode_name);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_aqualink_light_programmode( void *ptr )
{
  int i;
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_LIGHTPROGRAM_MODE);

  char *buf = (char*)threadCtrl->thread_args;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int iOn = atoi(&buf[10]);
  int iOff = atoi(&buf[15]);
  float pmode = atof(&buf[20]);

  if (btn < 0 || btn >= aq_data->total_buttons ) {
    logMessage(LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aq_data->aqbuttons[btn];
  unsigned char code = button->code;

  logMessage(LOG_INFO, "Light Programming #: %d, on button: %s, with pause mode: %f (initial on=%d, initial off=%d)\n", val, button->label, pmode, iOn, iOff);

  // Simply turn the light off if value is 0
  if (val <= 0) {
    if ( button->led->state == ON ) {
      send_cmd(code);
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  int seconds = 1000;
  // Needs to start programming sequence with light on, if off we need to turn on for 15 seconds
  // before we can send the next off.
  if ( button->led->state != ON ) {
    logMessage(LOG_INFO, "Light Programming Initial state off, turning on for %d seconds\n",iOn);
    send_cmd(code);
    delay(iOn * seconds);
  }

  logMessage(LOG_INFO, "Light Programming turn off for %d seconds\n",iOff);
  // Now need to turn off for between 11 and 14 seconds to reset light.
  send_cmd(code);
  delay(iOff * seconds);

  // Now light is reset, pulse the appropiate number of times to advance program.
  logMessage(LOG_INFO, "Light Programming button pulsing on/off %d times\n", val);
 
  // Program light in safe mode (slowley), or quick mode
  if (pmode > 0) {
    for (i = 1; i < (val * 2); i++) {
      logMessage(LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, i % 2 == 0 ? "Off" : "On", val);
      send_cmd(code);
      delay(pmode * seconds); // 0.3 works, but using 0.4 to be safe
    }
  } else {
    for (i = 1; i < val; i++) {
      logMessage(LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "ON", val);
      send_cmd(code);
      waitForButtonState(aq_data, button, ON, 2);
      logMessage(LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "OFF", val);
      send_cmd(code);
      waitForButtonState(aq_data, button, OFF, 2);
    }

    logMessage(LOG_INFO, "Finished - Light Programming button press number %d - %s of %d\n", i, "ON", val);
    send_cmd(code);
  }
  //waitForButtonState(aq_data, &aq_data->aqbuttons[btn], ON, 2);

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
void *set_aqualink_pool_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  char *name;
  char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_POOL_HEATER_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);
  /*
  if (val > HEATER_MAX) {
    val = HEATER_MAX;
  } else if ( val < MEATER_MIN) {
    val = MEATER_MIN;
  }
  */
  val = setpoint_check(POOL_HTR_SETOINT, val, aq_data);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    set_PDA_aqualink_heater_setpoint(aq_data, val, true);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
#endif

  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2
  if (isSINGLE_DEV_PANEL){
    name = "TEMP1";
    menu_name = "SET TEMP1";
  } else {
    name = "POOL";
    menu_name = "SET POOL TEMP";
  }

  logMessage(LOG_DEBUG, "Setting pool heater setpoint to %d\n", val);
  //setAqualinkTemp(aq_data, "SET TEMP", "SET POOL TEMP", NULL, "POOL", val);
  
  if ( select_menu_item(aq_data, "SET TEMP") != true ) {
    logMessage(LOG_WARNING, "Could not select SET TEMP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aq_data, "SET POOL TEMP") != true) {
  if (select_sub_menu_item(aq_data, menu_name) != true) {
    logMessage(LOG_WARNING, "Could not select SET POOL TEMP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  if (isSINGLE_DEV_PANEL){
    // Need to get pass this message 'TEMP1 MUST BE SET HIGHER THAN TEMP2'
    // and get this message 'TEMP1 20�C ~*'
    // Before going to numeric field.
    waitForMessage(threadCtrl->aq_data, "MUST BE SET", 5);
    send_cmd(KEY_LEFT);
    while (stristr(aq_data->last_message, "MUST BE SET") != NULL) { 
      delay(500);
    }
  } 

  //setAqualinkNumericField(aq_data, "POOL", val);
  setAqualinkNumericField(aq_data, name, val);
  
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  waitForMessage(threadCtrl->aq_data, "POOL TEMP IS SET TO", 1); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
void *set_aqualink_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_SPA_HEATER_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);
  char *name;
  char *menu_name;
  /*
  if (val > HEATER_MAX) {
    val = HEATER_MAX;
  } else if ( val < MEATER_MIN) {
    val = MEATER_MIN;
  }*/
  val = setpoint_check(SPA_HTR_SETOINT, val, aq_data);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    set_PDA_aqualink_heater_setpoint(aq_data, val, false);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
#endif
  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2

  if (isSINGLE_DEV_PANEL){
    name = "TEMP2";
    menu_name = "SET TEMP2";
  } else {
    name = "SPA";
    menu_name = "SET SPA TEMP";
  }

  logMessage(LOG_DEBUG, "Setting spa heater setpoint to %d\n", val);
   
  //setAqualinkTemp(aq_data, "SET TEMP", "SET SPA TEMP", NULL, "SPA", val);
  if ( select_menu_item(aq_data, "SET TEMP") != true ) {
    logMessage(LOG_WARNING, "Could not select SET TEMP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aq_data, "SET SPA TEMP") != true) {
  if (select_sub_menu_item(aq_data, menu_name) != true) {
    logMessage(LOG_WARNING, "Could not select SET SPA TEMP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  if (isSINGLE_DEV_PANEL){
    // Need to get pass this message 'TEMP2 MUST BE SET LOWER THAN TEMP1'
    // and get this message 'TEMP2 20�C ~*'
    // Before going to numeric field.
    waitForMessage(threadCtrl->aq_data, "MUST BE SET", 5);
    send_cmd(KEY_LEFT);
    while (stristr(aq_data->last_message, "MUST BE SET") != NULL) { 
      delay(500);
    }
  } 
  
  //setAqualinkNumericField(aq_data, "SPA", val);
  setAqualinkNumericField(aq_data, name, val);
  
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  waitForMessage(threadCtrl->aq_data, "SPA TEMP IS SET TO", 1);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_freeze_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_FRZ_PROTECTION_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);
  /*
  if (val > FREEZE_PT_MAX) {
    val = FREEZE_PT_MAX;
  } else if ( val < FREEZE_PT_MIN) {
    val = FREEZE_PT_MIN;
  }
  */
  val = setpoint_check(FREEZE_SETPOINT, val, aq_data);

  logMessage(LOG_DEBUG, "Setting sfreeze protection to %d\n", val);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    set_PDA_aqualink_freezeprotect_setpoint(aq_data, val);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
#endif
  //setAqualinkTemp(aq_data, "SYSTEM SETUP", "FRZ PROTECT", "TEMP SETTING", "FRZ", val);
  if ( select_menu_item(aq_data, "SYSTEM SETUP") != true ) {
    logMessage(LOG_WARNING, "Could not select SYSTEM SETUP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "FRZ PROTECT") != true) {
    logMessage(LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  if (select_sub_menu_item(aq_data, "TEMP SETTING") != true) {
    logMessage(LOG_WARNING, "Could not select TEMP SETTING menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  setAqualinkNumericField(aq_data, "FRZ", val);
  
  waitForMessage(threadCtrl->aq_data, "FREEZE PROTECTION IS SET TO", 3);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_TIME);
  //logMessage(LOG_NOTICE, "Setting time on aqualink\n");

  time_t now = time(0);   // get time now
  struct tm *result = localtime(&now);
  char hour[11];
  
  if (result->tm_hour == 0)
    sprintf(hour, "HOUR 12 AM");
  else if (result->tm_hour < 11)
    sprintf(hour, "HOUR %d AM", result->tm_hour);
  else if (result->tm_hour == 12)
    sprintf(hour, "HOUR 12 PM");
  else // Must be 13 or more
    sprintf(hour, "HOUR %d PM", result->tm_hour - 12);
  

  logMessage(LOG_DEBUG, "Setting time to %d/%d/%d %d:%d\n", result->tm_mon + 1, result->tm_mday, result->tm_year + 1900, result->tm_hour + 1, result->tm_min);

  if ( select_menu_item(aq_data, "SET TIME") != true ) {
    logMessage(LOG_WARNING, "Could not select SET TIME menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  setAqualinkNumericField(aq_data, "YEAR", result->tm_year + 1900);
  setAqualinkNumericField(aq_data, "MONTH", result->tm_mon + 1);
  setAqualinkNumericField(aq_data, "DAY", result->tm_mday);
  //setAqualinkNumericFieldExtra(aq_data, "HOUR", 11, "PM");
  select_sub_menu_item(aq_data, hour);
  setAqualinkNumericField(aq_data, "MINUTE", result->tm_min);
  
  send_cmd(KEY_ENTER);

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_aqualink_diag_model( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_DIAGNOSTICS_MODEL);
  
  if ( select_menu_item(aq_data, "SYSTEM SETUP") != true ) {
    logMessage(LOG_WARNING, "Could not select HELP menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "DIAGNOSTICS") != true) {
    logMessage(LOG_WARNING, "Could not select DIAGNOSTICS menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  waitForMessage(aq_data, NULL, 8); // Receive 8 messages
  //8157 REV MMM | BATTERY OK | Cal:  -27  0  6 | CONTROL PANEL #1 | CONTROL PANEL #3 | WATER SENSOR OK | AIR SENSOR OK | SOLAR SENSOR OPENED
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_aqualink_pool_spa_heater_temps( void *ptr )
{
//logMessage(LOG_DEBUG, "Could not select TEMP SET menu\n");
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_POOL_SPA_HEATER_TEMPS);
  //logMessage(LOG_NOTICE, "Getting pool & spa heat setpoints from aqualink\n");

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    if (!get_PDA_aqualink_pool_spa_heater_temps(aq_data)) {
      logMessage(LOG_ERR, "Error Getting PDA pool & spa heater setpoints\n");
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
#endif

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    logMessage(LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "TEMP SET") != true) {
    logMessage(LOG_WARNING, "Could not select TEMP SET menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  // Should receive 'POOL TEMP IS SET TO xx' then 'SPA TEMP IS SET TO xx' then 'MAINTAIN TEMP IS (OFF|ON)'
  // wait for the last message
  waitForMessage(threadCtrl->aq_data, "MAINTAIN TEMP IS", 5);
    
  //cancel_menu(threadCtrl->aq_data);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_freeze_protect_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_FREEZE_PROTECT_TEMP);
  //logMessage(LOG_NOTICE, "Getting freeze protection setpoints\n");

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    if (! get_PDA_freeze_protect_temp(aq_data)) {
      logMessage(LOG_ERR, "Error Getting PDA freeze protection setpoints\n");
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
#endif

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    logMessage(LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "FRZ PROTECT") != true) {
    logMessage(LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  waitForMessage(aq_data, "FREEZE PROTECTION IS SET TO", 6); // Changed from 3 to wait for multiple items to be listed
  //cancel_menu(aq_data); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

bool get_aqualink_program_for_button(struct aqualinkdata *aq_data, unsigned char cmd)
{
  int rtnStringsWait = 7;
  
  if (! waitForMessage(aq_data, "SELECT DEVICE TO REVIEW or PRESS ENTER TO END", rtnStringsWait))
    return false;
  
  logMessage(LOG_DEBUG, "Send key '%d'\n",cmd);
  send_cmd(cmd);
  return waitForEitherMessage(aq_data, "NOT SET", "TURNS ON", rtnStringsWait);
}

void *get_aqualink_programs( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  char keys[10] = {KEY_PUMP, KEY_SPA, KEY_AUX1, KEY_AUX2, KEY_AUX3, KEY_AUX4, KEY_AUX5}; // KEY_AUX6 & KEY_AUX7 kill programming mode
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_PROGRAMS);

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    //logMessage(LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "PROGRAMS") != true) {
    //logMessage(LOG_WARNING, "Could not select PROGRAMS menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  unsigned int i;
  
  for (i=0; i < strlen(keys); i++) {
    //logMessage(LOG_DEBUG, "**** START program for key in loop %d\n",i);
    if (! get_aqualink_program_for_button(aq_data, keys[i])) {
      //logMessage(LOG_DEBUG, "**** Didn't find program for key in loop %d\n",i);
      //cancel_menu(aq_data);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    //logMessage(LOG_DEBUG, "**** Found program for key in loop %d\n",i);    
  }
  
  
  if (waitForMessage(aq_data, "SELECT DEVICE TO REVIEW or PRESS ENTER TO END", 6)) {
    //logMessage(LOG_DEBUG, "Send key ENTER\n");
    send_cmd(KEY_ENTER);
  } else {
    //logMessage(LOG_DEBUG, "Send CANCEL\n");
    cancel_menu();
  }
  
  //cancel_menu(aq_data); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

/*
void send_cmd(unsigned char cmd, struct aqualinkdata *aq_data)
{
  push_aq_cmd(cmd);
  logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller\n", cmd);
}
*/

void _waitfor_queue2empty(bool longwait)
{
  int i=0;

  //while ( (_pgm_command != NUL) && ( i++ < (30*(longwait?2:1) ) ) ) {
  while ( (_pgm_command != NUL) && ( i++ < (50*(longwait?2:1) ) ) ) {
    //sleep(1); // NSF Change to smaller time.
    //logMessage(LOG_DEBUG, "********  QUEUE IS FULL ********  delay\n");
    delay(50);
  }

  if (_pgm_command != NUL) {
    #ifdef AQ_PDA
    if (isPDA_PANEL) {
      // Wait for longer in PDA mode since it's slower.
      while ( (_pgm_command != NUL) && ( i++ < (150*(longwait?2:1)) ) ) {
        delay(100);
      }
    }
    #endif
    logMessage(LOG_WARNING, "Send command Queue did not empty, timeout\n");
  }

}

void waitfor_queue2empty()
{
  _waitfor_queue2empty(false);
}
void longwaitfor_queue2empty()
{
  _waitfor_queue2empty(true);
}

void send_cmd(unsigned char cmd)
{
  waitfor_queue2empty();
  
  _pgm_command = cmd;
  //delay(200);

  logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller (programming)\n", _pgm_command);
}

/*
void send_cmd(unsigned char cmd, struct aqualinkdata *aq_data)
{
  int i=0;
  // If there is an unsent command, wait.
  while ( (aq_data->aq_command != NUL) && ( i++ < 10) ) {
    //sleep(1); // NSF Change to smaller time.
    delay(500);
  }
  
  aq_data->aq_command = cmd;
  //delay(200);

  logMessage(LOG_INFO, "Queue send '0x%02hhx' to controller\n", aq_data->aq_command);
}
*/

void cancel_menu()
{
  send_cmd(KEY_CANCEL);
}

/*
* added functionality, if start of string is ^ use that as must start with in comparison
*/

bool waitForEitherMessage(struct aqualinkdata *aq_data, char* message1, char* message2, int numMessageReceived)
{
  //logMessage(LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
  waitfor_queue2empty();  // MAke sure the last command was sent
  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);
  char* msgS1 = "";
  char* msgS2 = "";
  char* ptr = NULL;
  
  
  if (message1 != NULL) {
    if (message1[0] == '^')
      msgS1 = &message1[1];
    else
      msgS1 = message1;
  }
  if (message2 != NULL) {
    if (message2[0] == '^')
      msgS2 = &message2[1];
    else
      msgS2 = message2;
  }
  
  while( ++i <= numMessageReceived)
  {
    logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' OR '%s' received message1 '%s'\n",i,numMessageReceived,message1,message2,aq_data->last_message);
    
    if (message1 != NULL) {
      ptr = stristr(aq_data->last_message, msgS1);
      if (ptr != NULL) { // match
        logMessage(LOG_DEBUG, "Programming mode: String MATCH '%s'\n", msgS1);
        if (msgS1 == message1) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    if (message2 != NULL) {
      ptr = stristr(aq_data->last_message, msgS2);
      if (ptr != NULL) { // match
        logMessage(LOG_DEBUG, "Programming mode: String MATCH '%s'\n", msgS2);
        if (msgS2 == message2) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //logMessage(LOG_DEBUG, "Programming mode: looking for '%s' received message1 '%s'\n",message1,aq_data->last_message);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message1 '%s'\n",i,numMessageReceived,message1,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message1 != NULL && message2 != NULL && ptr == NULL) {
    //logmessage1(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "Programming mode: did not find '%s'\n",message1);
    return false;
  }
  logMessage(LOG_DEBUG, "Programming mode: found message1 '%s' or '%s' in '%s'\n",message1,message2,aq_data->last_message);
  
  return true;
}



bool waitForMessage(struct aqualinkdata *aq_data, char* message, int numMessageReceived)
{
  logMessage(LOG_DEBUG, "waitForMessage %s %d\n",message,numMessageReceived);
  waitfor_queue2empty();  // MAke sure the last command was sent

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);
  char* msgS;
  char* ptr = NULL;
  
  if (message != NULL) {
    if (message[0] == '^')
      msgS = &message[1];
    else
      msgS = message;
  }
  
  while( ++i <= numMessageReceived)
  {
    if (message != NULL)
      logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s', last message received '%s'\n",i,numMessageReceived,message,aq_data->last_message);
    else
      logMessage(LOG_DEBUG, "Programming mode: loop %d of %d waiting for next message, last message received '%s'\n",i,numMessageReceived,aq_data->last_message);

    if (message != NULL) {
      ptr = stristr(aq_data->last_message, msgS);
      if (ptr != NULL) { // match
        logMessage(LOG_DEBUG, "Programming mode: String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //logMessage(LOG_DEBUG, "Programming mode: looking for '%s' received message '%s'\n",message,aq_data->last_message);
    //logMessage(LOG_DEBUG, "*** pthread_cond_wait() sleep\n");
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //logMessage(LOG_DEBUG, "*** pthread_cond_wait() wake\n");
    //logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "Programming mode: did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    logMessage(LOG_DEBUG, "Programming mode: found message '%s' in '%s'\n",message,aq_data->last_message);
  else
    logMessage(LOG_DEBUG, "Programming mode: waited for %d message(s)\n",numMessageReceived);
  
  return true;
}

bool select_menu_item(struct aqualinkdata *aq_data, char* item_string)
{
  char* expectedMsg = "PRESS ENTER* TO SELECT";
  //char* expectedMsg = "PROGRAM";
  int wait_messages = 5;
  bool found = false;
  int tries = 0;
  // Select the MENU and wait to get the RS8 respond.
  
  while (found == false && tries <= 3) {
    send_cmd(KEY_MENU);
    found = waitForMessage(aq_data, expectedMsg, wait_messages);
    tries++;
  }

  if (found == false)
    return false;

  // NSF  This isn't needed and seems to cause issue on some controllers.
  //send_cmd(KEY_ENTER, aq_data);
  //waitForMessage(aq_data, NULL, 1);
  
  return select_sub_menu_item(aq_data, item_string);
}
/*
bool select_menu_item(struct aqualinkdata *aq_data, char* item_string)
{
  char* expectedMsg = "PRESS ENTER* TO SELECT";
  int wait_messages = 6;
  //int i=0;

  // Select the MENU and wait to get the RS8 respond.
  send_cmd(KEY_MENU, aq_data);
 
  if (waitForMessage(aq_data, expectedMsg, wait_messages) == false)
    return false;

  send_cmd(KEY_ENTER, aq_data);
  waitForMessage(aq_data, NULL, 1);
  
  // Blindly wait for next message
  //sendCmdWaitForReturn(aq_data, KEY_ENTER);
  // Can't determin the first response 
  //delay(500);
  
  return select_sub_menu_item(aq_data, item_string);
}
*/

//bool select_sub_menu_item(char* item_string, struct aqualinkdata *aq_data)
bool select_sub_menu_item(struct aqualinkdata *aq_data, char* item_string)
{
  int wait_messages = 25;
  int i=0;
 
  while( (stristr(aq_data->last_message, item_string) == NULL) && ( i++ < wait_messages) )
  {
    logMessage(LOG_DEBUG, "Find item in Menu: loop %d of %d looking for '%s' received message '%s'\n",i,wait_messages,item_string,aq_data->last_message);
    send_cmd(KEY_RIGHT);
    waitForMessage(aq_data, NULL, 1);
  }

  if (stristr(aq_data->last_message, item_string) == NULL) {
    //logMessage(LOG_ERR, "Could not select SUB_MENU of Aqualink control panel\n");
    return false;
  }
  
  logMessage(LOG_DEBUG, "Find item in Menu: loop %d of %d FOUND menu item '%s', sending ENTER command\n",i,wait_messages, item_string);
  // Enter the mode specified by the argument.
  
  
  send_cmd(KEY_ENTER);
  
 
  waitForMessage(aq_data, NULL, 1);
  
  
   //sendCmdWaitForReturn(aq_data, KEY_ENTER);
  
  return true;
 
}

// NSF Need to test this, then use it for the color change mode. 

bool waitForButtonState(struct aqualinkdata *aq_data, aqkey* button, aqledstate state, int numMessageReceived)
{
  //logMessage(LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for state change to '%d' for '%s' \n",i,numMessageReceived,button->led->state,button->name);

    if (button->led->state == state) {
      logMessage(LOG_INFO, "Programming mode: Button State =%d for %s\n",state,button->name);
        break;
    }

    //logMessage(LOG_DEBUG, "Programming mode: looking for '%s' received message '%s'\n",message,aq_data->last_message);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);

  if (numMessageReceived >= i) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "Programming mode: did not find state '%d' for '%s'\n",button->led->state,button->name);
    return false;
  }
  logMessage(LOG_DEBUG, "Programming mode: found state '%d' for '%s'\n",button->led->state,button->name);

  return true;
}

const char *ptypeName(program_type type)
{
  switch (type) {
    case AQ_GET_POOL_SPA_HEATER_TEMPS:
      return "Get Heater setpoints";
    break;
    case AQ_GET_FREEZE_PROTECT_TEMP:
      return "Get Freeze proctect";
    break;
    case AQ_SET_TIME:
      return "Set time";
    break;
    case AQ_SET_POOL_HEATER_TEMP:
      return "Set Pool heater setpoint";
    break;
    case AQ_SET_SPA_HEATER_TEMP:
      return "Set Spa heater setpoint";
    break;
    case AQ_SET_FRZ_PROTECTION_TEMP:
      return "Set Freeze protect setpoint";
    break;
    case AQ_GET_DIAGNOSTICS_MODEL:
      return "Get diagnostics";
    break;
    case AQ_GET_PROGRAMS:
      return "Get programs";
    break;
    case AQ_SET_LIGHTPROGRAM_MODE:
      return "Set light color (using AqualinkD)";
    break;
    case AQ_SET_LIGHTCOLOR_MODE:
      return "Set light color (using Panel)";
    break;
     case AQ_SET_SWG_PERCENT:
      return "Set SWG percent";
    break;
     case AQ_SET_BOOST:
      return "SWG Boost";
    break;
    case AQ_SET_PUMP_RPM:
      return "Set Pump RPM";
    break;
    case AQ_SET_PUMP_VS_PROGRAM:
      return "Set Pump VS Program";
    break;
#ifdef AQ_ONETOUCH
    case AQ_SET_ONETOUCH_PUMP_RPM:
      return "Set OneTouch Pump RPM";
    break;
    case AQ_SET_ONETOUCH_MACRO:
      return "Set OneTouch Macro";
    break;
    case AQ_GET_ONETOUCH_SETPOINTS:
      return "Get OneTouch setpoints";
    break;
    case AQ_SET_ONETOUCH_TIME:
      return "Set OneTouch time";
    break;
    case AQ_SET_ONETOUCH_BOOST:
      return "Set OneTouch Boost";
    break;
    case AQ_SET_ONETOUCH_SWG_PERCENT:
      return "Set OneTouch SWG Percent";
    break;
    case AQ_SET_ONETOUCH_FREEZEPROTECT:
      return "Set OneTouch Freezeprotect";
    break;
    case AQ_SET_ONETOUCH_POOL_HEATER_TEMP:
      return "Set OneTouch Pool Heater Temp";
    break;
    case AQ_SET_ONETOUCH_SPA_HEATER_TEMP:
      return "Set OneTouch Spa Heater Temp";
    break;
#endif
#ifdef AQ_IAQTOUCH
    case AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM:
      return "Set iAqualink Touch Pump VS Program";
    break;
    case AQ_SET_IAQTOUCH_PUMP_RPM:
      return "Set iAqualink Touch Pump RPM";
    break;
    case AQ_GET_IAQTOUCH_VSP_ASSIGNMENT:
      return "Get iAqualink Touch Pump Assignment";
    break;
    case AQ_GET_IAQTOUCH_SETPOINTS:
      return "Get iAqualink Touch Setpoints";
    break;
    case AQ_GET_IAQTOUCH_AUX_LABELS:
      return "Get iAqualink AUX Labels";
    break;
    case AQ_SET_IAQTOUCH_SWG_PERCENT:
      return "Set iAqualink SWG Percent";
    break;
    case AQ_SET_IAQTOUCH_SWG_BOOST:
      return "Set iAqualink Boost";
    break;
    case AQ_SET_IAQTOUCH_SPA_HEATER_TEMP:
      return "Set iAqualink Spa Heater";
    break;
    case AQ_SET_IAQTOUCH_POOL_HEATER_TEMP:
      return "Set iAqualink Pool Heater";
    break;
    case AQ_SET_IAQTOUCH_SET_TIME:
      return "Set iAqualink Set Time";
    break;
#endif
#ifdef AQ_PDA
    case AQ_PDA_INIT:
      return "Init PDA";
    break;
    case AQ_PDA_DEVICE_STATUS:
      return "Get PDA Device status";
    break;
    case AQ_PDA_DEVICE_ON_OFF:
      return "Switch PDA device on/off";
    break;
    case AQ_GET_AUX_LABELS:
      return "Get AUX labels";
    break;
    case AQ_PDA_WAKE_INIT:
      return "PDA init after wake";
    break;
#endif
    case AQP_NULL:
    default:
      return "Unknown";
    break;
  }
}

// Cleaner version of above for UI display purposes.
const char *programtypeDisplayName(program_type type)
{
  switch (type) {
    case AQ_GET_POOL_SPA_HEATER_TEMPS:
    case AQ_GET_ONETOUCH_SETPOINTS:
    case AQ_GET_FREEZE_PROTECT_TEMP:
    case AQ_GET_IAQTOUCH_SETPOINTS:
#ifdef AQ_PDA
    case AQ_PDA_INIT:
#endif
      return "Programming: retrieving setpoints";
    break;
    case AQ_SET_IAQTOUCH_SET_TIME:
    case AQ_SET_ONETOUCH_TIME:
    case AQ_SET_TIME:
      return "Programming: setting time";
    break;
    case AQ_SET_POOL_HEATER_TEMP:
    case AQ_SET_ONETOUCH_POOL_HEATER_TEMP:
    case AQ_SET_SPA_HEATER_TEMP:
    case AQ_SET_ONETOUCH_SPA_HEATER_TEMP:
    case AQ_SET_IAQTOUCH_SPA_HEATER_TEMP:
    case AQ_SET_IAQTOUCH_POOL_HEATER_TEMP:
      return "Programming: setting heater";
    break;
    case AQ_SET_FRZ_PROTECTION_TEMP:
    case AQ_SET_ONETOUCH_FREEZEPROTECT:
      return "Programming: setting Freeze protect";
    break;
    case AQ_GET_DIAGNOSTICS_MODEL:
      return "Programming: retrieving diagnostics";
    break;
    case AQ_GET_PROGRAMS:
      return "Programming: retrieving programs";
    break;
    case AQ_SET_LIGHTPROGRAM_MODE:
    case AQ_SET_LIGHTCOLOR_MODE:
      return "Programming: setting light color";
    break;
    case AQ_SET_SWG_PERCENT:
    case AQ_SET_ONETOUCH_SWG_PERCENT:
    case AQ_SET_IAQTOUCH_SWG_PERCENT:
      return "Programming: setting SWG percent";
    break;
    case AQ_GET_AUX_LABELS:
    case AQ_GET_IAQTOUCH_AUX_LABELS:
      return "Programming: retrieving AUX labels";
    break;
    case AQ_SET_BOOST:
    case AQ_SET_ONETOUCH_BOOST:
    case AQ_SET_IAQTOUCH_SWG_BOOST:
      return "Programming: setting SWG Boost";
    break;
    case AQ_SET_ONETOUCH_PUMP_RPM:
    case AQ_SET_IAQTOUCH_PUMP_RPM:
    case AQ_SET_PUMP_RPM:
    case AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM:
    case AQ_SET_PUMP_VS_PROGRAM:
      return "Programming: setting Pump RPM";
    break;
    case AQ_SET_ONETOUCH_MACRO:
      return "Programming: setting OneTouch Macro";
    break;
    case AQ_GET_IAQTOUCH_VSP_ASSIGNMENT:
      return "Get Pump Assignment";
    break;
#ifdef AQ_PDA
    case AQ_PDA_DEVICE_STATUS:
      return "Programming: retrieving PDA Device status";
    break;
    case AQ_PDA_DEVICE_ON_OFF:
      return "Programming: setting device on/off";
    break;
    case AQ_PDA_WAKE_INIT:
      return "Programming: PDA wakeup";
    break;
#endif
    default:
      return "Programming: please wait!";
    break;
  }
}
