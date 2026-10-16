
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


#define _GNU_SOURCE 1 // for strcasestr & strptime
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "aqualink.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "pda.h"
#include "pda_menu.h"
#include "pda_aq_programmer.h"
#include "config.h"
#include "aq_panel.h"
#include "rs_msg_utils.h"
#include "color_lights.h"

#ifdef AQ_DEBUG
  #include "timespec_subtract.h"
#endif

bool waitForPDAMessageHighlight(struct aqualinkdata *aqdata, int highlighIndex, int numMessageReceived);
static bool waitForPDAMessageType(struct aqualinkdata *aqdata, unsigned char mtype,
                                  unsigned long sec, unsigned long msec);
static bool _waitForPDAMessageType(struct aqualinkdata *aqdata, unsigned char mtype,
                                   unsigned long sec, unsigned long msec, bool forceNext);
bool waitForPDAMessageTypes(struct aqualinkdata *aqdata, unsigned char mtype1,
                            unsigned char mtype2, unsigned long sec,
                            unsigned long msec);
bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aqdata,
                                  unsigned char mtype1, unsigned char mtype2,
                                  unsigned char mtype3, unsigned long sec,
                                  unsigned long msec, char *text, int line);
static bool _waitForPDAMessageTypesOrMenu(struct aqualinkdata *aqdata,
                                          unsigned char mtype1, unsigned char mtype2,
                                          unsigned char mtype3, unsigned long sec,
                                          unsigned long msec, char *text, int line,
                                          bool forceNext);
bool waitForPDAMessages(struct aqualinkdata *aqdata, int numberMessages);
bool goto_pda_menu(struct aqualinkdata *aqdata, pda_menu_type menu);
bool wait_pda_selected_item(struct aqualinkdata *aqdata);
bool waitForPDAnextMenu(struct aqualinkdata *aqdata);
bool loopover_devices(struct aqualinkdata *aqdata);
bool find_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, int charlimit);
bool select_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, bool waitForNextMenu);

bool _get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aqdata); 
bool _get_PDA_freeze_protect_temp(struct aqualinkdata *aqdata);

static pda_type _PDA_Type;

//#define USE_ALLBUTTON_QUEUE

#ifndef USE_ALLBUTTON_QUEUE
/* Forcing all these to allbutton for the moment */
void waitfor_queue2empty();
void send_cmd(unsigned char cmd);
unsigned char pop_allb_cmd(struct aqualinkdata *aqdata);
int get_allb_queue_length();

void waitfor_pda_queue2empty() {
  waitfor_queue2empty();
}
void send_pda_cmd(unsigned char cmd) {
  //LOG(PDA_LOG, LOG_DEBUG, "PDA command %d\n", cmd);
  send_cmd(cmd);
}
unsigned char pop_pda_cmd(struct aqualinkdata *aqdata){
  return pop_allb_cmd(aqdata);
}
int get_pda_queue_length(){
  return get_allb_queue_length();
}

#else

// Use out own queue.
// This is NOT working correctly, need to come back and fix

#define MAX_STACK 20
int _pda_cmdstack_place = 0;
unsigned char _pda_cmd_queue[MAX_STACK];
unsigned char _pda_command = NUL;

bool waitfor_pda_queue2empty();

int get_pda_queue_length(){
  return _pda_cmdstack_place;
}

bool push_pda_cmd(unsigned char cmd) {
  _pda_command = cmd;
  /*
  if (_pda_cmdstack_place < MAX_STACK) {
    _pda_cmd_queue[_pda_cmdstack_place] = cmd;
    _pda_cmdstack_place++;
  } else {
    LOG(PDA_LOG, LOG_ERR, "Command queue overflow, too many unsent commands to RS control panel\n");
    return false;
  }
  */
  return true;
}
void send_pda_cmd(unsigned char cmd) {
  if (waitfor_pda_queue2empty()) {
    //LOG(PDA_LOG, LOG_DEBUG, "PDA command %d\n", cmd);
    push_pda_cmd(cmd);
  }
}

unsigned char pop_pda_cmd(struct aqualinkdata *aqdata)
{
  unsigned char cmd = NUL;

  if (_pda_command != NUL && aqdata->last_packet_type == CMD_STATUS) {
    cmd = _pda_command;
    _pda_command = NUL;
  }
/*
  // Only send commands on status messages 
  if (get_pda_queue_length() > 0 && aqdata->last_packet_type == CMD_STATUS) {
    cmd = _pda_cmd_queue[0];
    _pda_cmdstack_place--;
    LOG(PDA_LOG, LOG_DEBUG_SERIAL, "RS SEND cmd '0x%02hhx'\n", cmd);
    memmove(&_pda_cmd_queue[0], &_pda_cmd_queue[1], sizeof(unsigned char) * _pda_cmdstack_place ) ;
  }
*/
  return cmd;
}


bool waitfor_pda_queue2empty() {
  int i=0;

  if (_pda_command != NUL) {
    LOG(PDA_LOG, LOG_DEBUG, "Waiting for queue to empty\n");
  }

  while ( (_pda_command != NUL) && ( i++ < PROGRAMMING_POLL_COUNTER) ) {
    delay(100);
  }
/*
  if (get_pda_queue_length() > 0) {
    LOG(PDA_LOG, LOG_DEBUG, "Waiting for queue to empty\n");
  }

  while ( (get_pda_queue_length() > 0) && ( i++ < PROGRAMMING_POLL_COUNTER) ) {
    delay(100);
  }
*/
  if (i >= PROGRAMMING_POLL_COUNTER) {
    LOG(PDA_LOG, LOG_ERR, "Send command Queue did not empty, timeout\n");
    return false;
  }

  return true;
}

#endif





/* 
// Each RS message / call to this function is around 0.2 seconds apart
//#define MAX_ACK_FOR_THREAD 200 // ~40 seconds (Init takes 30)
#define MAX_ACK_FOR_THREAD 60 // ~12 seconds (testing, will stop every thread)
// *** DELETE THIS WHEN PDA IS OUT OF BETA ****
void pda_programming_thread_check(struct aqualinkdata *aqdata)
{
  static pthread_t thread_id = 0;
  static int ack_count = 0;
  #ifdef AQ_DEBUG
    static struct timespec start;
    static struct timespec now;
    struct timespec elapsed;
  #endif
  // Check for long lasting threads
  if (aqdata->active_thread.thread_id != 0) {
    if (thread_id != *aqdata->active_thread.thread_id) {
       printf ("**************** LAST POINTER SET %ld , %p ****************************\n",thread_id,&thread_id);
     
      thread_id = *aqdata->active_thread.thread_id;
      #ifdef AQ_DEBUG
         clock_gettime(CLOCK_REALTIME, &start);
      #endif
      printf ("**************** NEW POINTER SET %d, %ld %ld , %p %p ****************************\n",aqdata->active_thread.ptype,thread_id,*aqdata->active_thread.thread_id,&thread_id,aqdata->active_thread.thread_id);
      ack_count = 0;
    } else if (ack_count > MAX_ACK_FOR_THREAD) {
      #ifdef AQ_DEBUG
       clock_gettime(CLOCK_REALTIME, &now);
       timespec_subtract(&elapsed, &now, &start);
       LOG(PDA_LOG,LOG_ERR, "Thread %d,%p FAILED to finished in reasonable time, %d.%03ld sec, killing it.\n",
             aqdata->active_thread.ptype,
             aqdata->active_thread.thread_id,
             elapsed.tv_sec, elapsed.tv_nsec / 1000000L);
      #else
        LOG(PDA_LOG,LOG_ERR, "Thread %d,%p FAILED to finished in reasonable time, killing it!\n", aqdata->active_thread.ptype, aqdata->active_thread.thread_id)
      #endif
      if (pthread_cancel(*aqdata->active_thread.thread_id) != 0)
          LOG(PDA_LOG,LOG_ERR, "Thread kill failed\n");
      else {
       
      }
      aqdata->active_thread.thread_id = 0;
      aqdata->active_thread.ptype = AQP_NULL;
      ack_count = 0;
      thread_id = 0;
    } else {
      ack_count++;
    }
  } else {
    ack_count = 0;
    thread_id = 0;
  }
}
*/

bool wait_pda_selected_item(struct aqualinkdata *aqdata)
{
  int i=0;

  while ((pda_m_hlightindex() == -1) && (i < 5)){
    waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHT,2,0);
    i++;
  }

  if (pda_m_hlightindex() == -1)
    return false;
  else
   return true;
}

