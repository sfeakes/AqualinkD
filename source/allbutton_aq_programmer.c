#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "aqualink.h"
#include "allbutton_aq_programmer.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "color_lights.h"
#include "devices_jandy.h"



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
bool setAqualinkNumericField_noenter(struct aqualinkdata *aqdata, char *value_label, int value, time_t deadline);

/*
* set_allbutton_time() commits the panel clock on a minute boundary, because the panel has
* no seconds field and starts counting from HH:MM:00 the instant the entry is committed.
*
* The menu walk has to FINISH just before that boundary.  We do not know up front how long
* it takes (the MINUTE field is one keypress per minute of travel, up to 59 of them), so
* the duration of the last walk is remembered and used to decide when to start the next
* one.  The wait happens BEFORE the menu is opened, so the panel is not held in a
* programming menu, and neither is AqualinkD's programming thread.
*/
/*
* Timings for setting the panel clock.  Note that NONE of these try to predict how long
* the keypresses will take: a keypress round trip is really the RS485 poll cycle, which
* depends on how many devices share the bus and what else is happening on it, so it
* varies widely between installations and cannot be derived from one panel.  These are
* guards and intervals, not a schedule.
*/
#define AQ_SETTIME_COMMIT_GUARD      5  /* never aim at a boundary closer than this */
#define AQ_SETTIME_MAX_STAGING     180  /* runaway guard over the whole staging phase */
#define AQ_SETTIME_MAX_BUMPS         5  /* minutes we will step on before giving up */
#define AQ_SETTIME_BUMP_TIMEOUT     30  /* one single-keypress correction */
#define AQ_SETTIME_KEEPALIVE        20  /* nudge the menu this often while holding */
#define AQ_SETTIME_KEEPALIVE_GUARD  15  /* ...but never this close to the boundary */
#define AQ_SETTIME_COMMIT_SLOP       5  /* commit this late at worst, else step on */






/*
* Hold until 'target', keeping the panel's programming menu alive.
*
* The panel drops out of SET TIME on its own after a spell of inactivity, and this wait is
* up to a minute.  A keypress resets that timer, so nudge the MINUTE field one step and
* straight back - two presses that leave the staged value exactly where it was.  Each press
* waits for the value it should produce, so a slow bus cannot desynchronise it, and the
* step avoids 59->0 (and 0->-1) because the numeric fields do not wrap.
*
* Returns false on shutdown, or if a nudge fails.
*/
static bool settime_hold_until(struct aqualinkdata *aqdata, time_t target, int staged_min)
{
  time_t next_nudge = time(0) + AQ_SETTIME_KEEPALIVE;
  int other = (staged_min > 0) ? staged_min - 1 : staged_min + 1;

  while (time(0) < target) {
    if (isAqualinkDStopping())
      return false;

    /* Only nudge when there is room to finish it and still settle before the boundary. */
    if (time(0) >= next_nudge && target - time(0) > AQ_SETTIME_KEEPALIVE_GUARD) {
      LOG(ALLB_LOG, LOG_DEBUG, "Nudging MINUTE %d->%d->%d to keep SET TIME alive\n",
          staged_min, other, staged_min);
      if (! setAqualinkNumericField_noenter(aqdata, "MINUTE", other, time(0) + AQ_SETTIME_BUMP_TIMEOUT))
        return false;
      if (! setAqualinkNumericField_noenter(aqdata, "MINUTE", staged_min, time(0) + AQ_SETTIME_BUMP_TIMEOUT))
        return false;
      next_nudge = time(0) + AQ_SETTIME_KEEPALIVE;
    }
    delay(100);
  }
  return true;
}

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



