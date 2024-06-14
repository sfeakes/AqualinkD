#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "aqualink.h"
#include "allbutton_aq_programmer.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "color_lights.h"
#include "devices_jandy.h"

bool waitForButtonState(struct aqualinkdata *aq_data, aqkey* button, aqledstate state, int numMessageReceived);
bool waitForMessage(struct aqualinkdata *aq_data, char* message, int numMessageReceived);
bool waitForEitherMessage(struct aqualinkdata *aq_data, char* message1, char* message2, int numMessageReceived);

bool select_sub_menu_item(struct aqualinkdata *aq_data, char* item_string);
bool select_menu_item(struct aqualinkdata *aq_data, char* item_string);

void send_cmd(unsigned char cmd);
void cancel_menu();

void waitfor_queue2empty();
void longwaitfor_queue2empty();

int _expectNextMessage = 0;
unsigned char _allb_last_sent_command = NUL;

#define MAX_STACK 20
int _allb_stack_place = 0;
unsigned char _allb_commands[MAX_STACK];
//unsigned char pgm_allb_commands[MAX_STACK];
unsigned char _allb_pgm_command = NUL;
//unsigned char _ot_allb_pgm_command = NUL;

bool _last_sent_was_cmd = false;

bool push_allb_cmd(unsigned char cmd);

// External view of adding to queue
//void aq_send_cmd(unsigned char cmd) {
void aq_send_allb_cmd(unsigned char cmd) {
  push_allb_cmd(cmd);
}

bool push_allb_cmd(unsigned char cmd) {
  
  //LOG(ALLB_LOG, LOG_NOTICE, "push_aq_cmd '0x%02hhx'\n", cmd);

  if (_allb_stack_place < MAX_STACK) {
    _allb_commands[_allb_stack_place] = cmd;
    _allb_stack_place++;
  } else {
    LOG(ALLB_LOG, LOG_ERR, "Command queue overflow, too many unsent commands to RS control panel\n");
    return false;
  }

  return true;
}

int get_allb_queue_length()
{
  return _allb_stack_place;
}



unsigned char pop_allb_cmd(struct aqualinkdata *aq_data)
{
  unsigned char cmd = NUL;

  if ( _expectNextMessage > 0 &&
       (aq_data->last_packet_type == CMD_MSG || aq_data->last_packet_type == CMD_MSG_LONG || aq_data->last_packet_type == CMD_MSG_LOOP_ST))
  {
    _expectNextMessage=0;
  } else if (_expectNextMessage > 3) {
    // NSF Should probably check this is a status command AND we are in programming mode.
    LOG(ALLB_LOG, LOG_ERR, "Did not receive expected reply from last RS SEND command, resending '0x%02hhx'\n", _allb_last_sent_command); 
    _expectNextMessage=0;
    return _allb_last_sent_command;
  } else if (_expectNextMessage > 0) {
    _expectNextMessage++;
  }

  // Only send commands on status messages and is we are not in programming mode.
  // In programming move, we don't use the queue. 
  // Are we in programming mode and it's not ONETOUCH programming mode
  if (in_programming_mode(aq_data) && ( in_ot_programming_mode(aq_data) == false  && in_iaqt_programming_mode(aq_data) == false )) {
  //if (aq_data->active_thread.thread_id != 0) {
    if ( _allb_pgm_command != NUL && aq_data->last_packet_type == CMD_STATUS) {
      cmd = _allb_pgm_command;
      _allb_pgm_command = NUL;
      LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx' (programming)\n", cmd);
    } else if (_allb_pgm_command != NUL) {
      LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS Waiting to send cmd '0x%02hhx' (programming)\n", _allb_pgm_command);
    } else {
      LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx' empty queue (programming)\n", cmd);
    }
  } else if (_allb_stack_place > 0 && aq_data->last_packet_type == CMD_STATUS ) {
    cmd = _allb_commands[0];
    _allb_stack_place--;
    LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
    //LOG(ALLB_LOG, LOG_NOTICE, "pop_cmd '0x%02hhx'\n", cmd);
    memmove(&_allb_commands[0], &_allb_commands[1], sizeof(unsigned char) * _allb_stack_place ) ;
  } else {
    LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
  }

//printf("RSM sending cmd '0x%02hhx' in reply to '0x%02hhx'\n",cmd,aq_data->last_packet_type);

  if (cmd == KEY_ENTER || cmd == KEY_RIGHT || cmd == KEY_LEFT || cmd == KEY_MENU ) { //KEY_CANCEL KEY_HOLD KEY_OVERRIDE 
    _expectNextMessage=1;
    _allb_last_sent_command = cmd;
  }
  
  return cmd;
}



