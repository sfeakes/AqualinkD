#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "aqualink.h"
#include "allbutton_aq_programmer.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "color_lights.h"
#include "devices_jandy.h"
#include "timespec_subtract.h"



bool waitForButtonState(struct aqualinkdata *aqdata, aqkey* button, aqledstate state, int numMessageReceived);
bool waitForMessage(struct aqualinkdata *aqdata, const char* message, int numMessageReceived);
bool waitForEitherMessage(struct aqualinkdata *aqdata, const char* message1, const char* message2, int numMessageReceived);

bool select_sub_menu_item(struct aqualinkdata *aqdata, const char* item_string);
bool select_menu_item(struct aqualinkdata *aqdata, const char* item_string);
bool select_menu_item_noerror(struct aqualinkdata *aqdata, const char* item_string);

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



unsigned char pop_allb_cmd(struct aqualinkdata *aqdata)
{
  unsigned char cmd = NUL;

  if ( _expectNextMessage > 0 &&
       (aqdata->last_packet_type == CMD_MSG || aqdata->last_packet_type == CMD_MSG_LONG || aqdata->last_packet_type == CMD_MSG_LOOP_ST))
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
  if (in_programming_mode(aqdata) && ( in_ot_programming_mode(aqdata) == false  && in_iaqt_programming_mode(aqdata) == false )) {
  //if (aqdata->active_thread.thread_id != 0) {
    if ( _allb_pgm_command != NUL && aqdata->last_packet_type == CMD_STATUS) {
      cmd = _allb_pgm_command;
      _allb_pgm_command = NUL;
      if (cmd) {
        LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd %s '0x%02hhx' (programming)\n", cmd_to_string(cmd), cmd);
      }
    } else if (_allb_pgm_command != NUL) {
      LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS Waiting to send cmd '0x%02hhx' (programming)\n", _allb_pgm_command);
    } else {
      LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx' empty queue (programming)\n", cmd);
    }
  } else if (_allb_stack_place > 0 && aqdata->last_packet_type == CMD_STATUS ) {
    cmd = _allb_commands[0];
    _allb_stack_place--;
    LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
    //LOG(ALLB_LOG, LOG_NOTICE, "pop_cmd '0x%02hhx'\n", cmd);
    memmove(&_allb_commands[0], &_allb_commands[1], sizeof(unsigned char) * _allb_stack_place ) ;
  } else {
    LOG(ALLB_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
  }

//printf("RSM sending cmd '0x%02hhx' in reply to '0x%02hhx'\n",cmd,aqdata->last_packet_type);

  if (cmd == KEY_ENTER || cmd == KEY_RIGHT || cmd == KEY_LEFT || cmd == KEY_MENU ) { //KEY_CANCEL KEY_HOLD KEY_OVERRIDE 
    _expectNextMessage=1;
    _allb_last_sent_command = cmd;
  }
  
  return cmd;
}



bool setAqualinkNumericField_ex(struct aqualinkdata *aqdata, char *value_label, int value, int increment, bool sendEnter);
bool setAqualinkNumericField(struct aqualinkdata *aqdata, char *value_label, int value)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, 1, true);
}
bool setAqualinkNumericField_new(struct aqualinkdata *aqdata, char *value_label, int value, int increment)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, increment, true);
}
// Set the field but leave it unaccepted, for the last field of a menu where the caller times the ENTER.
bool setAqualinkNumericField_noenter(struct aqualinkdata *aqdata, char *value_label, int value)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, 1, false);
}
bool setAqualinkNumericField_ex(struct aqualinkdata *aqdata, char *value_label, int value, int increment, bool sendEnter)
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
    if (waitForMessage(aqdata, searchBuf, 4) != true) {
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//LOG(ALLB_LOG, LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    //sscanf(aqdata->last_message, "%s %d%s", leading, &current_val, trailing);
    //sscanf(aqdata->last_message, "%*[^0123456789]%d", &current_val);
    sscanf(&aqdata->last_message[val_len], "%*[^0123456789]%d", &current_val);
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
      if (sendEnter)
        send_cmd(KEY_ENTER);
    }

    if (i++ >= 100) {
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', to '%d'\n",value_label,value);
      if (sendEnter)
        send_cmd(KEY_ENTER);
      return false;   // was 'break', which reported a field it never set as success
    }
  } while(value != current_val); 
  
  return true;
}



bool OLD_setAqualinkNumericField_OLD(struct aqualinkdata *aqdata, char *value_label, int value)
{ // Works for everything but not SWG
  LOG(ALLB_LOG, LOG_DEBUG,"Setting menu item '%s' to %d\n",value_label, value);
  char leading[10];  // description of the field (POOL, SPA, FRZ)
  int current_val;        // integer value of the current set point
  char trailing[10];      // the degrees and scale
  char searchBuf[20];
    
  sprintf(searchBuf, "^%s", value_label);
  
  do 
  {
    if (waitForMessage(aqdata, searchBuf, 3) != true) {
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', not found\n",value_label);
      cancel_menu();
      return false;
    }
//LOG(ALLB_LOG, LOG_DEBUG,"WAITING for kick value=%d\n",current_val);     
    sscanf(aqdata->last_message, "%s %d%s", leading, &current_val, trailing);
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
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SEND_CMD);

  send_cmd( (unsigned char)*threadCtrl->thread_args, aqdata);
  
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}
*/


typedef enum { SWG_M_BOOST_UNKNOWN = 0, SWG_M_BOOST, SWG_M_SUPERCHLORINATE } swg_boost_menu_t;

// Returns true if a menu was selected; remembers which one worked.
static bool select_chlorinator_boost_menu(struct aqualinkdata *aqdata)
{
  static swg_boost_menu_t s_swg_boost_menu = SWG_M_BOOST_UNKNOWN;   // detected on first successful run
  const bool can_be_superchlor = (aqdata->panel_rev[0] < 'L');

  switch (s_swg_boost_menu) {
    case SWG_M_BOOST:
      return select_menu_item(aqdata, "BOOST POOL");

    case SWG_M_SUPERCHLORINATE:
      return select_menu_item(aqdata, "SUPERCHLORINATE");

    default:  // SWG_BOOST_UNKNOWN: probe once
      if (select_menu_item_noerror(aqdata, "BOOST POOL")) {
        s_swg_boost_menu = SWG_M_BOOST;
        return true;
      }
      if (can_be_superchlor && select_menu_item_noerror(aqdata, "SUPERCHLORINATE")) {
        s_swg_boost_menu = SWG_M_SUPERCHLORINATE;
        return true;
      }
      s_swg_boost_menu = SWG_M_BOOST_UNKNOWN;   // optional: re-detect next run
      return false;
  }
}