bool waitForPDAnextMenu(struct aqualinkdata *aqdata) {
  pda_menu_type previous_menu = pda_m_type();

  // A menu command was just queued, so ignore the packet that was current
  // before the command and wait for the controller's response.
  if (!_waitForPDAMessageTypesOrMenu(aqdata,CMD_PDA_CLEAR,CMD_STATUS,0xFF,
                                     2,0,NULL,0,true)) {
    LOG(PDA_LOG,LOG_ERR, "waitForPDAnextMenu - no CLEAR or STATUS\n");
    return false;
  }

  if (aqdata->last_packet_type == CMD_STATUS) {
    // A STATUS packet can precede CLEAR while the controller is still changing menus.
    // Only accept it as the completed transition when it changed the detected menu.
    if (pda_m_type() != previous_menu) {
      LOG(PDA_LOG,LOG_NOTICE, "waitForPDAnextMenu - menu changed on STATUS\n");
      return true;
    }
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAnextMenu - STATUS before CLEAR, waiting for menu transition\n");
    if (!waitForPDAMessageTypesOrMenu(aqdata,CMD_PDA_CLEAR,
                                     CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,
                                     3,0,NULL,0)) {
      LOG(PDA_LOG,LOG_ERR, "waitForPDAnextMenu - no menu transition after STATUS\n");
      return false;
    }
  }

  if (aqdata->last_packet_type == CMD_PDA_CLEAR) {
    if (! waitForPDAMessageTypesOrMenu(aqdata,CMD_PDA_HIGHLIGHT,
                                       CMD_PDA_HIGHLIGHTCHARS,CMD_STATUS,3,
                                       0,NULL,0)) {
      LOG(PDA_LOG,LOG_ERR, "waitForPDAnextMenu - no HIGHLIGHT or STATUS\n");
      return false;
    }
  }

  if (aqdata->last_packet_type == CMD_STATUS) {
    // The firmware-version and status menus do not have a highlight.
    LOG(PDA_LOG,LOG_NOTICE, "waitForPDAnextMenu - received STATUS instead of HIGHLIGHT\n");
  } else if ((aqdata->last_packet_type == CMD_PDA_HIGHLIGHTCHARS) &&
             ((pda_m_type() == PM_EQUIPTMENT_CONTROL) ||
              (pda_m_type() == PM_HOME) ||
              (pda_m_type() == PM_BUILDING_HOME))) {
    if (! waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_STATUS,2,0)) {
      LOG(PDA_LOG,LOG_ERR, "waitForPDAnextMenu - EQUIPTMENT_CONTROL no HIGHLIGHT or STATUS\n");
      return false;
    }
  }
  return true;
}

bool loopover_devices(struct aqualinkdata *aqdata) {
  int i;
  int index = -1;

  if (! goto_pda_menu(aqdata, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "loopover_devices :- can't goto PM_EQUIPTMENT_CONTROL menu\n");
    //cleanAndTerminateThread(threadCtrl);
    return false;
  }
  
  // Should look for message "ALL OFF", that's end of device list.
  for (i=0; i < 18 && (index = pda_find_m_index("ALL OFF")) == -1 ; i++) {
    send_pda_cmd(KEY_PDA_DOWN);
    //waitForMessage(aqdata, NULL, 1);
    waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_MSG_LONG,2,0);
  }
  if (index == -1) {
     LOG(PDA_LOG,LOG_ERR, "loopover_devices :- can't find ALL OFF\n");
     return false;
  }

  return true;
}

/*
  if charlimit is set, use case insensitive match and limit chars.
  if charlimit is -1, use VERY loose matching.
*/
bool find_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, int charlimit) {
  int i=pda_m_hlightindex();
  int min_index = -1;
  int max_index = -1;
  int index = -1;
  int cnt = 0;
  bool bLookingForBoost = false;

  LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer looking for menu text '%s' (limit=%d)\n",menuText,charlimit);

  if (charlimit == 0)
    index = pda_find_m_index(menuText);
  else if (charlimit > 0)
    index = pda_find_m_index_case(menuText, charlimit);
  else if (charlimit == -1)
    index = pda_find_m_index_loose(menuText);

  //int index = (charlimit == 0)?pda_find_m_index(menuText):pda_find_m_index_case(menuText, charlimit);

  if (index < 0) { // No menu, is there a page down.  "PDA Line 9 =    ^^ MORE __"
    if (strncasecmp(pda_m_line(9),"   ^^ MORE", 10) == 0) {
      int j;
      for(j=0; j < 20; j++) {
        send_pda_cmd(KEY_PDA_DOWN);
        // NSF MAYBE ADD THIS NEEd TO TEST EVERYTHING ON ALL VERSIONS FIRST.
        //waitForPDAMessages(aqdata, 1); // PDA needs another message sometimes (probably not using )
        waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_MSG_LONG,2,0);
        //waitForMessage(aqdata, NULL, 1);
        index = (charlimit == 0)?pda_find_m_index(menuText):pda_find_m_index_case(menuText, charlimit);
        if (index >= 0) {
          i=pda_m_hlightindex();
          break;
        }
      }
      if (index < 0) {
        LOG(PDA_LOG,LOG_ERR, "PDA Device programmer couldn't find menu item on any page '%s'\n",menuText);
        return false;
      }
    } else {
      LOG(PDA_LOG,LOG_ERR, "PDA Device programmer couldn't find menu item '%s' in menu %d index %d\n", menuText, pda_m_type(), index);
      return false;
    }
  }

  if (strncasecmp(pda_m_line(9),"   ^^ MORE", 10) != 0) {
    if (pda_m_type() == PM_HOME) {
        min_index = 4;
        max_index = 9;
    } else if (pda_m_type() == PM_EQUIPTMENT_CONTROL) {
        min_index = 1;
        max_index = 9;
    } else if (pda_m_type() == PM_MAIN) {
      // Line 0 =    MAIN MENU
      // Line 1 =
      // Line 2 = HELP           >
      // Line 3 = PROGRAM        >
      // Line 4 = SET TEMP       >
      // Line 5 = SET TIME       >
      // Line 6 = PDA OPTIONS    >
      // Line 7 = SYSTEM SETUP   >
      // Line 8 =
      // Line 9 =

      // Line 0 =    MAIN MENU
      // Line 1 = HELP           >
      // Line 2 = PROGRAM        >
      // Line 3 = SET TEMP       >
      // Line 4 = SET TIME       >
      // Line 5 = SET AquaPure   >
      // Line 6 = PDA OPTIONS    >
      // Line 7 = SYSTEM SETUP   >
      // Line 8 =
      // Line 9 =      BOOST

      // "SET AquaPure" and "BOOST" are only present when filter pump is running
      if ((strncasecmp(pda_m_line(9),"     BOOST      ", 16) == 0) ||
          (strncasecmp(pda_m_line(9),"   BOOST POOL   ", 16) == 0)) {
        min_index = 1;
        max_index = 8; // to account for 8 missing
        if (index == 9) { // looking for boost
          bLookingForBoost = true;
          index = 8;
        }
      } else {
          min_index = 2;
          max_index = 7;
      }
    } else if (pda_m_type() == PM_BOOST) {
      // PDA Line 0 =      BOOST
      // PDA Line 1 =
      // PDA Line 2 =   Operate the
      // PDA Line 3 =     AquaPure
      // PDA Line 4 =   chlorinator
      // PDA Line 5 =     at 100%
      // PDA Line 6 =   for 24 hrs.
      // PDA Line 7 =
      // PDA Line 8 =      START
      // PDA Line 9 =     GO BACK

      // PDA Line 0 =      BOOST
      // PDA Line 1 =
      // PDA Line 2 =
      // PDA Line 3 =  TIME REMAINING
      // PDA Line 4 =      23:59
      // PDA Line 5 =
      // PDA Line 6 =
      // PDA Line 7 =       PAUSE
      // PDA Line 8 =      RESTART
      // PDA Line 9 =       STOP

      if (strncasecmp(pda_m_line(9),"      STOP", 10) == 0) {
        min_index = 7;
        max_index = 9;
      } else {
        min_index = 8;
        max_index = 9;

      }
    }
  }

  LOG(PDA_LOG,LOG_DEBUG, "find_pda_menu_item i=%d idx=%d min=%d max=%d boost=%d\n",
             i, index, min_index, max_index, bLookingForBoost?1:0);

  if (i < index) {
    if ((min_index != -1) && ((index - i) > (i - min_index + max_index - index + 1))) {
        cnt = i - min_index + max_index - index + 1;
        for (i=0; i < cnt; i++) {
          waitfor_pda_queue2empty();
          send_pda_cmd(KEY_PDA_UP);
        }
    } else {
        for (i=pda_m_hlightindex(); i < index; i++) {
            waitfor_pda_queue2empty();
            send_pda_cmd(KEY_PDA_DOWN);
        }
    }
  } else if (i > index) {
    if ((min_index != -1) && ((i - index) > (index - min_index + max_index - i + 1))) {
        cnt = i - min_index + max_index - index + 1;
        for (i=0; i < cnt; i++) {
          waitfor_pda_queue2empty();
          send_pda_cmd(KEY_PDA_UP);
        }
    } else {
      for (i=pda_m_hlightindex(); i > index; i--) {
        waitfor_pda_queue2empty();
        send_pda_cmd(KEY_PDA_UP);
      }
    }
  }
  return waitForPDAMessageHighlight(aqdata, bLookingForBoost?9:index, 10);
}

bool _select_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, bool waitForNextMenu, bool loose);