bool setAqualinkNumericField_ex(struct aqualinkdata *aqdata, char *value_label, int value, int increment, bool sendEnter, time_t deadline);
bool setAqualinkNumericField(struct aqualinkdata *aqdata, char *value_label, int value)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, 1, true, 0);
}
bool setAqualinkNumericField_new(struct aqualinkdata *aqdata, char *value_label, int value, int increment)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, increment, true, 0);
}
/*
* As above but give up if 'deadline' (absolute, 0 for none) passes.  The iteration cap
* below bounds the number of KEYPRESSES, not the time they take, so a panel that answers
* slowly but keeps answering could otherwise hold a programming menu open for minutes.
*/
bool setAqualinkNumericField_until(struct aqualinkdata *aqdata, char *value_label, int value, time_t deadline)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, 1, true, deadline);
}
/*
* Park the field on `value` but leave the ENTER to the caller.  Only useful for the
* last field of a menu, where that ENTER is what commits the entry and the caller
* needs to control when that happens (see set_allbutton_time).
*/
bool setAqualinkNumericField_noenter(struct aqualinkdata *aqdata, char *value_label, int value, time_t deadline)
{
  return setAqualinkNumericField_ex(aqdata, value_label, value, 1, false, deadline);
}
bool setAqualinkNumericField_ex(struct aqualinkdata *aqdata, char *value_label, int value, int increment, bool sendEnter, time_t deadline)
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
    /* Checked before the wait, so an already-expired deadline returns without touching
       the panel at all. */
    if (deadline != 0 && time(0) > deadline) {
      LOG(ALLB_LOG, LOG_WARNING, "Ran out of time setting '%s' to '%d'\n", value_label, value);
      return false;
    }
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
      // We are at the right value.  Send ENTER to accept it, unless the caller asked
      // to keep hold of that keypress.
      sprintf(searchBuf, "%s %d", value_label, current_val);
      if (sendEnter)
        send_cmd(KEY_ENTER);
    }

    if (i++ >= 100) {
      LOG(ALLB_LOG, LOG_WARNING, "AQ_Programmer Could not set numeric input '%s', to '%d'\n",value_label,value);
      if (sendEnter)
        send_cmd(KEY_ENTER);
      // Was 'break', which then fell through to 'return true' and reported a field we
      // never managed to set as successful.
      return false;
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
      if (select_menu_item(aqdata, "BOOST POOL")) {
        s_swg_boost_menu = SWG_M_BOOST;
        return true;
      }
      if (can_be_superchlor && select_menu_item(aqdata, "SUPERCHLORINATE")) {
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
static bool select_chlorinator_menu(struct aqualinkdata *aqdata)
{
  static swg_menu_t s_swg_menu = SWG_UNKNOWN;   // detected on first successful run
  //const bool can_be_aquarite = (aqdata->panel_rev[0] < 'L');

  switch (s_swg_menu) {
    case SWG_AQUAPURE:
      return select_menu_item(aqdata, "SET AQUAPURE");

    case SWG_AQUARITE:
      return select_menu_item(aqdata, "SET AQUARITE");

    default:  // SWG_UNKNOWN: probe once
      if (select_menu_item_noerror(aqdata, "SET AQUAPURE")) {
        s_swg_menu = SWG_AQUAPURE;
        return true;
      }
      if (select_menu_item_noerror(aqdata, "SET AQUARITE")) {
        s_swg_menu = SWG_AQUARITE;
        return true;
      }
      s_swg_menu = SWG_UNKNOWN;   // optional: re-detect next run
      return false;
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
  if (!select_chlorinator_menu(aqdata)) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET AQUAPURE/AQUARITE menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName(aqdata->active_thread.ptype));
    //_s_swg_menu = SWG_UNKNOWN;   // optional: re-detect next run
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }


  // If spa is on, set SWG for spa, if not set SWG for pool
  if (aqdata->aqbuttons[SPA_INDEX].led->state != OFF) {
    if (select_sub_menu_item(aqdata, "SET SPA SP") != true) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu for SPA\n");
      LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    setAqualinkNumericField_new(aqdata, "SPA SP", val, 5);
  } else {
    if (select_sub_menu_item(aqdata, "SET POOL SP") != true) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not select SWG setpoint menu\n");
      LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
      cancel_menu();
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    setAqualinkNumericField_new(aqdata, "POOL SP", val, 5);
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

/*
* Build the SET TIME menu's HOUR line for a 0-23 hour, e.g. 16 -> "HOUR 4 PM".
*/
static void hour_menu_string(char *out, size_t size, int hour24)
{
  if (hour24 == 0)
    snprintf(out, size, "HOUR 12 AM");
  else if (hour24 <= 11)
    snprintf(out, size, "HOUR %d AM", hour24);
  else if (hour24 == 12)
    snprintf(out, size, "HOUR 12 PM");
  else
    snprintf(out, size, "HOUR %d PM", hour24 - 12);
}

/*
* Read a 0-23 hour back out of a panel HOUR line. false if it is not one.
*/
static bool parse_hour_menu(const char *msg, int *hour24)
{
  char *p = stristr(msg, "HOUR");
  int h12;
  char mer[4];

  if (p == NULL)
    return false;
  if (sscanf(p, "%*s %d %3s", &h12, mer) != 2)
    return false;
  if (h12 < 1 || h12 > 12)
    return false;

  if (mer[0] == 'A' || mer[0] == 'a')
    *hour24 = (h12 == 12) ? 0 : h12;
  else if (mer[0] == 'P' || mer[0] == 'p')
    *hour24 = (h12 == 12) ? 12 : h12 + 12;
  else
    return false;

  return true;
}

/*
* Set the SET TIME menu's HOUR field, leaving the panel on the next field.
*
* Neither of the generic helpers is safe here.  setAqualinkNumericField() cannot parse a
* field with a meridiem on it, and select_sub_menu_item() waits only for the FINAL target
* string, which loses a race the panel wins every time: coming out of the DAY field the
* last message received is still the DAY line, so its opening comparison fails, it presses
* RIGHT before it has ever seen the hour, and then matches the pre-keypress "HOUR n xM"
* the panel had already queued - committing an hour too far.  That is where the "panel is
* an hour ahead" came from.
*
* So do what setAqualinkNumericField() does and wait for the specific value the keypress
* we just sent should produce.  Every step re-reads the field, so a stale or buffered
* message cannot make us lose count.
*
* Only KEY_RIGHT is used.  It is the proven direction for this field and wraps 11 PM round
* to 12 AM, so any hour is reachable in at most 23 presses.  KEY_LEFT would halve the
* worst case but is unverified on this field, and guessing wrong here sets the clock wrong.
*/
static bool setAqualinkHourField(struct aqualinkdata *aqdata, int hour24, time_t deadline)
{
  char target[20];
  char expect[20];
  int cur24 = -1;
  int i = 0;

  hour_menu_string(target, sizeof(target), hour24);

  /* Before the first wait, so an already-expired deadline gives up without blocking.
     NOTE this cannot interrupt a wait already in progress: waitForMessage() uses
     pthread_cond_wait() with no timeout, so a panel that goes completely silent still
     parks the thread. Bounding that needs pthread_cond_timedwait() throughout. */
  if (deadline != 0 && time(0) > deadline) {
    LOG(ALLB_LOG, LOG_WARNING, "Ran out of time before setting the panel hour to '%s'\n", target);
    return false;
  }

  /* Get a real HOUR line before deciding anything, the last message is still the DAY
     field at this point. */
  if ( waitForMessage(aqdata, "HOUR", 4) != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Never saw the HOUR field on the panel\n");
    return false;
  }

  while (i++ <= 24) {
    if (deadline != 0 && time(0) > deadline) {
      LOG(ALLB_LOG, LOG_WARNING, "Ran out of time setting the panel hour to '%s'\n", target);
      return false;
    }
    if ( parse_hour_menu(aqdata->last_message, &cur24) != true ) {
      LOG(ALLB_LOG, LOG_WARNING, "Could not read panel hour from '%s'\n", aqdata->last_message);
      return false;
    }

    if (cur24 == hour24) {
      LOG(ALLB_LOG, LOG_DEBUG, "Panel hour is '%s', accepting\n", target);
      send_cmd(KEY_ENTER);
      waitForMessage(aqdata, NULL, 1); // let the panel move to the next field
      return true;
    }

    hour_menu_string(expect, sizeof(expect), (cur24 + 1) % 24);
    LOG(ALLB_LOG, LOG_DEBUG, "Stepping panel hour to '%s' (want '%s')\n", expect, target);
    send_cmd(KEY_RIGHT);
    waitfor_queue2empty();
    if ( waitForMessage(aqdata, expect, 4) != true ) {
      LOG(ALLB_LOG, LOG_WARNING, "Panel hour did not step to '%s'\n", expect);
      return false;
    }
  }

  LOG(ALLB_LOG, LOG_WARNING, "Gave up stepping panel hour to '%s'\n", target);
  return false;
}

void *set_allbutton_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  time_t now, target, give_up;
  struct tm tm_target;
  char hour[20];
  char buf[40];
  int bumps;

  /* The SET TIME menu has no seconds field: the panel starts its clock at HH:MM:00 the
     moment the entry is committed.  So the accuracy is decided entirely by WHEN the final
     ENTER lands, not by how cleverly the minute is rounded.

     Every field is a keypress that has to round trip through the panel display, and that
     round trip is really the RS485 poll cycle - it depends on how many devices are on the
     bus and what else is happening on it, so it varies by a lot from one installation to
     the next and cannot be predicted from a measurement taken on one panel.

     So do not predict it.  Stage all the fields, then simply hold the committing ENTER
     until the clock reaches the minute we staged.  If staging ran long and that minute
     has already gone, step the MINUTE field on by one (a single keypress) and aim at the
     next one.  That is self correcting at any bus speed.

     The cost is that the panel sits in SET TIME for up to a minute, which is why
     checkAqualinkTime() will normally only start this inside the configured quiet window.
  */
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_TIME);

  if ( select_menu_item(aqdata, "SET TIME") != true ) {
    LOG(ALLB_LOG, LOG_WARNING, "Could not select SET TIME menu\n");
    LOG(ALLB_LOG, LOG_ERR, "%s failed\n", ptypeName( aqdata->active_thread.ptype ) );
    cancel_menu();
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  /* SET TIME is up, so the clock sample and everything after it is unaffected by however
     long the menu walk took.  Aim at the next minute boundary; AQ_SETTIME_COMMIT_GUARD
     keeps us from targeting one we are already on top of. */
  now = time(0);
  target = ((now + AQ_SETTIME_COMMIT_GUARD) / 60) * 60 + 60;
  give_up = now + AQ_SETTIME_MAX_STAGING;

  localtime_r(&target, &tm_target);
  hour_menu_string(hour, sizeof(hour), tm_target.tm_hour);
  strftime(buf, sizeof(buf), "%m/%d/%y %I:%M %p", &tm_target);
  LOG(ALLB_LOG, LOG_NOTICE, "Setting panel time to %s ('%s')\n", buf, hour);

  /* Every field is checked.  Getting any of them wrong silently sets the panel clock
     wrong, and for MINUTE it is worse than that: on failure the helper has already
     cancelled the menu, so carrying on would hold the boundary and then fire two ENTERs
     into whatever the panel is showing by then. */
  if ( setAqualinkNumericField_until(aqdata, "YEAR", tm_target.tm_year + 1900, give_up) != true ) {
    LOG(ALLB_LOG, LOG_ERR, "Could not set panel year, abandoning panel time set\n");
    goto settime_failed;
  }
  if ( setAqualinkNumericField_until(aqdata, "MONTH", tm_target.tm_mon + 1, give_up) != true ) {
    LOG(ALLB_LOG, LOG_ERR, "Could not set panel month, abandoning panel time set\n");
    goto settime_failed;
  }
  if ( setAqualinkNumericField_until(aqdata, "DAY", tm_target.tm_mday, give_up) != true ) {
    LOG(ALLB_LOG, LOG_ERR, "Could not set panel day, abandoning panel time set\n");
    goto settime_failed;
  }
  /* See setAqualinkHourField() for why this does not use select_sub_menu_item(). */
  if ( setAqualinkHourField(aqdata, tm_target.tm_hour, give_up) != true ) {
    LOG(ALLB_LOG, LOG_ERR, "Could not set panel hour to '%s', abandoning panel time set\n", hour);
    goto settime_failed;
  }
  /* MINUTE is the last field, so its ENTER is the one that commits.  Park the field on
     the target minute but keep hold of that keypress. */
  if ( setAqualinkNumericField_noenter(aqdata, "MINUTE", tm_target.tm_min, give_up) != true ) {
    LOG(ALLB_LOG, LOG_ERR, "Could not set panel minute, abandoning panel time set\n");
    goto settime_failed;
  }

  /* Hold for the minute we staged, and if it has already gone, step the field on and aim
     at the next one - one keypress per minute, which costs nothing to get wrong.
     Two things can eat the boundary, and both are bus-speed dependent so neither is
     predicted: staging itself (easily over a minute on a busy bus), and a keep-alive
     nudge that outlasts its guard.  Either way the answer is the same - never commit a
     time we already know is late, just aim at the next minute. */
  for (bumps = 0; ; ) {
    now = time(0);

    if (now < target) {
      if (! settime_hold_until(aqdata, target, tm_target.tm_min)) {
        LOG(ALLB_LOG, LOG_WARNING, "Gave up waiting to commit panel time\n");
        goto settime_failed;
      }
      now = time(0);
      if (now - target <= AQ_SETTIME_COMMIT_SLOP)
        break;                    /* on the boundary (or near enough) - commit */
      LOG(ALLB_LOG, LOG_WARNING, "Keeping the menu alive outlasted %s by %ds, stepping on rather than committing late\n",
          buf, (int)(now - target));
    }

    if (tm_target.tm_min == 59) {
      /* Stepping past 59 needs the HOUR field, which was accepted several keypresses
         ago and cannot be revisited.  Rare, and the next check will simply try again. */
      LOG(ALLB_LOG, LOG_WARNING, "Ran past %s and the next minute crosses the hour. Abandoning, will re-check on next cycle\n", buf);
      goto settime_failed;
    }
    if (++bumps > AQ_SETTIME_MAX_BUMPS) {
      LOG(ALLB_LOG, LOG_ERR, "Still have not caught up after %d minutes. Abandoning\n", bumps - 1);
      goto settime_failed;
    }

    target += 60;
    localtime_r(&target, &tm_target);
    strftime(buf, sizeof(buf), "%m/%d/%y %I:%M %p", &tm_target);
    LOG(ALLB_LOG, LOG_NOTICE, "Stepping the panel on to %s\n", buf);
    if ( setAqualinkNumericField_noenter(aqdata, "MINUTE", tm_target.tm_min, time(0) + AQ_SETTIME_BUMP_TIMEOUT) != true ) {
      LOG(ALLB_LOG, LOG_ERR, "Could not step panel minute on, abandoning panel time set\n");
      goto settime_failed;
    }
  }

  send_cmd(KEY_ENTER); // Accept the MINUTE field, this is what starts the panel clock.
  send_cmd(KEY_ENTER); // Commit / leave the SET TIME menu.

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;

settime_failed:
  /* Leave the panel out of the menu rather than parked mid-entry.  checkAqualinkTime()
     will notice the clock is still wrong and try again on the next cycle. */
  cancel_menu();
  cleanAndTerminateThread(threadCtrl);
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

  LOG(ALLB_LOG, LOG_INFO, "Queue send %s '0x%02hhx' to controller (programming)\n", cmd_to_string(_allb_pgm_command), _allb_pgm_command);
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