void *set_allbutton_boost( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

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

#ifdef NEW_AQ_PROGRAMMER
  int val = threadCtrl->pArgs.value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  // stop/start Messages when BOOT is the term are below.
  //const char *start_msg = "TO START BOOST POOL";
  //const char *stop_msg = "TO STOP BOOST POOL";
  // stop/start Messages when SUPERCHLORINATE are unknown, so just look for START/STOP in message.
  const char *start_msg = "START";
  const char *stop_msg = "STOP";

  LOG(ALLB_LOG, LOG_DEBUG, "programming BOOST to %s\n", val==true?"On":"Off");

  //if ( select_menu_item(aqdata, "BOOST POOL") != true ) {
  if ( select_chlorinator_boost_menu(aqdata) != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select BOOST POOL menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  if (val==true) {
    waitForMessage(threadCtrl->aqdata, start_msg, 5);
    send_cmd(KEY_ENTER);
    longwaitfor_queue2empty();
  } else {
    int wait_messages = 5;
    int i=0;
    while( i++ < wait_messages) 
    {
      waitForMessage(aqdata, stop_msg, 1);
      if (stristr(aqdata->last_message, stop_msg) != NULL) {
        // This is a really bad hack, message sequence is out for boost for some reason, so as soon as we see stop message, force enter key.
        //_allb_pgm_command = KEY_ENTER;
        send_cmd(KEY_ENTER);
        LOG(ALLB_LOG, LOG_DEBUG, "**** FOUND STOP BOOST POOL ****\n");
        //waitfor_queue2empty();
        break;
      } else {
        LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d looking for 'STOP BOOST POOL' received message '%s'\n",i,wait_messages,aqdata->last_message);
        delay(200);
        if (stristr(aqdata->last_message, stop_msg) != NULL) {
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
      //waitForMessage(aqdata, NULL, 1);
    }
    if (i < wait_messages) {
      // Takes ages to see bost is off from menu, to set it here.
      setSWGboost(aqdata, false);
    }
    /*
    // Extra message overcome.
    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    if ( select_sub_menu_item(aqdata, "STOP BOOST POOL") != true ) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not select STOP BOOST POOL menu\n");
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }*/
    //send_cmd(KEY_ENTER);
  }

  waitForMessage(aqdata,NULL, 1);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


typedef enum { SWG_UNKNOWN = 0, SWG_AQUAPURE, SWG_AQUARITE } swg_menu_t;

// Returns true if a menu was selected; remembers which one worked.
static int select_chlorinator_menu(struct aqualinkdata *aqdata)
{
  static swg_menu_t s_swg_menu = SWG_UNKNOWN;   // detected on first successful run
  //const bool can_be_aquarite = (aqdata->panel_rev[0] < 'L');

  switch (s_swg_menu) {
    case SWG_AQUAPURE:
      return select_menu_item(aqdata, "SET AQUAPURE")==true?SWG_AQUAPURE:SWG_UNKNOWN;

    case SWG_AQUARITE:
      return select_menu_item(aqdata, "SET AQUARITE")==true?SWG_AQUARITE:SWG_UNKNOWN;

    default:  // SWG_UNKNOWN: probe once
      if (select_menu_item_noerror(aqdata, "SET AQUAPURE")) {
        s_swg_menu = SWG_AQUAPURE;
        return SWG_AQUAPURE;
      }
      if (select_menu_item_noerror(aqdata, "SET AQUARITE")) {
        s_swg_menu = SWG_AQUARITE;
        return SWG_AQUARITE;
      }
      s_swg_menu = SWG_UNKNOWN;   // optional: re-detect next run
      return SWG_UNKNOWN;
  }
}

void *set_allbutton_SWG( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_SWG_PERCENT);

#ifdef NEW_AQ_PROGRAMMER
  int val = threadCtrl->pArgs.value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  val = setpoint_check(SWG_SETPOINT, val, aqdata);

  //LOG(ALLB_LOG, LOG_NOTICE, "programming SWG percent to %d\n", val);

  // On revisions I to K (prior to L) can also see "SET AquaRite" in menu
  int swg_type = select_chlorinator_menu(aqdata);

  if (swg_type == SWG_UNKNOWN) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET AQUAPURE/AQUARITE menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName(aqdata->active_thread.ptype));
    //_s_swg_menu = SWG_UNKNOWN;   // optional: re-detect next run
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }


  // If spa is on, set SWG for spa, if not set SWG for pool
  if (doesPanelSupport(aqdata, RSP_SUP_SWG_PS) == RSP_NOT_SUPPORTED){
    // confirmed panel L is incrment of 5, not 1 as documentation mentions 
    setAqualinkNumericField_new(aqdata, (swg_type==SWG_AQUAPURE?"AQUAPURE":"AQUARITE"), val, 5); // Could be Aquarite or Aquapure. Also increment or 1 not 5
  } else if ( isSINGLE_DEV_PANEL ) {
    setAqualinkNumericField_new(aqdata, (swg_type==SWG_AQUAPURE?"AQUAPURE":"AQUARITE"), val, 5); // Could be Aquarite or Aquapure. Also increment 5
  } else {
    if (aqdata->aqbuttons[SPA_INDEX].led->state != OFF) {
      if (select_sub_menu_item(aqdata, "SET SPA SP") != true) {
        LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu for SPA\n");
        LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName(aqdata->active_thread.ptype));
        cancel_menu();
        cleanAndTerminateThread(threadCtrl);
        return ptr;
      }
      setAqualinkNumericField_new(aqdata, "SPA SP", val, 5);
    } else {
      if (select_sub_menu_item(aqdata, "SET POOL SP") != true)
      {
        LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu\n");
        LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName(aqdata->active_thread.ptype));
        cancel_menu();
        cleanAndTerminateThread(threadCtrl);
        return ptr;
      }
      setAqualinkNumericField_new(aqdata, "POOL SP", val, 5);
    }
  }

  // Let everyone know we set SWG, if it failed we will update on next message, unless it's 0.
  setSWGpercent(aqdata, val); // Don't use changeSWGpercent as we are in programming mode.