bool select_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, bool waitForNextMenu){
  return _select_pda_menu_item(aqdata, menuText, waitForNextMenu, false);
}
bool select_pda_menu_item_loose(struct aqualinkdata *aqdata, char *menuText, bool waitForNextMenu){
  return _select_pda_menu_item(aqdata, menuText, waitForNextMenu, true);
}
bool _select_pda_menu_item(struct aqualinkdata *aqdata, char *menuText, bool waitForNextMenu, bool loose) {

  //int matchType = loose?-1:0; // NSF release 2.1.0 was this and it worked.  Need to re-check why I did this.
  //int matchType = loose?-1:1;
  int matchType = loose?-1:strlen(menuText); // NSF Not way to check this. (release 2.2.0 introduced this with the line above)
  if ( find_pda_menu_item(aqdata, menuText, matchType) ) {
    send_pda_cmd(KEY_PDA_SELECT);

    LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer selected menu item '%s'\n",menuText);
    if (waitForNextMenu)
      return waitForPDAnextMenu(aqdata);

    return true;
  }

  LOG(PDA_LOG,LOG_ERR, "PDA Device programmer couldn't select menu item '%s' menu %d\n",menuText, pda_m_type());
  return false;
}

// for reference see H0572300 - AquaLink PDA I/O Manual
// https://www.jandy.com/-/media/zodiac/global/downloads/h/h0572300.pdf
// and H0574200 - AquaPalm Wireless Handheld Remote Installation and Operation Manual
// https://www.jandy.com/-/media/zodiac/global/downloads/h/h0574200.pdf
// and 6594 - AquaLink RS Control Panel Installation Manual
// https://www.jandy.com/-/media/zodiac/global/downloads/0748-91071/6594.pdf

bool goto_pda_menu(struct aqualinkdata *aqdata, pda_menu_type menu) {
  bool ret = true;
  int cnt = 0;
  pda_menu_type prev_menu = PM_UNKNOWN;

  LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer request for menu %d, current %d\n",
             menu, pda_m_type());

  if (pda_m_type() == PM_FW_VERSION) {
      LOG(PDA_LOG,LOG_DEBUG, "goto_pda_menu at FW version menu\n");
      send_pda_cmd(KEY_PDA_BACK);
      if (! waitForPDAnextMenu(aqdata)) {
        LOG(PDA_LOG,LOG_ERR, "PDA Device programmer wait for next menu failed\n");
      } else if ((pda_m_type() != PM_BUILDING_HOME) && (pda_m_type() != PM_HOME)) {
        LOG(PDA_LOG,LOG_NOTICE, "goto_pda_menu went from FW_VERSION to %d\n", pda_m_type());
      }
  }

  while (ret && (pda_m_type() != menu) && (cnt <= 5)) {
    if (pda_m_type() == PM_BUILDING_HOME) {
      LOG(PDA_LOG,LOG_DEBUG, "goto_pda_menu building home menu\n");
      if (! (ret=waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHT,3,0))) {
        LOG(PDA_LOG,LOG_ERR, "goto_pda_menu building home wait for highlight failed\n");
        break;
      }
      if (menu == pda_m_type()) {
        break;
      }
    }
    prev_menu = pda_m_type();
    switch (menu) {
      case PM_HOME:
         send_pda_cmd(KEY_PDA_BACK);
         ret = waitForPDAnextMenu(aqdata);
         break;
      case PM_EQUIPTMENT_CONTROL:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "EQUIPMENT ON/OFF", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        break;
      case PM_PALM_OPTIONS:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aqdata, "PALM OPTIONS", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        break;
      case PM_AUX_LABEL:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aqdata, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aqdata, "SYSTEM SETUP", true);
            } else if (pda_m_type() == PM_SYSTEM_SETUP) {
                ret = select_pda_menu_item(aqdata, "LABEL AUX", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aqdata);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / LABEL AUX menu\n");
        }
        break;
      case PM_SYSTEM_SETUP:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aqdata, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aqdata, "SYSTEM SETUP", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aqdata);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP menu\n");
        }
        break;
      case PM_FREEZE_PROTECT:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aqdata, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aqdata, "SYSTEM SETUP", true);
            } else if (pda_m_type() == PM_SYSTEM_SETUP) {
                ret = select_pda_menu_item(aqdata, "FREEZE PROTECT", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aqdata);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / FREEZE PROTECT menu\n");
        }
        break;
      case PM_AQUAPURE:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aqdata, "SET AquaPure", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        break;
      case PM_BOOST:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item_loose(aqdata, "BOOST", true);
            //ret = select_pda_menu_item(aqdata, "BOOST", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        //printf("****MENU SELECT RETURN %d*****\n",ret);
        break;
      case PM_SET_TEMP:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
          if (isCOMBO_PANEL) {
            ret = select_pda_menu_item(aqdata, "SET TEMP", true);
          } else {
              // Depending on control panel config, may get an extra menu asking to press any key
              LOG(PDA_LOG,LOG_DEBUG, "PDA in single device mode, \n");
              ret = select_pda_menu_item(aqdata, "SET TEMP", false);
              // We could press enter here, but I can't test it, so just wait for message to dissapear.
              ret = waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,5,0);
              //waitForPDAMessageType(aqdata,CMD_PDA_CLEAR,2,0);
              //waitForPDAMessageTypesOrMenu(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,CMD_STATUS,5,0,"press ANY key",8);
            }
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        break;
      case PM_SET_TIME:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aqdata, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aqdata, "SET TIME", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aqdata);
        }
        break;
      default:
        LOG(PDA_LOG,LOG_ERR, "PDA Device programmer didn't understand requested menu\n");
        return false;
        break;
    }
    if (prev_menu == pda_m_type()) {
      LOG(PDA_LOG,LOG_ERR, "PDA Device programmer request for menu %d, stuck on %d\n",
                 menu, pda_m_type());
      break;
    }
    LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer '%s' request for menu %d, current %d\n", get_current_programming_mode_name(aqdata), menu, pda_m_type());
    cnt++;
  }
  if (pda_m_type() != menu) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device programmer '%s' didn't find a requested menu %d, current %d\n", get_current_programming_mode_name(aqdata), menu, pda_m_type());
    return false;
  }

  return true;
}