bool setAqualinkNumericField_new(struct aqualinkdata *aq_data, char *value_label, int value, int increment);
bool setAqualinkNumericField(struct aqualinkdata *aq_data, char *value_label, int value)
{
  return setAqualinkNumericField_new(aq_data, value_label, value, 1);
}
bool setAqualinkNumericField_new(struct aqualinkdata *aq_data, char *value_label, int value, int increment)
{
  LOG(ALLB_LOG, LOG_DEBUG,"Setting menu item '%s' to %d\n",value_label, value);
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
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//LOG(ALLB_LOG, LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    //sscanf(aq_data->last_message, "%s %d%s", leading, &current_val, trailing);
    //sscanf(aq_data->last_message, "%*[^0123456789]%d", &current_val);
    sscanf(&aq_data->last_message[val_len], "%*[^0123456789]%d", &current_val);
    LOG(ALLB_LOG, LOG_DEBUG, "%s set to %d, looking for %d\n",value_label,current_val,value);
    
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
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', to '%d'\n",value_label,value);
      send_cmd(KEY_ENTER);
      break;
    }
  } while(value != current_val); 
  
  return true;
}



bool OLD_setAqualinkNumericField_OLD(struct aqualinkdata *aq_data, char *value_label, int value)
{ // Works for everything but not SWG
  LOG(ALLB_LOG, LOG_DEBUG,"Setting menu item '%s' to %d\n",value_label, value);
  char leading[10];  // description of the field (POOL, SPA, FRZ)
  int current_val;        // integer value of the current set point
  char trailing[10];      // the degrees and scale
  char searchBuf[20];
    
  sprintf(searchBuf, "^%s", value_label);
  
  do 
  {
    if (waitForMessage(aq_data, searchBuf, 3) != true) {
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//LOG(ALLB_LOG, LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    sscanf(aq_data->last_message, "%s %d%s", leading, &current_val, trailing);
    LOG(ALLB_LOG, LOG_DEBUG, "%s set to %d, looking for %d\n",value_label,current_val,value);
    
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

void *set_allbutton_boost( void *ptr )
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

  LOG(ALLB_LOG, LOG_DEBUG, "programming BOOST to %s\n", val==true?"On":"Off");

  if ( select_menu_item(aq_data, "BOOST POOL") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select BOOST POOL menu\n");
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
        //_allb_pgm_command = KEY_ENTER;
        send_cmd(KEY_ENTER);
        LOG(ALLB_LOG, LOG_DEBUG, "**** FOUND STOP BOOST POOL ****\n");
        //waitfor_queue2empty();
        break;
      } else {
        LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d looking for 'STOP BOOST POOL' received message '%s'\n",i,wait_messages,aq_data->last_message);
        delay(200);
        if (stristr(aq_data->last_message, "STOP BOOST POOL") != NULL) {
          //_allb_pgm_command = KEY_ENTER;
          send_cmd(KEY_ENTER);
          LOG(ALLB_LOG, LOG_DEBUG, "**** FOUND STOP BOOST POOL ****\n");
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
      LOG(ALLB_LOG, LOG_WARNING, "Could not select STOP BOOST POOL menu\n");
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


void *set_allbutton_SWG( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_SWG_PERCENT);

  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SWG_SETPOINT, val, aq_data);

  //LOG(ALLB_LOG, LOG_NOTICE, "programming SWG percent to %d\n", val);

  if ( select_menu_item(aq_data, "SET AQUAPURE") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET AQUAPURE menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // If spa is on, set SWG for spa, if not set SWG for pool
  if (aq_data->aqbuttons[SPA_INDEX].led->state != OFF) {
    if (select_sub_menu_item(aq_data, "SET SPA SP") != true) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu for SPA\n");
      LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    setAqualinkNumericField_new(aq_data, "SPA SP", val, 5);
  } else {
    if (select_sub_menu_item(aq_data, "SET POOL SP") != true) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu\n");
      LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
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
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu\n");
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



void *get_allbutton_aux_labels( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_AUX_LABELS);

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "AUX LABELS") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select AUX LABELS menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  waitForMessage(aq_data, NULL, 5); // Receive 5 messages

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_allbutton_light_colormode( void *ptr )
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
  bool use_current_mode = false;

  if (btn < 0 || btn >= aq_data->total_buttons ) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aq_data->aqbuttons[btn];
  unsigned char code = button->code;

  //LOG(ALLB_LOG, LOG_NOTICE, "Light Programming #: %d, on button: %s, color light type: %d\n", val, button->label, typ);
  
  if (val <= 0) {
    use_current_mode = true;
    LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, color light type: %d, using current mode\n", val, button->label, typ);
  } else {
    mode_name = light_mode_name(typ, val-1, ALLBUTTON);
    use_current_mode = false;
    if (mode_name == NULL) {
      LOG(ALLB_LOG, LOG_ERR, "Light Programming #: %d, on button: %s, color light type: %d, couldn't find mode name '%s'\n", val, button->label, typ, mode_name);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    } else {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, color light type: %d, name '%s'\n", val, button->label, typ, mode_name);
    }
  }
/*
  // Simply turn the light off if value is 0
  if (val <= 0) {
    if ( button->led->state == ON ) {
      send_cmd(code);
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
*/
  // Needs to start programming sequence with light off
  if ( button->led->state == ON ) {
    LOG(ALLB_LOG, LOG_INFO, "Light Programming Initial state on, turning off\n");
    send_cmd(code);
    waitfor_queue2empty();
    if ( !waitForMessage(threadCtrl->aq_data, "OFF", 5)) // Message like 'Aux3 Off'
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive OFF message\n");
  }

  // Now turn on and wait for the message "color mode name<>*"
  send_cmd(code);
  waitfor_queue2empty();
  i=0;
  int waitCounter=12;

  do{
    LOG(ALLB_LOG, LOG_INFO,"Light program wait for message\n");
    if ( !waitForMessage(threadCtrl->aq_data, "~*", waitCounter))
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive color light mode message\n");
    
    // Wait for less messages after first try. We get a lot of repeat messages before the one we need.
    waitCounter = 3;

    if (use_current_mode) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming using color mode %s\n",aq_data->last_message);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    } else if (strncasecmp(aq_data->last_message, mode_name, strlen(mode_name)) == 0) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming found color mode %s\n",mode_name);
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
    LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive color light mode message for '%s'\n",use_current_mode?"light program":mode_name);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_allbutton_light_programmode( void *ptr )
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
    LOG(ALLB_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aq_data->aqbuttons[btn];
  unsigned char code = button->code;

  LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, with pause mode: %f (initial on=%d, initial off=%d)\n", val, button->label, pmode, iOn, iOff);

  // Simply turn the light off if value is 0
  if (val <= 0) {
    if ( button->led->state == ON ) {
      send_cmd(code);
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  const int seconds = 1000;
  // Needs to start programming sequence with light on, if off we need to turn on for 15 seconds
  // before we can send the next off.
  if ( button->led->state != ON ) {
    LOG(ALLB_LOG, LOG_INFO, "Light Programming Initial state off, turning on for %d seconds\n",iOn);
    send_cmd(code);
    delay(iOn * seconds);
  }

  LOG(ALLB_LOG, LOG_INFO, "Light Programming turn off for %d seconds\n",iOff);
  // Now need to turn off for between 11 and 14 seconds to reset light.
  send_cmd(code);
  delay(iOff * seconds);

  // Now light is reset, pulse the appropiate number of times to advance program.
  LOG(ALLB_LOG, LOG_INFO, "Light Programming button pulsing on/off %d times\n", val);
 
  // Program light in safe mode (slowley), or quick mode
  if (pmode > 0) {
    for (i = 1; i < (val * 2); i++) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, i % 2 == 0 ? "Off" : "On", val);
      send_cmd(code);
      waitfor_queue2empty();
      delay(pmode * seconds); // 0.3 works, but using 0.4 to be safe
    }
  } else {
    for (i = 1; i < val; i++) {
      const int dt = 0.5;  // Time to wait after receiving conformation of light on/off
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "ON", val);
      send_cmd(code);
      waitForButtonState(aq_data, button, ON, 2);
      delay(dt * seconds);
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "OFF", val);
      send_cmd(code);
      waitForButtonState(aq_data, button, OFF, 2);
      delay(dt * seconds);
    }
    LOG(ALLB_LOG, LOG_INFO, "Finished - Light Programming button press number %d - %s of %d\n", i, "ON", val);
    send_cmd(code);
    waitfor_queue2empty();
  }
  //waitForButtonState(aq_data, &aq_data->aqbuttons[btn], ON, 2);

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
void *set_allbutton_pool_heater_temps( void *ptr )
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

  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2
  if (isSINGLE_DEV_PANEL){
    name = "TEMP1";
    menu_name = "SET TEMP1";
  } else {
    name = "POOL";
    menu_name = "SET POOL TEMP";
  }

  LOG(ALLB_LOG, LOG_DEBUG, "Setting pool heater setpoint to %d\n", val);
  //setAqualinkTemp(aq_data, "SET TEMP", "SET POOL TEMP", NULL, "POOL", val);
  
  if ( select_menu_item(aq_data, "SET TEMP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aq_data, "SET POOL TEMP") != true) {
  if (select_sub_menu_item(aq_data, menu_name) != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET POOL TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
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
void *set_allbutton_spa_heater_temps( void *ptr )
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

  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2

  if (isSINGLE_DEV_PANEL){
    name = "TEMP2";
    menu_name = "SET TEMP2";
  } else {
    name = "SPA";
    menu_name = "SET SPA TEMP";
  }

  LOG(ALLB_LOG, LOG_DEBUG, "Setting spa heater setpoint to %d\n", val);
   
  //setAqualinkTemp(aq_data, "SET TEMP", "SET SPA TEMP", NULL, "SPA", val);
  if ( select_menu_item(aq_data, "SET TEMP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aq_data, "SET SPA TEMP") != true) {
  if (select_sub_menu_item(aq_data, menu_name) != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET SPA TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
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

void *set_allbutton_freeze_heater_temps( void *ptr )
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

  LOG(ALLB_LOG, LOG_DEBUG, "Setting sfreeze protection to %d\n", val);

  //setAqualinkTemp(aq_data, "SYSTEM SETUP", "FRZ PROTECT", "TEMP SETTING", "FRZ", val);
  if ( select_menu_item(aq_data, "SYSTEM SETUP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SYSTEM SETUP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "FRZ PROTECT") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  if (select_sub_menu_item(aq_data, "TEMP SETTING") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select TEMP SETTING menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
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

void *set_allbutton_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_TIME);
  //LOG(ALLB_LOG, LOG_NOTICE, "Setting time on aqualink\n");

  time_t now = time(0);   // get time now
  struct tm *result = localtime(&now);
  char hour[20];

  // Add 10 seconds to time since this can take a while to program.
  // 10 to 20 seconds whould be right, but since there are no seconds we can set, add 30 seconds to get close to minute.
  // Should probably set this to program the next minute then wait before hitting the final enter command.
  result->tm_sec += 30;
  mktime(result);
  
  if (result->tm_hour == 0)
    sprintf(hour, "HOUR 12 AM");
  else if (result->tm_hour <= 11)
    sprintf(hour, "HOUR %d AM", result->tm_hour); // Need to fix compiler warning on new GCC, but %2d or %.2d does NOT give correct string
  else if (result->tm_hour == 12)
    sprintf(hour, "HOUR 12 PM");
  else // Must be 13 or more
    sprintf(hour, "HOUR %d PM", result->tm_hour - 12);
  

  LOG(ALLB_LOG, LOG_DEBUG, "Setting time to %d/%d/%d %d:%d\n", result->tm_mon + 1, result->tm_mday, result->tm_year + 1900, result->tm_hour + 1, result->tm_min);

  if ( select_menu_item(aq_data, "SET TIME") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TIME menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  setAqualinkNumericField(aq_data, "YEAR", result->tm_year + 1900);
  setAqualinkNumericField(aq_data, "MONTH", result->tm_mon + 1);
  setAqualinkNumericField(aq_data, "DAY", result->tm_mday);
  //setAqualinkNumericFieldExtra(aq_data, "HOUR", 11, "PM");
  select_sub_menu_item(aq_data, hour); // This will keep looping until it finds the right message
  setAqualinkNumericField(aq_data, "MINUTE", result->tm_min);
  
  send_cmd(KEY_ENTER);

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_allbutton_diag_model( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_DIAGNOSTICS_MODEL);
  
  if ( select_menu_item(aq_data, "SYSTEM SETUP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select HELP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "DIAGNOSTICS") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select DIAGNOSTICS menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aq_data->active_thread.ptype ) );
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

void *get_allbutton_pool_spa_heater_temps( void *ptr )
{
//LOG(ALLB_LOG, LOG_DEBUG, "Could not select TEMP SET menu\n");
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_POOL_SPA_HEATER_TEMPS);
  //LOG(ALLB_LOG, LOG_NOTICE, "Getting pool & spa heat setpoints from aqualink\n");

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get heater setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "TEMP SET") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select TEMP SET menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get heater setpoints from Control Panel\n");
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

void *get_allbutton_freeze_protect_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_FREEZE_PROTECT_TEMP);
  //LOG(ALLB_LOG, LOG_NOTICE, "Getting freeze protection setpoints\n");

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get freeze setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "FRZ PROTECT") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get freeze setpoints from Control Panel\n");
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
  
  LOG(ALLB_LOG, LOG_DEBUG, "Send key '%d'\n",cmd);
  send_cmd(cmd);
  return waitForEitherMessage(aq_data, "NOT SET", "TURNS ON", rtnStringsWait);
}

void *get_allbutton_programs( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  char keys[10] = {KEY_PUMP, KEY_SPA, KEY_AUX1, KEY_AUX2, KEY_AUX3, KEY_AUX4, KEY_AUX5}; // KEY_AUX6 & KEY_AUX7 kill programming mode
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_PROGRAMS);

  if ( select_menu_item(aq_data, "REVIEW") != true ) {
    //LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aq_data, "PROGRAMS") != true) {
    //LOG(ALLB_LOG, LOG_WARNING, "Could not select PROGRAMS menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  unsigned int i;
  
  for (i=0; i < strlen(keys); i++) {
    //LOG(ALLB_LOG, LOG_DEBUG, "**** START program for key in loop %d\n",i);
    if (! get_aqualink_program_for_button(aq_data, keys[i])) {
      //LOG(ALLB_LOG, LOG_DEBUG, "**** Didn't find program for key in loop %d\n",i);
      //cancel_menu(aq_data);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    //LOG(ALLB_LOG, LOG_DEBUG, "**** Found program for key in loop %d\n",i);    
  }
  
  
  if (waitForMessage(aq_data, "SELECT DEVICE TO REVIEW or PRESS ENTER TO END", 6)) {
    //LOG(ALLB_LOG, LOG_DEBUG, "Send key ENTER\n");
    send_cmd(KEY_ENTER);
  } else {
    //LOG(ALLB_LOG, LOG_DEBUG, "Send CANCEL\n");
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
  LOG(ALLB_LOG, LOG_INFO, "Queue send '0x%02hhx' to controller\n", cmd);
}
*/



void _waitfor_queue2empty(bool longwait)
{
  int i=0;

  if (_allb_pgm_command != NUL) {
    LOG(ALLB_LOG, LOG_DEBUG, "Waiting for queue to empty\n");
  } else {
    LOG(ALLB_LOG, LOG_DEBUG, "Queue empty!\n");
    return;
  }

  while ( (_allb_pgm_command != NUL) && ( i++ < (PROGRAMMING_POLL_COUNTER*(longwait?2:1) ) ) ) {
    delay(PROGRAMMING_POLL_DELAY_TIME);
  }

  if (_allb_pgm_command != NUL) {
    LOG(ALLB_LOG, LOG_WARNING, "Send command Queue did not empty, timeout\n");
  } else {
    LOG(ALLB_LOG, LOG_DEBUG, "Queue now empty!\n");
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
  
  _allb_pgm_command = cmd;
  //delay(200);

  LOG(ALLB_LOG, LOG_INFO, "Queue send '0x%02hhx' to controller (programming)\n", _allb_pgm_command);
}
void force_queue_delete()
{
  if (_allb_pgm_command != NUL)
    LOG(ALLB_LOG, LOG_INFO, "Really bad coding, don't use this in release\n");

  _allb_pgm_command = NUL;
}


void cancel_menu()
{
  send_cmd(KEY_CANCEL);
}

/*
* added functionality, if start of string is ^ use that as must start with in comparison
*/

bool waitForEitherMessage(struct aqualinkdata *aq_data, char* message1, char* message2, int numMessageReceived)
{
  //LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
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
    LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' OR '%s' received message1 '%s'\n",i,numMessageReceived,message1,message2,aq_data->last_message);
    
    if (message1 != NULL) {
      ptr = stristr(aq_data->last_message, msgS1);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH '%s'\n", msgS1);
        if (msgS1 == message1) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    if (message2 != NULL) {
      ptr = stristr(aq_data->last_message, msgS2);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH '%s'\n", msgS2);
        if (msgS2 == message2) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message1 '%s'\n",message1,aq_data->last_message);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message1 '%s'\n",i,numMessageReceived,message1,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message1 != NULL && message2 != NULL && ptr == NULL) {
    //logmessage1(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_ERR, "Did not find '%s'\n",message1);
    return false;
  }
  LOG(ALLB_LOG, LOG_DEBUG, "Found message1 '%s' or '%s' in '%s'\n",message1,message2,aq_data->last_message);
  
  return true;
}



bool waitForMessage(struct aqualinkdata *aq_data, char* message, int numMessageReceived)
{
  LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d\n",message,numMessageReceived);
  // NSF Need to come back to this, as it stops on test enviornment but not real panel, so must be speed related.
  //waitfor_queue2empty();  // MAke sure the last command was sent

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
      LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s', last message received '%s'\n",i,numMessageReceived,message,aq_data->last_message);
    else
      LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d waiting for next message, last message received '%s'\n",i,numMessageReceived,aq_data->last_message);

    if (message != NULL) {
      ptr = stristr(aq_data->last_message, msgS);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message '%s'\n",message,aq_data->last_message);
    //LOG(ALLB_LOG, LOG_DEBUG, "*** pthread_cond_wait() sleep\n");
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "*** pthread_cond_wait() wake\n");
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //LOG(ALLB_LOG, LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_DEBUG, "did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    LOG(ALLB_LOG, LOG_DEBUG, "found message '%s' in '%s'\n",message,aq_data->last_message);
  else
    LOG(ALLB_LOG, LOG_DEBUG, "waited for %d message(s)\n",numMessageReceived);
  
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
  int wait_messages = 28;
  int i=0;
 
  waitfor_queue2empty();

  while( (stristr(aq_data->last_message, item_string) == NULL) && ( i++ < wait_messages) )
  {
    LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d looking for '%s' received message '%s'\n",i,wait_messages,item_string,aq_data->last_message);
    send_cmd(KEY_RIGHT);
    waitfor_queue2empty(); // ADDED BACK MAY 2023 setting time warked better
    //waitForMessage(aq_data, NULL, 1);
    waitForMessage(aq_data, item_string, 1);
  }

  if (stristr(aq_data->last_message, item_string) == NULL) {
    LOG(ALLB_LOG, LOG_ERR, "Could not find menu item '%s'\n",item_string);
    return false;
  }
  
  LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d FOUND menu item '%s', sending ENTER command\n",i,wait_messages, item_string);
  // Enter the mode specified by the argument.
  
  
  send_cmd(KEY_ENTER);
  
 
  waitForMessage(aq_data, NULL, 1);
  
  
   //sendCmdWaitForReturn(aq_data, KEY_ENTER);
  
  return true;
 
}

// NSF Need to test this, then use it for the color change mode. 

bool waitForButtonState(struct aqualinkdata *aq_data, aqkey* button, aqledstate state, int numMessageReceived)
{
  //LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for state change to '%d' for '%s' \n",i,numMessageReceived,button->led->state,button->name);

    if (button->led->state == state) {
      LOG(ALLB_LOG, LOG_INFO, "Button State =%d for %s\n",state,button->name);
        break;
    }

    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message '%s'\n",message,aq_data->last_message);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);

  if (numMessageReceived >= i) {
    //LOG(ALLB_LOG, LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_DEBUG, "did not find state '%d' for '%s'\n",button->led->state,button->name);
    return false;
  }
  LOG(ALLB_LOG, LOG_DEBUG, "found state '%d' for '%s'\n",button->led->state,button->name);

  return true;
}