/*
  if (select_sub_menu_item(aqdata, "SET POOL SP") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu\n");
    cancel_menu(aqdata);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  setAqualinkNumericField_new(aqdata, "POOL SP", val, 5);
*/
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  // NSF have see the below message RS Message :-
  // 'Pool set to 20%'
  // 'POOL SP IS SET TO 20%'
  waitForMessage(threadCtrl->aqdata, "SET TO", 1);
  //waitForMessage(threadCtrl->aqdata, "POOL SP IS SET TO", 1);

  // Since we read % directly from RS message, wait for another few messages that way
  // We won't registed a SWG bounce, since we already told clients SWG was at new % before programming started
  waitForMessage(threadCtrl->aqdata, NULL, 1);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}



void *get_allbutton_aux_labels( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_AUX_LABELS);

  if ( select_menu_item(aqdata, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "AUX LABELS") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select AUX LABELS menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  waitForMessage(aqdata, NULL, 5); // Receive 5 messages

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_allbutton_light_colormode( void *ptr )
{
  int i;
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_LIGHTCOLOR_MODE);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int val = pargs->value;
  //bool useDefaultIfValid = pargs->alt_value;
  bool use_current_mode = false;
  const char *mode_name;
#else
  char *buf = (char*)threadCtrl->thread_args;
  const char *mode_name;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int typ = atoi(&buf[10]);
  bool use_current_mode = false;


  if (btn < 0 || btn >= aqdata->total_buttons ) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aqdata->aqbuttons[btn];
  unsigned char code = button->code;
#endif

  if (!isPLIGHT(button->special_mask)) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light for button '%d', configuration is incorrect\n", button->label);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  //LOG(ALLB_LOG, LOG_NOTICE, "Light Programming #: %d, on button: %s, color light type: %d\n", val, button->label, typ);
  
  if (val <= 0) {
    use_current_mode = true;
    LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, color light type: %d, using current mode\n", val, button->label, ((clight_detail *)button->special_mask_ptr)->lightType);
  } else {
    mode_name = light_mode_name(((clight_detail *)button->special_mask_ptr)->lightType, val, ALLBUTTON); // NSF DO NOT -1 from value (check WS logic if you do)
    use_current_mode = false;
    if (mode_name == NULL) {
      LOG(ALLB_LOG, LOG_ERR, "Light Programming #: %d, on button: %s, color light type: %d, couldn't find mode name '%s'\n", val, button->label, ((clight_detail *)button->special_mask_ptr)->lightType, mode_name);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    } else {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, color light type: %d, color name '%s'\n", val, button->label, ((clight_detail *)button->special_mask_ptr)->lightType, mode_name);
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
    send_cmd(button->code);
    waitfor_queue2empty();
    if ( !waitForMessage(threadCtrl->aqdata, "OFF", 5)) // Message like 'Aux3 Off'
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive OFF message\n");
  }

  // Now turn on and wait for the message "color mode name<>*"
  send_cmd(button->code);
  waitfor_queue2empty();
  i=0;
  int waitCounter=12;

  do{
    LOG(ALLB_LOG, LOG_INFO,"Light program wait for message\n");
    if ( !waitForMessage(threadCtrl->aqdata, "~*", waitCounter))
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive color light mode message\n");
    
    // Wait for less messages after first try. We get a lot of repeat messages before the one we need.
    waitCounter = 3;

    if (use_current_mode) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming using color mode %s\n",aqdata->last_message);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    } else if (strncasecmp(aqdata->last_message, mode_name, strlen(mode_name)) == 0) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming found color mode %s\n",mode_name);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    }

    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    // Just clear current message before waiting for next, since the test in the do can't distinguish
    // as both messages end in "~*"
    waitForMessage(threadCtrl->aqdata, NULL, 1); 

    i++;
  } while (i <= LIGHT_COLOR_OPTIONS);

  if (i == LIGHT_COLOR_OPTIONS) {
    LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive color light mode message for '%s'\n",use_current_mode?"light program":mode_name);
  } else {
    // Set before we are called.
    //updateButtonLightProgram(aqdata, val, btn);
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
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  const int seconds = 1000;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_LIGHTPROGRAM_MODE);


#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int val = pargs->value;
  //bool useDefaultIfValid = pargs->alt_value;
  //bool use_current_mode = false;
  //const char *mode_name;
#else
  char *buf = (char*)threadCtrl->thread_args;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  //int iOn = atoi(&buf[10]);
  //int iOff = atoi(&buf[15]);
  //float pmode = atof(&buf[20]);

  if (btn < 0 || btn >= aqdata->total_buttons ) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aqdata->aqbuttons[btn];
  unsigned char code = button->code;