void goto_pda_home_first(struct aqualinkdata *aqdata)
{
  if (!_aqconfig_.pda_force_home_onprogram)
    return;

  // Before start programming sequence, go back to home menu. Otherwise may get timing issue.
  // AquaLinkD may send SELECT at EQUIPMNET menu while PDA just switched to the status page.
  // In such case, the SELECT will be ignored.
  send_pda_cmd(KEY_PDA_BACK);
  if (!waitForPDAnextMenu(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA goto menu: can't find HOME menu\n");
  }
  //
  // Wait for another menu. PDA may going back to the home menu while the KEY_PDA_BACK
  // is still being transmitted. As such, wait for second time in case the menu received
  // is from the PDA going back to menu. At which point, there should be a second
  // menu coming. Otherwse, the code proceeds and may be selecting at the main menu and turn
  // off the pool equipment.
  waitForPDAnextMenu(aqdata);
}

#ifdef NEW_AQ_PROGRAMMER
void *set_aqualink_PDA_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //int i=0;
  //int found;
  char device_name[15];
  
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int state = pargs->value;
  int device = pargs->alt_value;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_ON_OFF);
  
  LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",button->label,state);

  goto_pda_home_first(aqdata); // if enabled

  if (! goto_pda_menu(aqdata, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off :- can't find EQUIPTMENT CONTROL menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // If single config (Spa OR pool) rather than (Spa AND pool) heater is TEMP1 and TEMP2
  if (isSINGLE_DEV_PANEL && device == aqdata->pool_heater_index) { // rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", "TEMP1");
  } else if (isSINGLE_DEV_PANEL && device == aqdata->spa_heater_index)  {// rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", "TEMP2");
  } else {
    //Pad name with spaces so something like "SPA" doesn't match "SPA BLOWER"
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", button->label);
  }

  // NSF Added this since DEBUG hitting wrong command
  //waitfor_pda_queue2empty();

  if ( find_pda_menu_item(aqdata, device_name, 12) ) { // Remove 1 char to account for '100%' (4 chars not the usual 3)
    if (button->led->state != state) {
      //printf("*** Select State ***\n");
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', changing state\n",button->label);
      force_queue_delete(); // NSF This is a bad thing to do.  Need to fix this
      send_pda_cmd(KEY_PDA_SELECT);
      waitfor_pda_queue2empty();
      // If you are turning on a heater there will be a sub menu to set temp
      if ((state == ON) && ((device == aqdata->pool_heater_index) || (device == aqdata->spa_heater_index))) {
        if (! waitForPDAnextMenu(aqdata)) {
          LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - waitForPDAnextMenu\n", button->label);
        } else {
          send_pda_cmd(KEY_PDA_SELECT);
          waitfor_pda_queue2empty();
          if (!waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHT,5,0)) {
            LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - wait for CMD_PDA_HIGHLIGHT\n",button->label);
          }
        }
      } else if ( isPLIGHT(button->special_mask) ) {
        // THIS EXTRA ENTER IS ONLY FOR ON, NOT OFF
        // PDA Menu Line 0 =    Set Color   // for color light
        // PDA Menu Line 0 =       Set      // for dimmer light
        if ( state == ON ) {
          waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,1,0);
          if (strncasecmp(pda_m_line(0),"Set", 3) == 0) {
            LOG(PDA_LOG,LOG_DEBUG, "PDA Device On/Off, '%s' is programmable light, but no mode using default\n",button->label);
            send_pda_cmd(KEY_PDA_SELECT);
          } else {
            LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: expected Set menu for programmable light '%s', not found\n",button->label);
          }
        }
      } else { // not turning on heater wait for line update
          // worst case spa when pool is running
          if (!waitForPDAMessageType(aqdata,CMD_MSG_LONG,0,500)) {
              LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s - wait for status update\n",button->label);
          }
      }
      
    } else {
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', not changing state, is same\n",button->label,state);
    }
  } else {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off, device '%s' not found\n",button->label);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;

}
#else
void *set_aqualink_PDA_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //int i=0;
  //int found;
  char device_name[15];
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_ON_OFF);
  
  char *buf = (char*)threadCtrl->thread_args;
  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  if (device > aqdata->total_buttons) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off :- bad device number '%d'\n",device);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",aqdata->aqbuttons[device].label,state);

  goto_pda_home_first(aqdata); // if enabled

  if (! goto_pda_menu(aqdata, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off :- can't find EQUIPTMENT CONTROL menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // If single config (Spa OR pool) rather than (Spa AND pool) heater is TEMP1 and TEMP2
  if (isSINGLE_DEV_PANEL && device == aqdata->pool_heater_index) { // rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", "TEMP1");
  } else if (isSINGLE_DEV_PANEL && device == aqdata->spa_heater_index)  {// rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", "TEMP2");
  } else {
    //Pad name with spaces so something like "SPA" doesn't match "SPA BLOWER"
    snprintf(device_name, sizeof(device_name), "%-13.13s\n", aqdata->aqbuttons[device].label);
  }

  // NSF Added this since DEBUG hitting wrong command
  //waitfor_pda_queue2empty();

  if ( find_pda_menu_item(aqdata, device_name, 12) ) { // Remove 1 char to account for '100%' (4 chars not the usual 3)
    if (aqdata->aqbuttons[device].led->state != state) {
      //printf("*** Select State ***\n");
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', changing state\n",aqdata->aqbuttons[device].label);
      force_queue_delete(); // NSF This is a bad thing to do.  Need to fix this
      send_pda_cmd(KEY_PDA_SELECT);
      waitfor_pda_queue2empty();
      // If you are turning on a heater there will be a sub menu to set temp
      if ((state == ON) && ((device == aqdata->pool_heater_index) || (device == aqdata->spa_heater_index))) {
        if (! waitForPDAnextMenu(aqdata)) {
          LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - waitForPDAnextMenu\n", aqdata->aqbuttons[device].label);
        } else {
          send_pda_cmd(KEY_PDA_SELECT);
          waitfor_pda_queue2empty();
          if (!waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHT,5,0)) {
            LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - wait for CMD_PDA_HIGHLIGHT\n",aqdata->aqbuttons[device].label);
          }
        }
      } else if ( isPLIGHT(aqdata->aqbuttons[device].special_mask) ) {
        // THIS EXTRA ENTER IS ONLY FOR ON, NOT OFF
        // PDA Menu Line 0 =    Set Color   // for color light
        // PDA Menu Line 0 =       Set      // for dimmer light
        if ( state == ON ) {
          waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,1,0);
          if (strncasecmp(pda_m_line(0),"Set", 3) == 0) {
            LOG(PDA_LOG,LOG_DEBUG, "PDA Device On/Off, '%s' is programmable light, but no mode using default\n",aqdata->aqbuttons[device].label);
            send_pda_cmd(KEY_PDA_SELECT);
          } else {
            LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: expected Set menu for programmable light '%s', not found\n",aqdata->aqbuttons[device].label);
          }
        }
      } else { // not turning on heater wait for line update
          // worst case spa when pool is running
          if (!waitForPDAMessageType(aqdata,CMD_MSG_LONG,0,500)) {
              LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s - wait for status update\n",
                         aqdata->aqbuttons[device].label);
          }
      }
      
    } else {
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', not changing state, is same\n",aqdata->aqbuttons[device].label,state);
    }
  } else {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off, device '%s' not found\n",aqdata->aqbuttons[device].label);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;

}
#endif

bool waitForLightCycleMessage(struct aqualinkdata *aqdata)
{

  waitfor_queue2empty();
  
  // Wait for the message to appear.
  waitForPDAMessages(aqdata, 15);

  // check the message did appear .....  PDA Menu Line 4 =      Please
  if (rsm_strmatch(pda_m_line(4), "Please") == 0) 
  {       
    // Wait for it to disapear                                                                         
    waitForPDAMessageTypes(aqdata, CMD_PDA_HIGHLIGHT, CMD_PDA_HIGHLIGHTCHARS, 12, 0); // Long wait
  }
  else
  {
    LOG(PDA_LOG, LOG_WARNING, "PDA light Programming :- Didn't see Cycling message\n");
    return false;
  }

  return true;
}

bool waitForLightOffMessage(struct aqualinkdata *aqdata)
{
  if (rsm_strmatch(pda_m_line(3),"Light will turn") == 0) {
    waitForPDAnextMenu(aqdata);
  } else {
    LOG(PDA_LOG,LOG_WARNING, "PDA light Programming :- Didn't see off message\n");
    return false;
  }
  return true;
}

void *set_aqualink_PDA_light_mode( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  bool use_current_mode = false;
  const char *mode_name = NULL;
  //int i=0;
  //int found;
  //char device_name[15];

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_LIGHT_MODE); 

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int mode = pargs->value;
  use_current_mode = pargs->alt_value;
  //clight_type typ = ((clight_detail *)button->special_mask_ptr)->lightType;
  clight_detail *light = (clight_detail *)button->special_mask_ptr;
  clight_type typ = light->lightType;