#endif

  if (!isPLIGHT(button->special_mask)) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light for button '%d', configuration is incorrect\n", button->label);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  int final_mode=val;
  bool useProgAdvance = _aqconfig_.light_programming_advance_mode;
  int iOn =  _aqconfig_.light_programming_initial_on;
  int iOff = _aqconfig_.light_programming_initial_off;
  float pmode =  _aqconfig_.light_programming_mode;

    // Simply turn the light off if value is 0
  if (val <= 0) {
    if ( button->led->state == ON ) {
      LOG(ALLB_LOG, LOG_INFO, "Light mode request set to 0, turning off\n");
      send_cmd(button->code);
    } else {
      LOG(ALLB_LOG, LOG_INFO, "Light mode request set to 0 and light is off. Not sure what to do!\n");
    }
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  LOG(ALLB_LOG, LOG_INFO, "Light Programming #: %d, on button: %s, with pause mode: %f (initial on=%d, initial off=%d)\n", val, button->label, pmode, iOn, iOff);

  if ( button->led->state == ON && useProgAdvance ) {
    // Light is on, we know the mode, we can advance to next mode without re-setting program to 1
    int cmode = ((clight_detail *)button->special_mask_ptr)->currentValue;
    if (cmode > 0) {
      int numPrograms = get_num_light_modes(0) - 1; // Need to ignore program 0=off, so reduce by 1
      //int adv_steps = (val - cmode + numPrograms) % numPrograms;
      int adv_steps = ((val - cmode) % numPrograms + numPrograms) % numPrograms;
      LOG(ALLB_LOG, LOG_INFO, "Advancing Light program by %d (current=%d, new=%d, total=%d)\n",adv_steps, cmode, val, numPrograms);
      if (adv_steps > 0) {
        send_cmd(button->code);
        waitfor_queue2empty();
        delay(pmode * seconds); // 0.3 works, but using 0.4 to be safe
      }
      final_mode = val;
      val = adv_steps;
      iOn = -1;
      iOff = -1;
      //goto programming;
    } else {
      LOG(ALLB_LOG, LOG_INFO, "Can't advance Light program, current state unknown\n");
    }
  } else {
    //LOG(ALLB_LOG, LOG_INFO, "Not using advance mode (state=%d, usemode=%d)\n",button->led->state,useProgAdvance);
  }

  
  // Needs to start programming sequence with light on, if off we need to turn on for 15 seconds
  // before we can send the next off.
  if ( button->led->state != ON && iOn > 0) {
    LOG(ALLB_LOG, LOG_INFO, "Light Programming Initial state off, turning on for %d seconds\n",iOn);
    send_cmd(button->code);
    delay(iOn * seconds);
  }

  // Now need to turn off for between 11 and 14 seconds to reset light.
  if (iOff > 0) {
    LOG(ALLB_LOG, LOG_INFO, "Light Programming turn off for %d seconds\n",iOff);
    send_cmd(button->code);
    delay(iOff * seconds);
  }

  // Now light is reset, pulse the appropiate number of times to advance program.
  LOG(ALLB_LOG, LOG_INFO, "Light Programming button pulsing on/off %d times\n", val);
 
  // Program light in safe mode (slowley), or quick mode
  if (pmode > 0 && val > 0) {
    for (i = 1; i < (val * 2); i++) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, i % 2 == 0 ? "Off" : "On", val);
      send_cmd(button->code);
      waitfor_queue2empty();
      delay(pmode * seconds); // 0.3 works, but using 0.4 to be safe
    }
  } else if (val > 0) {
    for (i = 1; i < val; i++) {
      const int dt = 0.5;  // Time to wait after receiving conformation of light on/off
      waitfor_queue2empty();
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "ON", val);
      send_cmd(button->code);
      waitForButtonState(aqdata, button, ON, 2);
      delay(dt * seconds);
      waitfor_queue2empty();
      LOG(ALLB_LOG, LOG_INFO, "Light Programming button press number %d - %s of %d\n", i, "OFF", val);
      send_cmd(button->code);
      waitForButtonState(aqdata, button, OFF, 2);
      delay(dt * seconds);
    }
    LOG(ALLB_LOG, LOG_INFO, "Finished - Light Programming button press number %d - %s of %d\n", i, "ON", val);
    send_cmd(button->code);
    //waitfor_queue2empty();
    longwaitfor_queue2empty();
  }
  //waitForButtonState(aqdata, &aqdata->aqbuttons[btn], ON, 2);

  // set before we are called
  //updateButtonLightProgram(aqdata, val, btn);

  // Save value if needed
  updateLightProgram(aqdata, final_mode, (clight_detail *)button->special_mask_ptr );

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_allbutton_light_dimmer( void *ptr )
{
  int i;
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_LIGHTDIMMER);

  /*
  char *buf = (char*)threadCtrl->thread_args;
  const char *mode_name;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int typ = atoi(&buf[10]);
  bool use_current_mode = false;

  if (btn < 0 || btn >= aqdata->total_buttons ) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light dimmer on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aqdata->aqbuttons[btn];
  unsigned char code = button->code;
  */

  struct programmerArgs *pargs = &threadCtrl->pArgs;
  //unsigned char code = threadCtrl->pArgs.button.code;
  //aqkey *button2 = pargs->button;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int val = pargs->value;
  bool useDefaultIfValid = pargs->alt_value;
  bool use_current_mode = false;
  const char *mode_name;

  //printf("****** set_allbutton_light_dimmer Light %s on/off=%s use-current-state=%s\n",button->label, button->led->state==ON?"on":"off", use_current_mode==false?"no":"yes");

  if (!isPLIGHT(button->special_mask)) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light for button '%d', configuration is incorrect\n", button->label);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  clight_detail *light = (clight_detail *)button->special_mask_ptr;

  if (pargs->value <= 0) {
    use_current_mode = true;
    LOG(ALLB_LOG, LOG_INFO, "Light Dimmer Programming #: %d, on button: %s, using current mode\n", val, button->label);
  } else {
    //mode_name = light_mode_name(typ, val-1, ALLBUTTON);
    //mode_name = light_mode_name(((clight_detail *)button->special_mask_ptr)->lightType, val, ALLBUTTON);
    mode_name = light_mode_name(LC_DIMMER, val, ALLBUTTON);
    use_current_mode = false;
    if (mode_name == NULL) {
      LOG(ALLB_LOG, LOG_ERR, "Light Dimmer Programming #: %d, on button: %s, couldn't find mode name '%s'\n", val, button->label, mode_name);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    } else {
      LOG(ALLB_LOG, LOG_INFO, "Light Dimmer Programming #: %d, on button: %s, name '%s'\n", val, button->label, mode_name);
    }
  }

  //printf("****** set_allbutton_light_dimmer Light %s on/off=%d %s\n",button->label, pargs->button->led->state, button->led->state==ON?"on":"off");
  // Needs to start programming sequence with light off

  if ( button->led->state == ON ) {
    LOG(ALLB_LOG, LOG_INFO, "Light Programming Initial state on, turning off\n");
    send_cmd(button->code);
    waitfor_queue2empty();
    if ( !waitForMessage(threadCtrl->aqdata, "OFF", 5)) // Message like 'Aux3 Off'
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive OFF message\n");
  }

  // Look for <>*
  // Now turn on and wait for the message "color mode name<>*"
  send_cmd(button->code);
  waitfor_queue2empty();
  i=0;
  int waitCounter=12;

  do{
    LOG(ALLB_LOG, LOG_INFO,"Light program wait for message\n");
    if ( !waitForMessage(threadCtrl->aqdata, "~*", waitCounter))
      LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive light mode message\n");
    
    // Wait for less messages after first try. We get a lot of repeat messages before the one we need.
    waitCounter = 3;
    
    if (useDefaultIfValid) {
      // If 0 in last message then it's NOT valid and we will change to 100% (5 char =0, 6 char=%)
      if (aqdata->last_message[0] == '0' && aqdata->last_message[1] == '%' ) {
        //printf("******** Light stuck ************\n");
        LOG(ALLB_LOG, LOG_WARNING, "Light Programming detected Jandy panel light%% bug, re-setting to %d%%\n",val * 25);
        useDefaultIfValid=false;
      } else {
        // printf("******** Light good ************ '%c' '%c'\n",aqdata->last_message[0],aqdata->last_message[1]);
        send_cmd(KEY_ENTER);
        waitfor_queue2empty();
        break;
      }
    } else if (use_current_mode) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming using mode %s\n",aqdata->last_message);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    } else if (strncasecmp(aqdata->last_message, mode_name, strlen(mode_name)) == 0) {
      LOG(ALLB_LOG, LOG_INFO, "Light Programming found mode %s\n",mode_name);
      send_cmd(KEY_ENTER);
      waitfor_queue2empty();
      break;
    }

    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    // Just clear current message before waiting for next, since the test in the do can't distinguish
    // as both messages end in "~*"
    waitForMessage(threadCtrl->aqdata, NULL, 1); 

    i++;
  } while (i <= 8);

  if (i == 8) {
    LOG(ALLB_LOG, LOG_ERR, "Light Programming didn't receive light mode message for '%s'\n",use_current_mode?"light program":mode_name);
  } else {
    // update status before we are exit.
    if (light->lightType == LC_DIMMER2 ) {
      // value or Dimmer2 is the actual %, while Dimmer & colorlight is an index into an array
      updateLightProgram(aqdata, val * 25, light);
    } else {
      updateLightProgram(aqdata, val, light);
    }
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}






void *set_allbutton_pool_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  char *name;
  char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_POOL_HEATER_TEMP);
  
#ifdef NEW_AQ_PROGRAMMER
  int val = threadCtrl->pArgs.value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif
  
  /*
  if (val > HEATER_MAX) {
    val = HEATER_MAX;
  } else if ( val < MEATER_MIN) {
    val = MEATER_MIN;
  }
  */
  val = setpoint_check(POOL_HTR_SETPOINT, val, aqdata);

  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2
  if (isSINGLE_DEV_PANEL){
    name = "TEMP1";
    menu_name = "SET TEMP1";
  } else {
    name = "POOL";
    menu_name = "SET POOL TEMP";
  }

  LOG(ALLB_LOG, LOG_DEBUG, "Setting pool heater setpoint to %d\n", val);
  //setAqualinkTemp(aqdata, "SET TEMP", "SET POOL TEMP", NULL, "POOL", val);
  
  if ( select_menu_item(aqdata, "SET TEMP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aqdata, "SET POOL TEMP") != true) {
  if (select_sub_menu_item(aqdata, menu_name) != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET POOL TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  if (isSINGLE_DEV_PANEL){
    // Need to get pass this message 'TEMP1 MUST BE SET HIGHER THAN TEMP2'
    // and get this message 'TEMP1 20�C ~*'
    // Before going to numeric field.
    waitForMessage(threadCtrl->aqdata, "MUST BE SET", 5);
    send_cmd(KEY_LEFT);
    while (stristr(aqdata->last_message, "MUST BE SET") != NULL) { 
      delay(500);
    }
  } 

  //setAqualinkNumericField(aqdata, "POOL", val);
  setAqualinkNumericField(aqdata, name, val);
  
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  waitForMessage(threadCtrl->aqdata, "POOL TEMP IS SET TO", 1); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
void *set_allbutton_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_SPA_HEATER_TEMP);
  
#ifdef NEW_AQ_PROGRAMMER
  int val = threadCtrl->pArgs.value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif
  

  char *name;
  char *menu_name;
  /*
  if (val > HEATER_MAX) {
    val = HEATER_MAX;
  } else if ( val < MEATER_MIN) {
    val = MEATER_MIN;
  }*/
  val = setpoint_check(SPA_HTR_SETPOINT, val, aqdata);

  // NSF IF in TEMP1 / TEMP2 mode, we need C range of 1 to 40 is 2 to 40 for TEMP1, 1 to 39 TEMP2

  if (isSINGLE_DEV_PANEL){
    name = "TEMP2";
    menu_name = "SET TEMP2";
  } else {
    name = "SPA";
    menu_name = "SET SPA TEMP";
  }

  LOG(ALLB_LOG, LOG_DEBUG, "Setting spa heater setpoint to %d\n", val);
   
  //setAqualinkTemp(aqdata, "SET TEMP", "SET SPA TEMP", NULL, "SPA", val);
  if ( select_menu_item(aqdata, "SET TEMP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  //if (select_sub_menu_item(aqdata, "SET SPA TEMP") != true) {
  if (select_sub_menu_item(aqdata, menu_name) != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET SPA TEMP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  if (isSINGLE_DEV_PANEL){
    // Need to get pass this message 'TEMP2 MUST BE SET LOWER THAN TEMP1'
    // and get this message 'TEMP2 20�C ~*'
    // Before going to numeric field.
    waitForMessage(threadCtrl->aqdata, "MUST BE SET", 5);
    send_cmd(KEY_LEFT);
    while (stristr(aqdata->last_message, "MUST BE SET") != NULL) { 
      delay(500);
    }
  } 
  
  //setAqualinkNumericField(aqdata, "SPA", val);
  setAqualinkNumericField(aqdata, name, val);
  
  // usually miss this message, not sure why, but wait anyway to make sure programming has ended
  waitForMessage(threadCtrl->aqdata, "SPA TEMP IS SET TO", 1);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_allbutton_freeze_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_FRZ_PROTECTION_TEMP);
  
#ifdef NEW_AQ_PROGRAMMER
  int val = threadCtrl->pArgs.value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  /*
  if (val > FREEZE_PT_MAX) {
    val = FREEZE_PT_MAX;
  } else if ( val < FREEZE_PT_MIN) {
    val = FREEZE_PT_MIN;
  }
  */
  val = setpoint_check(FREEZE_SETPOINT, val, aqdata);

  LOG(ALLB_LOG, LOG_DEBUG, "Setting sfreeze protection to %d\n", val);

  //setAqualinkTemp(aqdata, "SYSTEM SETUP", "FRZ PROTECT", "TEMP SETTING", "FRZ", val);
  if ( select_menu_item(aqdata, "SYSTEM SETUP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SYSTEM SETUP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "FRZ PROTECT") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  if (select_sub_menu_item(aqdata, "TEMP SETTING") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select TEMP SETTING menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr; 
  }
  
  setAqualinkNumericField(aqdata, "FRZ", val);
  
  waitForMessage(threadCtrl->aqdata, "FREEZE PROTECTION IS SET TO", 3);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

/* sync_time_accurately: the panel starts its clock at HH:MM:00 on the final ENTER, so hold that
   ENTER for the minute change.  Staging time depends on the bus, so if it overruns step MINUTE on. */
#define AQ_SETTIME_LEAD         15  // aim at a boundary at least this far ahead
#define AQ_SETTIME_KEEPALIVE    20  // nudge the menu this often so SET TIME does not time out
#define AQ_SETTIME_NUDGE_GUARD  15  // but not this close to the boundary
#define AQ_SETTIME_COMMIT_SLOP   5  // commit up to this late, otherwise step on a minute
#define AQ_SETTIME_MAX_STEPS     5
#define AQ_SETTIME_MAX_HOLD     (AQ_SETTIME_LEAD + 60)  // a target can never legitimately be further away

static bool settime_sleep_for(struct timespec remaining)
{
  while (nanosleep(&remaining, &remaining) != 0) {
    if (errno != EINTR || isAqualinkDStopping())
      return false;
  }
  return !isAqualinkDStopping();
}

// Wait for target (wall clock), nudging MINUTE one step and back so the menu stays open.  The nudge
// timer is CLOCK_MONOTONIC so a clock correction cannot stop it; fields do not wrap, so 0 steps up.
static bool settime_hold_until(struct aqualinkdata *aqdata, time_t target, int min)
{
  const struct timespec target_time = { .tv_sec = target, .tv_nsec = 0 };
  struct timespec next_nudge;

  if (clock_gettime(CLOCK_MONOTONIC, &next_nudge) != 0)
    return false;
  next_nudge.tv_sec += AQ_SETTIME_KEEPALIVE;

  for (;;) {
    struct timespec realtime;
    struct timespec monotonic;
    struct timespec until_target;
    struct timespec until_nudge;
    struct timespec sleep_for;
    bool nudge_due;
    bool can_nudge;

    if (isAqualinkDStopping())
      return false;
    if (clock_gettime(CLOCK_REALTIME, &realtime) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0)
      return false;
    if (timespec_subtract(&until_target, &target_time, &realtime) ||
        (until_target.tv_sec == 0 && until_target.tv_nsec == 0))
      return true;

    // A backwards clock step moves the target away; do not hold SET TIME open indefinitely.
    if (until_target.tv_sec > AQ_SETTIME_MAX_HOLD) {
      LOG(ALLB_LOG, LOG_WARNING, "System clock changed while setting the panel time, giving up\n");
      return false;
    }

    can_nudge = until_target.tv_sec > AQ_SETTIME_NUDGE_GUARD ||
                (until_target.tv_sec == AQ_SETTIME_NUDGE_GUARD &&
                 until_target.tv_nsec > 0);
    nudge_due =
        timespec_subtract(&until_nudge, &next_nudge, &monotonic);

    if (can_nudge && nudge_due) {
      int other = min > 0 ? min - 1 : min + 1;
      if (!setAqualinkNumericField_noenter(aqdata, "MINUTE", other) ||
          !setAqualinkNumericField_noenter(aqdata, "MINUTE", min))
        return false;
      if (clock_gettime(CLOCK_MONOTONIC, &next_nudge) != 0)
        return false;
      next_nudge.tv_sec += AQ_SETTIME_KEEPALIVE;
      continue;
    }

    sleep_for = until_target;
    if (can_nudge && !nudge_due &&
        (until_nudge.tv_sec < sleep_for.tv_sec ||
         (until_nudge.tv_sec == sleep_for.tv_sec &&
          until_nudge.tv_nsec < sleep_for.tv_nsec)))
      sleep_for = until_nudge;
    if (!settime_sleep_for(sleep_for))
      return false;
  }
}

// Stage MINUTE, hold for the boundary, then commit.  False means cancel the menu.
static bool settime_commit_on_boundary(struct aqualinkdata *aqdata, time_t target, int min)
{
  int steps = 0;

  if (! setAqualinkNumericField_noenter(aqdata, "MINUTE", min))
    return false;

  for (;;) {
    if (time(0) < target) {
      if (! settime_hold_until(aqdata, target, min))
        return false;
      if (time(0) - target <= AQ_SETTIME_COMMIT_SLOP)
        break;
    }
    // Boundary gone.  Past 59 would need HOUR, which is already accepted.
    if (min == 59 || ++steps > AQ_SETTIME_MAX_STEPS) {
      LOG(ALLB_LOG, LOG_WARNING, "Missed the minute to set the panel time on, will try again later\n");
      return false;
    }
    min++;
    target += 60;
    LOG(ALLB_LOG, LOG_NOTICE, "Setting panel time took longer than a minute, stepping on to minute %d\n", min);
    if (! setAqualinkNumericField_noenter(aqdata, "MINUTE", min))
      return false;
  }

  send_cmd(KEY_ENTER);   // accept MINUTE, the panel clock starts at HH:MM:00 here
  send_cmd(KEY_ENTER);   // leave SET TIME
  return true;
}

void *set_allbutton_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_TIME);
  //LOG(ALLB_LOG, LOG_NOTICE, "Setting time on aqualink\n");

  time_t now;
  time_t target = 0;
  struct tm tm_result;
  struct tm *result;
  char hour[20];

  if ( select_menu_item(aqdata, "SET TIME") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TIME menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // Read the clock once SET TIME is up, so the menu walk is not part of the wait.  localtime_r as
  // localtime()'s buffer is shared with other threads.
  now = time(0);
  if (_aqconfig_.sync_time_accurately) {
    target = ((now + AQ_SETTIME_LEAD + 59) / 60) * 60;   // first minute boundary >= now + lead
    result = localtime_r(&target, &tm_result);
    LOG(ALLB_LOG, LOG_NOTICE, "Setting panel time accurately, committing at %02d:%02d\n", result->tm_hour, result->tm_min);
  } else {
    // Add 10 seconds to time since this can take a while to program.
    // 10 to 20 seconds whould be right, but since there are no seconds we can set, add 30 seconds to get close to minute.
    // sync_time_accurately above does the "program the next minute then wait" this used to suggest.
    result = localtime_r(&now, &tm_result);
    result->tm_sec += 30;
    mktime(result);
  }
  
  if (result->tm_hour == 0)
    sprintf(hour, "HOUR 12 AM");
  else if (result->tm_hour <= 11)
    sprintf(hour, "HOUR %d AM", result->tm_hour); // Need to fix compiler warning on new GCC, but %2d or %.2d does NOT give correct string
  else if (result->tm_hour == 12)
    sprintf(hour, "HOUR 12 PM");
  else // Must be 13 or more
    sprintf(hour, "HOUR %d PM", result->tm_hour - 12);
  

  LOG(ALLB_LOG, LOG_DEBUG, "Setting time to %d/%d/%d %d:%d\n", result->tm_mon + 1, result->tm_mday, result->tm_year + 1900, result->tm_hour + 1, result->tm_min);

  setAqualinkNumericField(aqdata, "YEAR", result->tm_year + 1900);
  setAqualinkNumericField(aqdata, "MONTH", result->tm_mon + 1);
  setAqualinkNumericField(aqdata, "DAY", result->tm_mday);
  //setAqualinkNumericFieldExtra(aqdata, "HOUR", 11, "PM");
  select_sub_menu_item(aqdata, hour); // This will keep looping until it finds the right message

  if (! _aqconfig_.sync_time_accurately) {
    setAqualinkNumericField(aqdata, "MINUTE", result->tm_min);
    send_cmd(KEY_ENTER);
  } else if (! settime_commit_on_boundary(aqdata, target, result->tm_min)) {
    cancel_menu();
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_allbutton_diag_model( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_DIAGNOSTICS_MODEL);
  
  if ( select_menu_item(aqdata, "SYSTEM SETUP") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select HELP menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "DIAGNOSTICS") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select DIAGNOSTICS menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  waitForMessage(aqdata, NULL, 8); // Receive 8 messages
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
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_POOL_SPA_HEATER_TEMPS);
  //LOG(ALLB_LOG, LOG_NOTICE, "Getting pool & spa heat setpoints from aqualink\n");

  if ( select_menu_item(aqdata, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get heater setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "TEMP SET") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select TEMP SET menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get heater setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  // Should receive 'POOL TEMP IS SET TO xx' then 'SPA TEMP IS SET TO xx' then 'MAINTAIN TEMP IS (OFF|ON)'
  // wait for the last message
  waitForMessage(threadCtrl->aqdata, "MAINTAIN TEMP IS", 5);
    
  //cancel_menu(threadCtrl->aqdata);
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_allbutton_freeze_protect_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_FREEZE_PROTECT_TEMP);
  //LOG(ALLB_LOG, LOG_NOTICE, "Getting freeze protection setpoints\n");

  if ( select_menu_item(aqdata, "REVIEW") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get freeze setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "FRZ PROTECT") != true) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select FRZ PROTECT menu\n");
    LOG(ALLB_LOG, LOG_ERR, "Can't get freeze setpoints from Control Panel\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  waitForMessage(aqdata, "FREEZE PROTECTION IS SET TO", 6); // Changed from 3 to wait for multiple items to be listed
  //cancel_menu(aqdata); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

bool get_aqualink_program_for_button(struct aqualinkdata *aqdata, unsigned char cmd)
{
  int rtnStringsWait = 7;
  
  if (! waitForMessage(aqdata, "SELECT DEVICE TO REVIEW or PRESS ENTER TO END", rtnStringsWait))
    return false;
  
  LOG(ALLB_LOG, LOG_DEBUG, "Send key '%d'\n",cmd);
  send_cmd(cmd);
  return waitForEitherMessage(aqdata, "NOT SET", "TURNS ON", rtnStringsWait);
}

void *get_allbutton_programs( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  char keys[10] = {KEY_PUMP, KEY_SPA, KEY_AUX1, KEY_AUX2, KEY_AUX3, KEY_AUX4, KEY_AUX5}; // KEY_AUX6 & KEY_AUX7 kill programming mode
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_PROGRAMS);

  if ( select_menu_item(aqdata, "REVIEW") != true ) {
    //LOG(ALLB_LOG, LOG_WARNING, "Could not select REVIEW menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }     
           
  if (select_sub_menu_item(aqdata, "PROGRAMS") != true) {
    //LOG(ALLB_LOG, LOG_WARNING, "Could not select PROGRAMS menu\n");
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  
  unsigned int i;
  
  for (i=0; i < strlen(keys); i++) {
    //LOG(ALLB_LOG, LOG_DEBUG, "**** START program for key in loop %d\n",i);
    if (! get_aqualink_program_for_button(aqdata, keys[i])) {
      //LOG(ALLB_LOG, LOG_DEBUG, "**** Didn't find program for key in loop %d\n",i);
      //cancel_menu(aqdata);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    //LOG(ALLB_LOG, LOG_DEBUG, "**** Found program for key in loop %d\n",i);    
  }
  
  
  if (waitForMessage(aqdata, "SELECT DEVICE TO REVIEW or PRESS ENTER TO END", 6)) {
    //LOG(ALLB_LOG, LOG_DEBUG, "Send key ENTER\n");
    send_cmd(KEY_ENTER);
  } else {
    //LOG(ALLB_LOG, LOG_DEBUG, "Send CANCEL\n");
    cancel_menu();
  }
  
  //cancel_menu(aqdata); 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

/*
void send_cmd(unsigned char cmd, struct aqualinkdata *aqdata)
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

  LOG(ALLB_LOG, LOG_INFO, "Queue send %s'0x%02hhx' to controller (programming)\n", cmd_to_string(_allb_pgm_command), _allb_pgm_command);
}

void force_queue_delete()
{
  //if (_allb_pgm_command != NUL)
  //  LOG(ALLB_LOG, LOG_INFO, "Really bad coding, don't use this in release\n");

  _allb_pgm_command = NUL;
}


void cancel_menu()
{
  send_cmd(KEY_CANCEL);
}

/*
* added functionality, if start of string is ^ use that as must start with in comparison
*/

bool waitForEitherMessage(struct aqualinkdata *aqdata, const char* message1, const char* message2, int numMessageReceived)
{
  //LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
  waitfor_queue2empty();  // MAke sure the last command was sent
  int i=0;
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);
  const char* msgS1 = "";
  const char* msgS2 = "";
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
    LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' OR '%s' received message1 '%s'\n",i,numMessageReceived,message1,message2,aqdata->last_message);
    
    if (message1 != NULL) {
      ptr = stristr(aqdata->last_message, msgS1);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH '%s'\n", msgS1);
        if (msgS1 == message1) // match & don't care if first char
          break;
        else if (ptr == aqdata->last_message) // match & do care if first char
          break;
      }
    }
    if (message2 != NULL) {
      ptr = stristr(aqdata->last_message, msgS2);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH '%s'\n", msgS2);
        if (msgS2 == message2) // match & don't care if first char
          break;
        else if (ptr == aqdata->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message1 '%s'\n",message1,aqdata->last_message);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message1 '%s'\n",i,numMessageReceived,message1,aqdata->last_message);
  }
  
  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);
  
  if (message1 != NULL && message2 != NULL && ptr == NULL) {
    //logmessage1(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_ERR, "Did not find '%s'\n",message1);
    return false;
  }
  LOG(ALLB_LOG, LOG_DEBUG, "Found message1 '%s' or '%s' in '%s'\n",message1,message2,aqdata->last_message);
  
  return true;
}



bool waitForMessage(struct aqualinkdata *aqdata, const char* message, int numMessageReceived)
{
  LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d\n",message,numMessageReceived);
  // NSF Need to come back to this, as it stops on test enviornment but not real panel, so must be speed related.
  //waitfor_queue2empty();  // MAke sure the last command was sent

  int i=0;
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);
  const char* msgS;
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
      LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s', last message received '%s'\n",i,numMessageReceived,message,aqdata->last_message);
    else
      LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d waiting for next message, last message received '%s'\n",i,numMessageReceived,aqdata->last_message);

    if (message != NULL) {
      ptr = stristr(aqdata->last_message, msgS);
      if (ptr != NULL) { // match
        LOG(ALLB_LOG, LOG_DEBUG, "String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aqdata->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message '%s'\n",message,aqdata->last_message);
    //LOG(ALLB_LOG, LOG_DEBUG, "*** pthread_cond_wait() sleep\n");
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "*** pthread_cond_wait() wake\n");
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aqdata->last_message);
  }
  
  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //LOG(ALLB_LOG, LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_DEBUG, "did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    LOG(ALLB_LOG, LOG_DEBUG, "found message '%s' in '%s'\n",message,aqdata->last_message);
  else
    LOG(ALLB_LOG, LOG_DEBUG, "waited for %d message(s)\n",numMessageReceived);
  
  return true;
}


//bool select_sub_menu_item(char* item_string, struct aqualinkdata *aqdata)
bool _select_sub_menu_item(struct aqualinkdata *aqdata, const char* item_string, bool supress_err)
{
  int wait_messages = 28;
  int i=0;
 
  waitfor_queue2empty();

  while( (stristr(aqdata->last_message, item_string) == NULL) && ( i++ < wait_messages) )
  {
    LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d looking for '%s' received message '%s'\n",i,wait_messages,item_string,aqdata->last_message);
    send_cmd(KEY_RIGHT);
    waitfor_queue2empty(); // ADDED BACK MAY 2023 setting time warked better
    //waitForMessage(aqdata, NULL, 1);
    waitForMessage(aqdata, item_string, 1);
  }

  if (stristr(aqdata->last_message, item_string) == NULL) {
    if (!supress_err) {
      LOG(ALLB_LOG, LOG_ERR, "Could not find menu item '%s'\n",item_string);
    }
    return false;
  }
  
  LOG(ALLB_LOG, LOG_DEBUG, "Find item in Menu: loop %d of %d FOUND menu item '%s', sending ENTER command\n",i,wait_messages, item_string);
  // Enter the mode specified by the argument.
  
  send_cmd(KEY_ENTER);
  waitForMessage(aqdata, NULL, 1);
  
  return true;
 
}

bool _select_menu_item(struct aqualinkdata *aqdata, const char* item_string, bool supress_err)
{
  char* expectedMsg = "PRESS ENTER* TO SELECT";
  //char* expectedMsg = "PROGRAM";
  int wait_messages = 5;
  bool found = false;
  int tries = 0;
  // Select the MENU and wait to get the RS8 respond.
  
  while (found == false && tries <= 3) {
    send_cmd(KEY_MENU);
    found = waitForMessage(aqdata, expectedMsg, wait_messages);
    tries++;
  }

  if (found == false)
    return false;

  // NSF  This isn't needed and seems to cause issue on some controllers.
  //send_cmd(KEY_ENTER, aqdata);
  //waitForMessage(aqdata, NULL, 1);
  
  return _select_sub_menu_item(aqdata, item_string, supress_err);
}


bool select_sub_menu_item(struct aqualinkdata *aqdata, const char* item_string){
  return _select_sub_menu_item(aqdata, item_string, false);
}
bool select_menu_item(struct aqualinkdata *aqdata, const char* item_string){
  return _select_menu_item(aqdata, item_string, false);
}
bool select_menu_item_noerror(struct aqualinkdata *aqdata, const char* item_string){
  return _select_menu_item(aqdata, item_string, true);
}

// NSF Need to test this, then use it for the color change mode. 

bool waitForButtonState(struct aqualinkdata *aqdata, aqkey* button, aqledstate state, int numMessageReceived)
{
  //LOG(ALLB_LOG, LOG_DEBUG, "waitForMessage %s %d %d\n",message,numMessageReceived,cmd);
  int i=0;
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for state change to '%d' for '%s' \n",i,numMessageReceived,button->led->state,button->name);

    if (button->led->state == state) {
      LOG(ALLB_LOG, LOG_INFO, "Button State =%d for %s\n",state,button->name);
        break;
    }

    //LOG(ALLB_LOG, LOG_DEBUG, "looking for '%s' received message '%s'\n",message,aqdata->last_message);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    //LOG(ALLB_LOG, LOG_DEBUG, "loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aqdata->last_message);
  }

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  if (numMessageReceived >= i) {
    //LOG(ALLB_LOG, LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ALLB_LOG, LOG_DEBUG, "did not find state '%d' for '%s'\n",button->led->state,button->name);
    return false;
  }
  LOG(ALLB_LOG, LOG_DEBUG, "found state '%d' for '%s'\n",button->led->state,button->name);

  return true;
}