#else
  char *buf = (char*)threadCtrl->thread_args;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int typ = atoi(&buf[10]);

  if (btn < 0 || btn >= aqdata->total_buttons ) {
    LOG(PDA_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  aqkey *button = &aqdata->aqbuttons[btn];
#endif

  if ( ! isPLIGHT(button->special_mask) ) {
    LOG(PDA_LOG, LOG_ERR, "Can't program light mode on button '%s', it's not a programmable light\n", button->label);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  mode_name = light_mode_name(typ, mode, AQUAPDA);

  if (mode_name == NULL) {
      LOG(PDA_LOG, LOG_ERR, "PDA Light Programming #: Received %d, on button: %s, color light type: %d, couldn't find mode name\n", mode, button->label, typ);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
  } else {
      LOG(PDA_LOG, LOG_INFO, "PDA Light Programming #: Received %d, on button: %s, color light type: %d, name '%s'\n", mode, button->label, typ, mode_name);
  }
  
  goto_pda_home_first(aqdata); // if enabled

  if (! goto_pda_menu(aqdata, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "PDA light Programming :- can't find EQUIPTMENT CONTROL menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  if ( find_pda_menu_item(aqdata, button->label, 0) ) { // Remove 1 char to account for '100%' (4 chars not the usual 3)
    LOG(PDA_LOG,LOG_INFO, "PDA Light Programming, found device '%s', changing to '%s'\n",button->label,mode_name);
    force_queue_delete(); // NSF This is a bad thing to do.  Need to fix this
    // get the status as it would have been updated by pda.c seeing the state so we know it's accurate.
    // BUT, it could change after next key press
    aqledstate current_state = button->led->state;
    send_pda_cmd(KEY_PDA_SELECT);
    waitfor_queue2empty();
    waitForPDAMessages(aqdata, 15);  // We get a number of different things here depending on light state, so simply wait 15 messages

    if (typ == LC_DIMMER2 || typ == LC_DIMMER) {
      if (mode == 0) { 
        // We are simply turning it off, and that would have happened above, so do nothing but wait for the light turn off message
        //waitForLightOffMessage(aqdata);
      } else {
        if (current_state == ON && mode > 0) { // Need to use the state BEFORE the last key press
          // Button was on, and we are changing mode so turn it on as the previous send_pda_cmd(KEY_PDA_SELECT)
          // would have tured it off, so turn it on
          send_pda_cmd(KEY_PDA_SELECT);
          waitfor_queue2empty();
          waitForPDAMessages(aqdata, 5);
        }
        if (use_current_mode) {
          char *current_mode = pda_m_hlight();
          send_pda_cmd(KEY_PDA_SELECT);
          mode = light_mode_index(typ, current_mode);
          LOG(PDA_LOG,LOG_INFO, "PDA light Programming :- Current Mode = %d '%s'\n",mode,current_mode);
          // No light cycling message at this point.
        } else {
          //int current = rsm_atoi(pda_m_line(4));
          //while(current != mode_name)
          int i = 0;
          while(rsm_strmatch(pda_m_line(4), mode_name) != 0) {
            send_pda_cmd(KEY_PDA_DOWN);
            waitfor_queue2empty();
            waitForPDAMessages(aqdata, 2);
            if (++i > 6) {
              LOG(PDA_LOG,LOG_ERR, "PDA light Programming :- Couldn't find %s\n",mode_name);
              break;
            }
          }
          if (rsm_strmatch(pda_m_line(4), mode_name) == 0) {
            send_pda_cmd(KEY_PDA_SELECT);
            waitfor_queue2empty();
            waitForPDAMessages(aqdata, 5);
          }
        }
      }
    } else {
      // Turn off look for "Light will turn" and simply wait.
      // Turn on to default, get light color name from menu and press select.
      // Turn to mode, loop over mode options.
      if (mode == 0) { // off
        //waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_STATUS,1,0); // Wait for the actual text to show.
        if (waitForLightOffMessage(aqdata)) {
          light->button->led->state = OFF;
        }
      } else if (use_current_mode) { // use current
        //waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,3,0);
        char *current_color = pda_m_hlight();
        send_pda_cmd(KEY_PDA_SELECT);
        // Reset the mode indet
        mode = light_mode_index(typ, current_color);
        LOG(PDA_LOG,LOG_INFO, "PDA light Programming :- Current Color = %d '%s'\n",mode,current_color);
        waitForLightCycleMessage(aqdata);
      } else { // set mode.
        if (strncasecmp(pda_m_line(3),"Light will turn", 15) == 0) {
          // If light is currently on, we will get this message, and need to clear it.
          waitForPDAMessages(aqdata, 5);  // PDA needs another 5 messages. Otherwise, sometime it does not observed PDA SELECT
          send_pda_cmd(KEY_PDA_SELECT);
          waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,3,0);
        }
        if (find_pda_menu_item(aqdata,(char *)mode_name,strlen(mode_name))) {
          send_pda_cmd(KEY_PDA_SELECT);
          waitForLightCycleMessage(aqdata);
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA Light Programming, could find mode '%s' for device '%s'\n",mode_name,button->label);
        }
      }
    }
    if (mode > 0) {updateLightProgram(aqdata, mode, light);}
  } else {
    LOG(PDA_LOG,LOG_ERR, "PDA Light Programming, device '%s' not found\n",button->label);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *get_aqualink_PDA_device_status( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //int i;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_STATUS);
  
  goto_pda_home_first(aqdata); // if enabled
  goto_pda_menu(aqdata, PM_HOME);

  if (! loopover_devices(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device Status :- failed\n");
  }
 
  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_PDA_init( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //int i=0;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_INIT);
  
  //int val = atoi((char*)threadCtrl->thread_args);

  //LOG(PDA_LOG,LOG_DEBUG, "PDA Init\n", val);

  LOG(PDA_LOG,LOG_DEBUG, "PDA Init\n");

  if (pda_m_type() == PM_FW_VERSION) {
    // check pda_m_line(1) to "AquaPalm"
    if (strstr(pda_m_line(1), "AquaPalm") != NULL) {
      _PDA_Type = AQUAPALM;
    } else {
      _PDA_Type = PDA;
    }

    setPanelInformationFromPanelMsg(aqdata, pda_m_line(1), PANEL_STRING,AQUAPDA);
    setPanelInformationFromPanelMsg(aqdata, pda_m_line(5), PANEL_REV, AQUAPDA);
    //setPanelInformationFromPanelMsg(aqdata, "     PDA: 7.1.0", PANEL_REV, AQUAPDA);
/*
    char *ptr1 = pda_m_line(1);
    char *ptr2 = pda_m_line(5);
    ptr1[AQ_MSGLEN+1] = '\0';
    ptr2[AQ_MSGLEN+1] = '\0';
    //strcpy(aqdata->version, stripwhitespace(ptr));
    snprintf(aqdata->version, (AQ_MSGLEN*2)-1, "%s %s",stripwhitespace(ptr1),stripwhitespace(ptr2));

    //printf("****** Version '%s' ********\n",aqdata->version);
    LOG(PDA_LOG,LOG_DEBUG, "PDA type=%d, version=%s\n", _PDA_Type, aqdata->version);
 */   
    // don't wait for version menu to time out press back to get to home menu faster
    send_pda_cmd(KEY_PDA_BACK);
  }
  else {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- should be called when on FW VERSION menu.\n");
  }
/* 
  // Get status of all devices
  if (! loopover_devices(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- can't find menu\n");
  }
*/

  // Get heater setpoints
  if (! _get_PDA_aqualink_pool_spa_heater_temps(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- Error getting heater setpoints\n");
  }

  //goto_pda_menu(aqdata, PM_HOME);

  // Get freeze protect setpoint, AquaPalm doesn't have freeze protect in menu.
  if (_PDA_Type != AQUAPALM && ! _get_PDA_freeze_protect_temp(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- Error getting freeze setpoints\n");
  }

  goto_pda_menu(aqdata, PM_HOME);

  pda_reset_sleep();

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_aqualink_PDA_wakeinit( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //int i=0;

  // At this point, we should probably just exit if there is a thread already going as 
  // it means the wake was called due to changing a device.
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_WAKE_INIT);

  LOG(PDA_LOG,LOG_DEBUG, "PDA Wake Init\n");

  // Get status of all devices
  if (! loopover_devices(aqdata)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- can't find menu\n");
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


bool _get_PDA_freeze_protect_temp(struct aqualinkdata *aqdata) {
  
  if ( _PDA_Type == PDA) {
    if (! goto_pda_menu(aqdata, PM_FREEZE_PROTECT)) {   
      return false;
    }
    /* select the freeze protect temp to see which devices are enabled by freeze
       protect */
    send_pda_cmd(KEY_PDA_SELECT);
    return waitForPDAnextMenu(aqdata);
  } else {
    LOG(PDA_LOG,LOG_INFO, "In PDA AquaPalm mode, freezepoints not supported\n");
    return false;
  }
}

bool _get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aqdata) {
  
   // Get heater setpoints
  if (! goto_pda_menu(aqdata, PM_SET_TEMP)) {
    LOG(PDA_LOG,LOG_ERR, "Could not get heater setpoints, trying again!\n");
    // Going to try this twice.
    if (! goto_pda_menu(aqdata, PM_SET_TEMP)) {
      return false;
    }
  }
  
  return true;
}

void *get_PDA_aqualink_pool_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_POOL_SPA_HEATER_TEMPS);
  goto_pda_home_first(aqdata); // if enabled
  _get_PDA_aqualink_pool_spa_heater_temps(aqdata);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

void *get_PDA_freeze_protect_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_FREEZE_PROTECT_TEMP);
  goto_pda_home_first(aqdata); // if enabled
  _get_PDA_freeze_protect_temp(aqdata);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

bool waitForPDAMessageHighlight(struct aqualinkdata *aqdata, int highlighIndex, int numMessageReceived)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight index %d\n",highlighIndex);

  if(pda_m_hlightindex() == highlighIndex) return true;

  int i=0;
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight last = 0x%02hhx : index %d : (%d of %d)\n",aqdata->last_packet_type,pda_m_hlightindex(),i,numMessageReceived);

    if (aqdata->last_packet_type == CMD_PDA_HIGHLIGHT && pda_m_hlightindex() == highlighIndex) break;

    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);
  
  if (pda_m_hlightindex() != highlighIndex) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight: did not receive index '%d'\n",highlighIndex);
    return false;
  } else 
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight: received index '%d'\n",highlighIndex);
  
  return true;
}


static bool _waitForPDAMessageType(struct aqualinkdata *aqdata, unsigned char mtype,
                                   unsigned long sec, unsigned long msec, bool forceNext)
{
  return _waitForPDAMessageTypesOrMenu(aqdata, mtype, 0xFF, 0xFF, sec, msec,
                                       NULL, 0, forceNext);
}

static bool waitForPDAMessageType(struct aqualinkdata *aqdata, unsigned char mtype,
                                  unsigned long sec, unsigned long msec)
{
  return _waitForPDAMessageType(aqdata, mtype, sec, msec, false);
}

bool waitForPDANextMessageType(struct aqualinkdata *aqdata, unsigned char mtype,
                               unsigned long sec, unsigned long msec)
{
  return _waitForPDAMessageType(aqdata, mtype, sec, msec, true);
}

bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aqdata,
                                  unsigned char mtype1, unsigned char mtype2,
                                  unsigned char mtype3, unsigned long sec,
                                  unsigned long msec, char *text, int line)
{
  return _waitForPDAMessageTypesOrMenu(aqdata, mtype1, mtype2, mtype3, sec,
                                       msec, text, line, false);
}

static bool _waitForPDAMessageTypesOrMenu(struct aqualinkdata *aqdata,
                                          unsigned char mtype1, unsigned char mtype2,
                                          unsigned char mtype3, unsigned long sec,
                                          unsigned long msec, char *text, int line,
                                          bool forceNext)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypesOrMenu 0x%02hhx,0x%02hhx,0x%02hhx,%s,%d,%lu.%03lu sec, fn %d\n",
      mtype1,mtype2,mtype3,text,line,sec,msec,forceNext);

  int i=0;
  bool gotmenu = false;
  struct timespec max_wait;
  int ret = 0;

  if (msec > 999) {
    LOG(PDA_LOG,LOG_ERR, "waitForPDAMessageTypesOrMenu INVALID msec value %lu\n", msec);
  }
  clock_gettime(CLOCK_REALTIME, &max_wait);
  max_wait.tv_sec += sec;
  max_wait.tv_nsec += msec * 1000000;
  if (max_wait.tv_nsec > 999999999L) {
    max_wait.tv_nsec -= 1000000000L;
    max_wait.tv_sec++;
  }
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  if (forceNext) { // Ignore the current message type and wait for the next message.
    if ((ret = pthread_cond_timedwait(&aqdata->active_thread.thread_cond,
                                      &aqdata->active_thread.thread_mutex, &max_wait))) {
      LOG(PDA_LOG,LOG_ERR, "waitForPDAMessageTypesOrMenu 0x%02hhx,0x%02hhx,%s,%d - %s\n",
          mtype1,mtype2,text,line,strerror(ret));
      pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);
      return false;
    }
  }

  while (true) {
    i++;
    if (gotmenu == false && line > 0 && text != NULL) {
      if (stristr(pda_m_line(line), text) != NULL) {
        send_pda_cmd(KEY_PDA_SELECT);
        gotmenu = true;
        LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypesOrMenu saw '%s' in line %d\n",text,line);
      }
    }
    if (aqdata->last_packet_type == mtype1 || aqdata->last_packet_type == mtype2 ||
        aqdata->last_packet_type == mtype3) {
      break;
    }
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypesOrMenu last message type 0x%02hhx (%d)\n",
        aqdata->last_packet_type,i);
    if ((ret = pthread_cond_timedwait(&aqdata->active_thread.thread_cond,
                                      &aqdata->active_thread.thread_mutex, &max_wait))) {
      LOG(PDA_LOG,LOG_ERR, "waitForPDAMessageTypesOrMenu 0x%02hhx,0x%02hhx,0x%02hhx,%s,%d - %s\n",
          mtype1,mtype2,mtype3,text,line,strerror(ret));
      break;
    }
  }

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  if (aqdata->last_packet_type != mtype1 &&
      aqdata->last_packet_type != mtype2 &&
      aqdata->last_packet_type != mtype3) {
    LOG(PDA_LOG,LOG_ERR, "waitForPDAMessageTypesOrMenu: did not receive 0x%02hhx, 0x%02hhx or 0x%02hhx\n",
        mtype1,mtype2,mtype3);
    return false;
  } else {
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypesOrMenu: received 0x%02hhx\n",aqdata->last_packet_type);
  }
  return true;
}

bool waitForPDAMessageTypes(struct aqualinkdata *aqdata, unsigned char mtype1,
                            unsigned char mtype2, unsigned long sec,
                            unsigned long msec)
{
  return waitForPDAMessageTypesOrMenu(aqdata, mtype1, mtype2, 0xFF, sec, msec, NULL, 0);
}

bool waitForPDAMessages(struct aqualinkdata *aqdata, int numberMessages)
{
  int received=0;

  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);
  while( ++received <= numberMessages)
  {
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessages: %d of %d\n",received,numberMessages);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
  }
  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  return true;
}

/*
  Use -1 for cur_val if you want this to find the current value and change it.
  Use number for cur_val to  increase / decrease from known start point
*/

bool set_PDA_numeric_field_value(struct aqualinkdata *aqdata, int val, int cur_val, char *select_label, int step) {
  int i=0;

  LOG(PDA_LOG,LOG_DEBUG, "set_PDA_numeric_field_value %s from %d to %d step %d\n", select_label, cur_val, val, step);
  if (select_label != NULL) {
    //if ( ! select_pda_menu_item(aqdata, select_label, false) ) {
    // Changed to loose for menu items that don't start at char position 1, like SWG message "   SET TO 100%".
    if ( ! select_pda_menu_item_loose(aqdata, select_label, false) ) {
      return false;
    }
  }

  if (cur_val == -1) {
    char *hghlight_chars;
    int hlight_length=0;
    int i=0;
    hghlight_chars = pda_m_hlightchars(&hlight_length); // NSF May need to take this out and therefore the LOG entry after while
    while (hlight_length >= 15 || hlight_length <= 0) {
      delay(500);
      waitForPDANextMessageType(aqdata,CMD_PDA_HIGHLIGHTCHARS,1,0);
      hghlight_chars = pda_m_hlightchars(&hlight_length);
      LOG(PDA_LOG,LOG_DEBUG, "Numeric selector, highlight chars '%.*s'\n",hlight_length , hghlight_chars);
      if (++i >= 20) {
        LOG(PDA_LOG,LOG_ERR, "Numeric selector, didn't find highlight chars, current selection is '%.*s'\n",hlight_length , hghlight_chars);
        return false;
      }
    }

    cur_val = atoi(hghlight_chars);
    LOG(PDA_LOG,LOG_DEBUG, "Numeric selector, highlight chars '%.*s', numeric value using %d\n",hlight_length , hghlight_chars, cur_val);
  }

  if (val < cur_val) {
    LOG(PDA_LOG,LOG_DEBUG, "Numeric selector %s value : lower from %d to %d\n", select_label, cur_val, val);
    for (i = cur_val; i > val; i=i-step) {
      send_pda_cmd(KEY_PDA_DOWN);
    }
  } else if (val > cur_val) {
    LOG(PDA_LOG,LOG_DEBUG, "Numeric selector %s value : raise from %d to %d\n", select_label, cur_val, val);
    for (i = cur_val; i < val; i=i+step) {
      send_pda_cmd(KEY_PDA_UP);
    }
  } else {
    LOG(PDA_LOG,LOG_DEBUG, "Numeric selector %s value : already at %d\n", select_label, val);
  }

  send_pda_cmd(KEY_PDA_SELECT);
  LOG(PDA_LOG,LOG_DEBUG, "Numeric selector %s value : set to %d\n", select_label, val);
  
  return true;
}

//bool set_PDA_aqualink_SWG_setpoint(struct aqualinkdata *aqdata, int val) {
void *set_PDA_aqualink_SWG_setpoint(void *ptr) {
  
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_SWG_PERCENT);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  goto_pda_home_first(aqdata); // if enabled

   val = setpoint_check(SWG_SETPOINT, val, aqdata);

  if (! goto_pda_menu(aqdata, PM_AQUAPURE)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding SWG setpoints menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  /*
  // wait for menu to display to capture current value with process_pda_packet_msg_long_SWG
  waitForPDAMessageTypes(aqdata,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,3,0);
  
  // On an PDA-ONLY panel we get the above BEFORE the actual menu is displayed, so wait a bit more, 
  waitForPDAMessages(aqdata, 2);
  */


/*
Fucking PDA, can't be that same for all version.
PDA Line 0 =   SET AquaPure
PDA Line 1 =
PDA Line 2 =
PDA Line 3 =    SET TO 100%
PDA Line 4 =
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Use ARROW KEYS
PDA Line 8 = to set value.
PDA Line 9 = Then SELECT.

PDA Line 0 =   SET AquaPure
PDA Line 1 =
PDA Line 2 =
PDA Line 3 =    SET TO 100%
PDA Line 4 =
PDA Line 5 =
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = Use Arrow Keys
PDA Line 9 = to set value.

PDA Line 0 =   Set AQUAPURE  
PDA Line 1 = 
PDA Line 2 = 
PDA Line 3 = Set Pool to: 35%
PDA Line 4 =                 
PDA Line 5 = 
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = Use Arrow Keys
PDA Line 9 = to set value.

PDA Line 0 =   SET AquaPure
PDA Line 1 =
PDA Line 2 =
PDA Line 3 = SET POOL TO: 45%
PDA Line 4 =  SET SPA TO:  0%
PDA Line 5 =
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = Highlight an
PDA Line 9 = item and press
*/

  // At the menus above if we just get CMD_PDA_HIGHLIGHT then we need to press enter to set the %
  // if it's CMD_PDA_HIGHLIGHT then CMD_PDA_HIGHLIGHTCHARS, we can simply set the %

  // At this point the Aquapure menu should be showing, if the last message was CMD_PDA_HIGHLIGHTCHARS then we
  // only need to change value, so check and wait a few more messages so see it.  if not we assume we need to select
  // a device to change ie Pool or Spa.
  bool selected=false;
  //waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHT,3,0);
  //if (aqdata->last_packet_type != CMD_PDA_HIGHLIGHTCHARS) { // not needed, waitForPDAMessageType will return if last message was CMD_PDA_HIGHLIGHTCHARS
    if ( waitForPDAMessageType(aqdata,CMD_PDA_HIGHLIGHTCHARS, 1, 0)) {
      selected = true;
    }
  //}

if (selected) {
  LOG(PDA_LOG,LOG_DEBUG, "SWG %% already selected\n");
  set_PDA_numeric_field_value(aqdata, val, -1, NULL, 5); // Null = line already selected
} else {
  LOG(PDA_LOG,LOG_DEBUG, "Looking for SWG device (pool/spa) to set\n");
 if (pda_find_m_index_loose("SET TO") > 0) {
  set_PDA_numeric_field_value(aqdata, val, -1, "SET TO", 5);
 } else if (aqdata->aqbuttons[SPA_INDEX].led->state != OFF) {
  set_PDA_numeric_field_value(aqdata, val, -1, "SET SPA", 5);
 } else {
  // Dual Setpoint Screen with SPA mode disabled
  set_PDA_numeric_field_value(aqdata, val, -1, "SET POOL", 5);
 }
}
    

/*
  if (pda_find_m_index("SET POOL") < 0) {
    // Single Setpoint Screen
     set_PDA_numeric_field_value(aqdata, val, -1, NULL, 5);
  } else*/ /*if (aqdata->aqbuttons[SPA_INDEX].led->state != OFF) {
    // Dual Setpoint Screen with SPA mode enabled
    // :TODO: aqdata should have 2 swg_precent values and GUI should be updated to
    //   display and modify both values.
     set_PDA_numeric_field_value(aqdata, val, -1, "SET SPA", 5);
  } else {
    // Dual Setpoint Screen with SPA mode disabled
     set_PDA_numeric_field_value(aqdata, val, -1, "SET POOL", 5);
  }*/
  // NSF Need to cater for "SET TO" message on some PDA versions
  
  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_boost(struct aqualinkdata *aqdata, bool val)
void *set_PDA_aqualink_boost(void *ptr)
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_BOOST);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  goto_pda_home_first(aqdata); // if enabled
  if (! goto_pda_menu(aqdata, PM_BOOST)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding BOOST menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  // Should be on the START menu item
  if (val == true) { // Turn on should just be enter
    select_pda_menu_item_loose(aqdata, "START", false);
  } else {
    // PDA Line 0 =      BOOST
    // PDA Line 1 =
    // PDA Line 2 =
    // PDA Line 3 =  TIME REMAINING
    // PDA Line 4 =      23:59
    // PDA Line 5 =
    // PDA Line 6 =
    // PDA Line 7 =       PAUSE
    // PDA Line 8 =      RESTART
    // PDA Line 9 =       STOP

    select_pda_menu_item_loose(aqdata, "STOP", false);
  }

  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}



bool set_PDA_aqualink_heater_setpoint(struct aqualinkdata *aqdata, int val, bool isPool) {
  char label[10];
  int cur_val;

  if ( isCOMBO_PANEL ) {
    if (isPool) {
      sprintf(label, "POOL HEAT");
      cur_val = aqdata->pool_htr_set_point;
    } else {
      sprintf(label, "SPA HEAT");
      cur_val = aqdata->spa_htr_set_point;
    }
  } else {
    if (isPool) {
      sprintf(label, "TEMP1");
      cur_val = aqdata->pool_htr_set_point;
    } else {
      sprintf(label, "TEMP2");
      cur_val = aqdata->spa_htr_set_point;
    }
  }

  if (val == cur_val) {
    LOG(PDA_LOG,LOG_INFO, "PDA %s setpoint : temp already %d\n", label, val);
    send_pda_cmd(KEY_PDA_BACK);
    return true;
  } 

  if (! goto_pda_menu(aqdata, PM_SET_TEMP)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding heater setpoints menu\n");
    return false;
  }

  set_PDA_numeric_field_value(aqdata, val, cur_val, label, 1);

  return true;
}

void *set_aqualink_PDA_pool_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //char *name;
  //char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_POOL_HEATER_TEMPS);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  goto_pda_home_first(aqdata); // if enabled
  val = setpoint_check(POOL_HTR_SETPOINT, val, aqdata);

  set_PDA_aqualink_heater_setpoint(aqdata, val, true);

  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}
void *set_aqualink_PDA_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //char *name;
  //char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_SPA_HEATER_TEMPS);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  goto_pda_home_first(aqdata); // if enabled
  val = setpoint_check(SPA_HTR_SETPOINT, val, aqdata);

  set_PDA_aqualink_heater_setpoint(aqdata, val, false);
  
  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_freezeprotect_setpoint(struct aqualinkdata *aqdata, int val) {
void *set_aqualink_PDA_freeze_protectsetpoint( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_FREEZE_PROTECT_TEMP);
 
#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif


  goto_pda_home_first(aqdata); // if enabled
  val = setpoint_check(FREEZE_SETPOINT, val, aqdata);
  
  if (_PDA_Type != PDA) {
    LOG(PDA_LOG,LOG_INFO, "In PDA AquaPalm mode, freezepoints not supported\n");
    //return false;
  } else if (! goto_pda_menu(aqdata, PM_FREEZE_PROTECT)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding freeze protect setpoints menu\n");
    //return false;
  } else if (! set_PDA_numeric_field_value(aqdata, val, aqdata->frz_protect_set_point, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set freeze protect temp value\n");
    //return false;
  } else {
    waitForPDAnextMenu(aqdata);
  }

  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_time(struct aqualinkdata *aqdata) 
void *set_PDA_aqualink_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_TIME);

  goto_pda_home_first(aqdata); // if enabled
  if (! goto_pda_menu(aqdata, PM_SET_TIME)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding set time menu\n");
    goto f_end;
  }
  struct tm tm;
  struct tm panel_tm;
  time_t now;
  char result[30];

  time(&now);   // get time now
  localtime_r(&now, &tm);
  LOG(PDA_LOG,LOG_DEBUG, "set_PDA_aqualink_time to %s", asctime_r(&tm,result));
/*  
Debug:   PDA:       PDA Menu Line 0 =     Set Time    
Debug:   PDA:       PDA Menu Line 1 = 
Debug:   PDA:       PDA Menu Line 2 =   01/18/11 Tue  
Debug:   PDA:       PDA Menu Line 3 =      2:51 PM    
Debug:   PDA:       PDA Menu Line 4 = 
Debug:   PDA:       PDA Menu Line 5 = 
Debug:   PDA:       PDA Menu Line 6 = Use Arrow Keys
Debug:   PDA:       PDA Menu Line 7 = to set value.
Debug:   PDA:       PDA Menu Line 8 = Press SELECT
Debug:   PDA:       PDA Menu Line 9 = to continue.
*/

  if (strptime(pda_m_line(2), "%t%D %a", &panel_tm) == NULL) {
    LOG(PDA_LOG,LOG_ERR, "set_PDA_aqualink_time read date (%.*s) failed\n",
        AQ_MSGLEN, pda_m_line(2));
    goto f_end;
  }
  if (strptime(pda_m_line(3), "%t%I:%M %p", &panel_tm) == NULL) {
    LOG(PDA_LOG,LOG_ERR, "set_PDA_aqualink_time read time (%.*s) failed\n",
        AQ_MSGLEN, pda_m_line(3));
    goto f_end;
  }
  panel_tm.tm_isdst = tm.tm_isdst;
  panel_tm.tm_sec = 0;

  LOG(PDA_LOG,LOG_DEBUG, "set_PDA_aqualink_time panel time %s",
      asctime_r(&panel_tm,result));

  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x02|0x03|0x01|0x8c|0x10|0x03|
  if (! set_PDA_numeric_field_value(aqdata, tm.tm_mon, panel_tm.tm_mon, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set month\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x05|0x06|0x01|0x92|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aqdata, tm.tm_mday, panel_tm.tm_mday, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set day\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x08|0x09|0x01|0x98|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aqdata, tm.tm_year, panel_tm.tm_year, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set year\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x04|0x05|0x01|0x91|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aqdata, tm.tm_hour, panel_tm.tm_hour, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set hour\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x07|0x08|0x01|0x97|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aqdata, tm.tm_min, panel_tm.tm_min, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set min\n");
  }

  waitForPDAnextMenu(aqdata);
  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);

  f_end:
  
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

// Test ine this.
//bool get_PDA_aqualink_aux_labels(struct aqualinkdata *aqdata) {
void *get_PDA_aqualink_aux_labels( void *ptr ) {

  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
#ifdef BETA_PDA_AUTOLABEL 
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
 
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_AUX_LABELS);

  int i=0;
  char label[10];

  LOG(PDA_LOG,LOG_INFO, "Finding PDA labels, (BETA ONLY)\n");

  goto_pda_home_first(aqdata); // if enabled
  if (! goto_pda_menu(aqdata, PM_AUX_LABEL)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding aux label menu\n");
    goto f_end;
  }

  for (i=1;i<8;i++) {
    sprintf(label, "AUX%d",i);
    select_pda_menu_item(aqdata, label, true);
    send_pda_cmd(KEY_PDA_BACK);
    waitForPDAnextMenu(aqdata);
  }

  // Read first page of devices and make some assumptions.

  waitfor_pda_queue2empty();
  goto_pda_menu(aqdata, PM_HOME);
  
  f_end:
  
#else
  LOG(PDA_LOG,LOG_INFO, "Finding PDA labels, (NOT IMPLIMENTED)\n");
#endif
  
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

/*
bool waitForPDAMessage(struct aqualinkdata *aqdata, int numMessageReceived, unsigned char packettype)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessage %s %d\n",message,numMessageReceived);
  int i=0;
  pthread_mutex_init(&aqdata->active_thread.thread_mutex, NULL);
  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);
  char* msgS;
  char* ptr;
  
  if (message != NULL) {
    if (message[0] == '^')
      msgS = &message[1];
    else
      msgS = message;
  }
  
  while( ++i <= numMessageReceived)
  {
    if (message != NULL)
      LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aqdata->last_message);
    else
      LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d waiting for next message, received '%s'\n",i,numMessageReceived,aqdata->last_message);
    if (message != NULL) {
      ptr = stristr(aqdata->last_message, msgS);
      if (ptr != NULL) { // match
        LOG(PDA_LOG,LOG_DEBUG, "Programming mode: String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aqdata->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(PDA_LOG,LOG_DEBUG, "Programming mode: looking for '%s' received message '%s'\n",message,aqdata->last_message);
    pthread_cond_init(&aqdata->active_thread.thread_cond, NULL);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    //LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aqdata->last_message);
  }
  
  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_DEBUG, "Programming mode: did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    LOG(PDA_LOG,LOG_DEBUG, "Programming mode: found message '%s' in '%s'\n",message,aqdata->last_message);
  
  return true;
}
*/


/*
Link to two different menu's used in PDA
http://www.poolequipmentpriceslashers.com.au/wp-content/uploads/2012/11/Jandy-Aqualink-RS-PDA-Wireless-Pool-Controller_manual.pdf
https://www.jandy.com/-/media/zodiac/global/downloads/h/h0574200.pdf
*/

/*
  List of how menu's display
PDA Line 0 =
PDA Line 1 =     AquaPalm
PDA Line 2 =
PDA Line 3 = Firmware Version
PDA Line 4 =
PDA Line 5 =     REV MMM
PDA Line 6 =
PDA Line 7 =
PDA Line 8 =
PDA Line 9 =
PDA Line 0 = 
PDA Line 1 =     AquaPalm    
PDA Line 2 = 
PDA Line 3 = Firmware Version
PDA Line 4 = 
PDA Line 5 =      REV T      
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 
PDA Menu Line 0 = 
PDA Menu Line 1 =   PDA-P4 Only   
PDA Menu Line 2 = 
PDA Menu Line 3 = Firmware Version
PDA Menu Line 4 = 
PDA Menu Line 5 =      PDA: 7.1.0 
PDA Menu Line 6 = 
PDA Menu Line 7 = 
PDA Menu Line 8 = 
PDA Menu Line 9 = 
************** The above have different menu to below rev/version *********
***************** Think this is startup different rev *************
PDA Menu Line 0 =
PDA Menu Line 1 =  PDA-PS4 Combo
PDA Menu Line 2 =
PDA Menu Line 3 = Firmware Version
PDA Menu Line 4 =
PDA Menu Line 5 =   PPD: PDA 1.2
PDA Line 0 =
PDA Line 1 = AIR         POOL
PDA Line 2 =
PDA Line 3 =
PDA Line 4 = POOL MODE     ON
PDA Line 5 = POOL HEATER  OFF
PDA Line 6 = SPA MODE     OFF
PDA Line 7 = SPA HEATER   OFF
PDA Line 8 = MENU
PDA Line 9 = EQUIPMENT ON/OFF
PDA Line 0 =    MAIN MENU
PDA Line 1 =
PDA Line 2 = SET TEMP       >
PDA Line 3 = SET TIME       >
PDA Line 4 = SET AquaPure   >
PDA Line 5 = PALM OPTIONS   >
PDA Line 6 =
PDA Line 7 =    BOOST POOL
PDA Line 8 =
PDA Line 9 =
**************** OPTION 2 FOR THIS MENU ********************

PDA Line 0 =    MAIN MENU
PDA Line 1 = HELP           >
PDA Line 2 = PROGRAM        >
PDA Line 3 = SET TEMP       >
PDA Line 4 = SET TIME       >
PDA Line 5 = SET AquaPure   >
PDA Line 6 = PDA OPTIONS    >
PDA Line 7 = SYSTEM SETUP   >
PDA Line 8 =
PDA Line 9 =      BOOST


PDA Line 0 =      BOOST
PDA Line 1 =
PDA Line 2 =   Operate the
PDA Line 3 =     AquaPure
PDA Line 4 =   chlorinator
PDA Line 5 =     at 100%
PDA Line 6 =   for 24 hrs.
PDA Line 7 =
PDA Line 8 =      START
PDA Line 9 =     GO BACK


********** Guess at SYSTEM SETUP Menu  (not on Rev MMM or before)************
// PDA Line 0 =   SYSTEM SETUP
// PDA Line 1 = LABEL AUX      >
// PDA Line 2 = FREEZE PROTECT >
// PDA Line 3 = AIR TEMP       >
// PDA Line 4 = DEGREES C/F    >
// PDA Line 5 = TEMP CALIBRATE >
// PDA Line 6 = SOLAR PRIORITY >
// PDA Line 7 = PUMP LOCKOUT   >
// PDA Line 8 = ASSIGN JVAs    >
// PDA Line 9 =    ^^ MORE __
// PDA Line 5 = COLOR LIGHTS   >
// PDA Line 6 = SPA SWITCH     >
// PDA Line 7 = SERVICE INFO   >
// PDA Line 8 = CLEAR MEMORY   >
PDA Line 0 =   PALM OPTIONS
PDA Line 1 =
PDA Line 2 =
PDA Line 3 = SET AUTO-OFF   >
PDA Line 4 = BACKLIGHT      >
PDA Line 5 = ASSIGN HOTKEYS >
PDA Line 6 =
PDA Line 7 = Choose setting
PDA Line 8 = and press SELECT
PDA Line 9 =
PDA Line 0 =   SET AquaPure
PDA Line 1 =
PDA Line 2 =
PDA Line 3 = SET POOL TO: 45%
PDA Line 4 =  SET SPA TO:  0%
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT
PDA Line 0 =     SET TIME
PDA Line 1 =
PDA Line 2 =   05/22/19 WED
PDA Line 3 =     10:53 AM
PDA Line 4 =
PDA Line 5 =
PDA Line 6 = Use ARROW KEYS
PDA Line 7 = to set value.
PDA Line 8 = Press SELECT
PDA Line 9 = to continue.
PDA Line 0 =     SET TEMP
PDA Line 1 =
PDA Line 2 = POOL HEAT   70`F
PDA Line 3 = SPA HEAT    98`F
PDA Line 4 =
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT
******* GUSSING AT BELOW *******
when single mode (pool OR spa) not (pool AND spa) temps are different.
PDA Line 0 =     SET TEMP
PDA Line 1 =
PDA Line 2 = TEMP1       70`F
PDA Line 3 = TEMP2       98`F
PDA Line 4 =
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT
PDA Line 0 =    EQUIPMENT
PDA Line 1 = FILTER PUMP   ON
PDA Line 2 = SPA          OFF
PDA Line 3 = POOL HEAT    OFF
PDA Line 4 = SPA HEAT     OFF
PDA Line 5 = CLEANER       ON
PDA Line 6 = WATERFALL    OFF
PDA Line 7 = AIR BLOWER   OFF
PDA Line 8 = LIGHT        OFF
PDA Line 9 =    ^^ MORE __
PDA Line 0 =    EQUIPMENT
PDA Line 1 = WATERFALL    OFF
PDA Line 2 = AIR BLOWER   OFF
PDA Line 3 = LIGHT        OFF
PDA Line 4 = AUX5         OFF
PDA Line 5 = EXTRA AUX    OFF
PDA Line 6 = SPA MODE     OFF
PDA Line 7 = CLEAN MODE   OFF
PDA Line 8 = ALL OFF
PDA Line 9 =
// This is from a single device setup (pool OR spa not pool AND spa)
PDA Menu Line 0 =    EQUIPMENT    
PDA Menu Line 1 = 
PDA Menu Line 2 = FILTER PUMP   ON
PDA Menu Line 3 = TEMP1        OFF
PDA Menu Line 4 = TEMP2        OFF
PDA Menu Line 5 = AUX1         OFF
PDA Menu Line 6 = Pool Light    ON
PDA Menu Line 7 = AUX3         OFF
PDA Menu Line 8 = EXTRA AUX    OFF
PDA Menu Line 9 = ALL OFF       
PDA Line 0 = Equipment Status
PDA Line 1 = 
PDA Line 2 = Intelliflo VS 1 
PDA Line 3 =      RPM: 1700  
PDA Line 4 =     Watts: 367  
PDA Line 5 = 
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 
PDA Line 0 = Equipment Status
PDA Line 1 = 
PDA Line 2 =   AquaPure 20%  
PDA Line 3 =  Salt 4000 PPM  
PDA Line 4 = 
PDA Line 5 = 
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 
VSP Motes.
four types of variable speed pumps, 
Jandy ePumpTM DC, 
Jandy ePumpTM AC,
IntelliFlo® 1 VF,
IntelliFlo® VS.
The SCALE setting is fixed to RPM for the Jandy ePumpTM DC, Jandy ePumpTM AC, and IntelliFlo® VS. 
The SCALE setting is fixed to GPM for the IntelliFlo® VF
There are eight (8) default speed presets for each variable speed pump. 
*/
