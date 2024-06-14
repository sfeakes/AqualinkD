
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


#ifdef AQ_DEBUG
  #include "timespec_subtract.h"
#endif

bool waitForPDAMessageHighlight(struct aqualinkdata *aq_data, int highlighIndex, int numMessageReceived);
bool waitForPDAMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived);
bool waitForPDAMessageTypes(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived);
bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived, char *text, int line);
bool goto_pda_menu(struct aqualinkdata *aq_data, pda_menu_type menu);
bool wait_pda_selected_item(struct aqualinkdata *aq_data);
bool waitForPDAnextMenu(struct aqualinkdata *aq_data);
bool loopover_devices(struct aqualinkdata *aq_data);
bool find_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, int charlimit);
bool select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu);

bool _get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aq_data); 
bool _get_PDA_freeze_protect_temp(struct aqualinkdata *aq_data);

static pda_type _PDA_Type;

//#define USE_ALLBUTTON_QUEUE

#ifndef USE_ALLBUTTON_QUEUE
/* Forcing all these to allbutton for the moment */
void waitfor_queue2empty();
void send_cmd(unsigned char cmd);
unsigned char pop_allb_cmd(struct aqualinkdata *aq_data);
int get_allb_queue_length();

void waitfor_pda_queue2empty() {
  waitfor_queue2empty();
}
void send_pda_cmd(unsigned char cmd) {
  send_cmd(cmd);
}
unsigned char pop_pda_cmd(struct aqualinkdata *aq_data){
  return pop_allb_cmd(aq_data);
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
    push_pda_cmd(cmd);
  }
}

unsigned char pop_pda_cmd(struct aqualinkdata *aq_data)
{
  unsigned char cmd = NUL;

  if (_pda_command != NUL && aq_data->last_packet_type == CMD_STATUS) {
    cmd = _pda_command;
    _pda_command = NUL;
  }
/*
  // Only send commands on status messages 
  if (get_pda_queue_length() > 0 && aq_data->last_packet_type == CMD_STATUS) {
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
void pda_programming_thread_check(struct aqualinkdata *aq_data)
{
  static pthread_t thread_id = 0;
  static int ack_count = 0;
  #ifdef AQ_DEBUG
    static struct timespec start;
    static struct timespec now;
    struct timespec elapsed;
  #endif
  // Check for long lasting threads
  if (aq_data->active_thread.thread_id != 0) {
    if (thread_id != *aq_data->active_thread.thread_id) {
       printf ("**************** LAST POINTER SET %ld , %p ****************************\n",thread_id,&thread_id);
     
      thread_id = *aq_data->active_thread.thread_id;
      #ifdef AQ_DEBUG
         clock_gettime(CLOCK_REALTIME, &start);
      #endif
      printf ("**************** NEW POINTER SET %d, %ld %ld , %p %p ****************************\n",aq_data->active_thread.ptype,thread_id,*aq_data->active_thread.thread_id,&thread_id,aq_data->active_thread.thread_id);
      ack_count = 0;
    } else if (ack_count > MAX_ACK_FOR_THREAD) {
      #ifdef AQ_DEBUG
       clock_gettime(CLOCK_REALTIME, &now);
       timespec_subtract(&elapsed, &now, &start);
       LOG(PDA_LOG,LOG_ERR, "Thread %d,%p FAILED to finished in reasonable time, %d.%03ld sec, killing it.\n",
             aq_data->active_thread.ptype,
             aq_data->active_thread.thread_id,
             elapsed.tv_sec, elapsed.tv_nsec / 1000000L);
      #else
        LOG(PDA_LOG,LOG_ERR, "Thread %d,%p FAILED to finished in reasonable time, killing it!\n", aq_data->active_thread.ptype, aq_data->active_thread.thread_id)
      #endif
      if (pthread_cancel(*aq_data->active_thread.thread_id) != 0)
          LOG(PDA_LOG,LOG_ERR, "Thread kill failed\n");
      else {
       
      }
      aq_data->active_thread.thread_id = 0;
      aq_data->active_thread.ptype = AQP_NULL;
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

bool wait_pda_selected_item(struct aqualinkdata *aq_data)
{
  int i=0;

  while (pda_m_hlightindex() == -1 && i < 5){
    waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,10);
    i++;
  }

  if (pda_m_hlightindex() == -1)
    return false;
  else
   return true;
}

bool waitForPDAnextMenu(struct aqualinkdata *aq_data) {
  if (!waitForPDAMessageType(aq_data,CMD_PDA_CLEAR,10))
    return false;
  return waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
}

bool loopover_devices(struct aqualinkdata *aq_data) {
  int i;
  int index = -1;

  if (! goto_pda_menu(aq_data, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "loopover_devices :- can't goto PM_EQUIPTMENT_CONTROL menu\n");
    //cleanAndTerminateThread(threadCtrl);
    return false;
  }
  
  // Should look for message "ALL OFF", that's end of device list.
  for (i=0; i < 18 && (index = pda_find_m_index("ALL OFF")) == -1 ; i++) {
    send_pda_cmd(KEY_PDA_DOWN);
    //waitForMessage(aq_data, NULL, 1);
    waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_MSG_LONG,8);
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
bool find_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, int charlimit) {
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
        //delay(500);
        //wait_for_empty_cmd_buffer();
        //waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,2);
        waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_MSG_LONG,8);
        //waitForMessage(aq_data, NULL, 1);
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
      if (strncasecmp(pda_m_line(9),"     BOOST      ", 16) == 0) {
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
  return waitForPDAMessageHighlight(aq_data, bLookingForBoost?9:index, 10);
}

bool _select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu, bool loose);

bool select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu){
  return _select_pda_menu_item(aq_data, menuText, waitForNextMenu, false);
}
bool select_pda_menu_item_loose(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu){
  return _select_pda_menu_item(aq_data, menuText, waitForNextMenu, true);
}
bool _select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu, bool loose) {

  //int matchType = loose?-1:0; // NSF release 2.1.0 was this and it worked.  Need to re-check why I did this.
  //int matchType = loose?-1:1;
  int matchType = loose?-1:strlen(menuText); // NSF Not way to check this. (release 2.2.0 introduced this with the line above)
  if ( find_pda_menu_item(aq_data, menuText, matchType) ) {
    send_pda_cmd(KEY_PDA_SELECT);

    LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer selected menu item '%s'\n",menuText);
    if (waitForNextMenu)
      return waitForPDAnextMenu(aq_data);

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

bool goto_pda_menu(struct aqualinkdata *aq_data, pda_menu_type menu) {
  bool ret = true;
  int cnt = 0;

  LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer request for menu %d\n",menu);

  if (pda_m_type() == PM_FW_VERSION) {
      LOG(PDA_LOG,LOG_DEBUG, "goto_pda_menu at FW version menu\n");
      send_pda_cmd(KEY_PDA_BACK);
      if (! waitForPDAnextMenu(aq_data)) {
          LOG(PDA_LOG,LOG_ERR, "PDA Device programmer wait for next menu failed\n");
      }
  } else if (pda_m_type() == PM_BUILDING_HOME) {
      LOG(PDA_LOG,LOG_DEBUG, "goto_pda_menu building home menu\n");
      waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,15);
  }
  

  while (ret && (pda_m_type() != menu) && cnt <= 5) {
    switch (menu) {
      case PM_HOME:
         send_pda_cmd(KEY_PDA_BACK);
         ret = waitForPDAnextMenu(aq_data);
         break;
      case PM_EQUIPTMENT_CONTROL:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "EQUIPMENT ON/OFF", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        break;
      case PM_PALM_OPTIONS:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aq_data, "PALM OPTIONS", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        break;
      case PM_AUX_LABEL:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aq_data, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aq_data, "SYSTEM SETUP", true);
            } else if (pda_m_type() == PM_SYSTEM_SETUP) {
                ret = select_pda_menu_item(aq_data, "LABEL AUX", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aq_data);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / LABEL AUX menu\n");
        }
        break;
      case PM_SYSTEM_SETUP:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aq_data, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aq_data, "SYSTEM SETUP", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aq_data);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP menu\n");
        }
        break;
      case PM_FREEZE_PROTECT:
        if ( _PDA_Type == PDA) {
            if (pda_m_type() == PM_HOME) {
                ret = select_pda_menu_item(aq_data, "MENU", true);
            } else if (pda_m_type() == PM_MAIN) {
                ret = select_pda_menu_item(aq_data, "SYSTEM SETUP", true);
            } else if (pda_m_type() == PM_SYSTEM_SETUP) {
                ret = select_pda_menu_item(aq_data, "FREEZE PROTECT", true);
            } else {
                send_pda_cmd(KEY_PDA_BACK);
                ret = waitForPDAnextMenu(aq_data);
            }
        } else {
          LOG(PDA_LOG,LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / FREEZE PROTECT menu\n");
        }
        break;
      case PM_AQUAPURE:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aq_data, "SET AquaPure", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        break;
      case PM_BOOST:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item_loose(aq_data, "BOOST", true);
            //ret = select_pda_menu_item(aq_data, "BOOST", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        //printf("****MENU SELECT RETURN %d*****\n",ret);
        break;
      case PM_SET_TEMP:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
          if (isCOMBO_PANEL) {
            ret = select_pda_menu_item(aq_data, "SET TEMP", true);
          } else {
              // Depending on control panel config, may get an extra menu asking to press any key
              LOG(PDA_LOG,LOG_DEBUG, "PDA in single device mode, \n");
              ret = select_pda_menu_item(aq_data, "SET TEMP", false);
              // We could press enter here, but I can't test it, so just wait for message to dissapear.
              ret = waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,25);    
              //waitForPDAMessageType(aq_data,CMD_PDA_CLEAR,10);
              //waitForPDAMessageTypesOrMenu(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,20,"press ANY key",8);
            }
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        break;
      case PM_SET_TIME:
        if (pda_m_type() == PM_HOME) {
            ret = select_pda_menu_item(aq_data, "MENU", true);
        } else if (pda_m_type() == PM_MAIN) {
            ret = select_pda_menu_item(aq_data, "SET TIME", true);
        } else {
            send_pda_cmd(KEY_PDA_BACK);
            ret = waitForPDAnextMenu(aq_data);
        }
        break;
      default:
        LOG(PDA_LOG,LOG_ERR, "PDA Device programmer didn't understand requested menu\n");
        return false;
        break;
    }
    LOG(PDA_LOG,LOG_DEBUG, "PDA Device programmer request for menu %d, current %d\n", menu, pda_m_type());
    cnt++;
  }
  if (pda_m_type() != menu) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device programmer didn't find a requested menu %d, current %d\n", menu, pda_m_type());
    return false;
  }

  return true;
}

void *set_aqualink_PDA_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //int i=0;
  //int found;
  char device_name[15];
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_ON_OFF);
  
  char *buf = (char*)threadCtrl->thread_args;
  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  if (device > aq_data->total_buttons) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off :- bad device number '%d'\n",device);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",aq_data->aqbuttons[device].label,state);

  if (! goto_pda_menu(aq_data, PM_EQUIPTMENT_CONTROL)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off :- can't find EQUIPTMENT CONTROL menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  // If single config (Spa OR pool) rather than (Spa AND pool) heater is TEMP1 and TEMP2
  if (isSINGLE_DEV_PANEL && device == aq_data->pool_heater_index) { // rename Heater and Spa
    sprintf(device_name,"%-13s\n","TEMP1");
  } else if (isSINGLE_DEV_PANEL && device == aq_data->spa_heater_index)  {// rename Heater and Spa
    sprintf(device_name,"%-13s\n","TEMP2");
  } else {
    //Pad name with spaces so something like "SPA" doesn't match "SPA BLOWER"
    sprintf(device_name,"%-13s\n",aq_data->aqbuttons[device].label);
  }

  // NSF Added this since DEBUG hitting wrong command
  //waitfor_pda_queue2empty();

  if ( find_pda_menu_item(aq_data, device_name, 13) ) {
    if (aq_data->aqbuttons[device].led->state != state) {
      //printf("*** Select State ***\n");
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', changing state\n",aq_data->aqbuttons[device].label,state);
      force_queue_delete(); // NSF This is a bad thing to do.  Need to fix this
      send_pda_cmd(KEY_PDA_SELECT);
      while (get_pda_queue_length() > 0) { delay(500); }
      // If you are turning on a heater there will be a sub menu to set temp
      if ((state == ON) && ((device == aq_data->pool_heater_index) || (device == aq_data->spa_heater_index))) {
          if (! waitForPDAnextMenu(aq_data)) {
            LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - waitForPDAnextMenu\n",
                       aq_data->aqbuttons[device].label);
          } else {
              send_pda_cmd(KEY_PDA_SELECT);
	      while (get_pda_queue_length() > 0) { delay(500); }
              if (!waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,20)) {
                  LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - wait for CMD_PDA_HIGHLIGHT\n",
                             aq_data->aqbuttons[device].label);
              }
          }
      } else { // not turning on heater wait for line update
          // worst case spa when pool is running
          if (!waitForPDAMessageType(aq_data,CMD_MSG_LONG,2)) {
              LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off: %s on - wait for CMD_MSG_LONG\n",
                         aq_data->aqbuttons[device].label);
          }
      }
      
    } else {
      LOG(PDA_LOG,LOG_INFO, "PDA Device On/Off, found device '%s', not changing state, is same\n",aq_data->aqbuttons[device].label,state);
    }
  } else {
    LOG(PDA_LOG,LOG_ERR, "PDA Device On/Off, device '%s' not found\n",aq_data->aqbuttons[device].label);
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;

}



void *get_aqualink_PDA_device_status( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //int i;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_STATUS);
  
  goto_pda_menu(aq_data, PM_HOME);

  if (! loopover_devices(aq_data)) {
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
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
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
    char *ptr1 = pda_m_line(1);
    char *ptr2 = pda_m_line(5);
    ptr1[AQ_MSGLEN+1] = '\0';
    ptr2[AQ_MSGLEN+1] = '\0';
    //strcpy(aq_data->version, stripwhitespace(ptr));
    snprintf(aq_data->version, (AQ_MSGLEN*2)-1, "%s %s",stripwhitespace(ptr1),stripwhitespace(ptr2));

    //printf("****** Version '%s' ********\n",aq_data->version);
    LOG(PDA_LOG,LOG_DEBUG, "PDA type=%d, version=%s\n", _PDA_Type, aq_data->version);
    // don't wait for version menu to time out press back to get to home menu faster
    //send_pda_cmd(KEY_PDA_BACK);
    //if (! waitForPDAnextMenu(aq_data)) { // waitForPDAnextMenu waits for highlight chars, which we don't get on normal menu
    
    if (! waitForPDAMessageType(aq_data,CMD_PDA_CLEAR,10)) {
        LOG(PDA_LOG,LOG_ERR, "PDA Init :- wait for next menu failed\n");
    }
  }
  else {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- should be called when on FW VERSION menu.\n");
  }
/* 
  // Get status of all devices
  if (! loopover_devices(aq_data)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- can't find menu\n");
  }
*/
  // Get heater setpoints
  if (! _get_PDA_aqualink_pool_spa_heater_temps(aq_data)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- Error getting heater setpoints\n");
  }

  // Get freeze protect setpoint, AquaPalm doesn't have freeze protect in menu.
  if (_PDA_Type != AQUAPALM && ! _get_PDA_freeze_protect_temp(aq_data)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- Error getting freeze setpoints\n");
  }

  pda_reset_sleep();

  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_aqualink_PDA_wakeinit( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //int i=0;

  // At this point, we should probably just exit if there is a thread already going as 
  // it means the wake was called due to changing a device.
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_WAKE_INIT);

  LOG(PDA_LOG,LOG_DEBUG, "PDA Wake Init\n");

  // Get status of all devices
  if (! loopover_devices(aq_data)) {
    LOG(PDA_LOG,LOG_ERR, "PDA Init :- can't find menu\n");
  }

  cleanAndTerminateThread(threadCtrl);
  
  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


bool _get_PDA_freeze_protect_temp(struct aqualinkdata *aq_data) {
  
  if ( _PDA_Type == PDA) {
    if (! goto_pda_menu(aq_data, PM_FREEZE_PROTECT)) {   
      return false;
    }
    /* select the freeze protect temp to see which devices are enabled by freeze
       protect */
    send_pda_cmd(KEY_PDA_SELECT);
    return waitForPDAnextMenu(aq_data);
  } else {
    LOG(PDA_LOG,LOG_INFO, "In PDA AquaPalm mode, freezepoints not supported\n");
    return false;
  }
}

bool _get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aq_data) {
  
   // Get heater setpoints
  if (! goto_pda_menu(aq_data, PM_SET_TEMP)) {
    LOG(PDA_LOG,LOG_ERR, "Could not get heater setpoints, trying again!\n");
    // Going to try this twice.
    if (! goto_pda_menu(aq_data, PM_SET_TEMP)) {
      return false;
    }
  }
  
  return true;
}

void *get_PDA_aqualink_pool_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_POOL_SPA_HEATER_TEMPS);
  _get_PDA_aqualink_pool_spa_heater_temps(aq_data);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

void *get_PDA_freeze_protect_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_FREEZE_PROTECT_TEMP);
  _get_PDA_freeze_protect_temp(aq_data);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

bool waitForPDAMessageHighlight(struct aqualinkdata *aq_data, int highlighIndex, int numMessageReceived)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight index %d\n",highlighIndex);

  if(pda_m_hlightindex() == highlighIndex) return true;

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight last = 0x%02hhx : index %d : (%d of %d)\n",aq_data->last_packet_type,pda_m_hlightindex(),i,numMessageReceived);

    if (aq_data->last_packet_type == CMD_PDA_HIGHLIGHT && pda_m_hlightindex() == highlighIndex) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (pda_m_hlightindex() != highlighIndex) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight: did not receive index '%d'\n",highlighIndex);
    return false;
  } else 
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageHighlight: received index '%d'\n",highlighIndex);
  
  return true;
}


bool _waitForPDAMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived, bool forceNext)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageType  0x%02hhx\n",mtype);

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  if (forceNext) { // Ignore current message type, and wait for next
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  } 

  while( ++i <= numMessageReceived)
  {
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageType 0x%02hhx, last message type was 0x%02hhx (%d of %d)\n",mtype,aq_data->last_packet_type,i,numMessageReceived);
    
    if (aq_data->last_packet_type == mtype) break;
    
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (aq_data->last_packet_type != mtype) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageType: did not receive 0x%02hhx\n",mtype);
    return false;
  } else 
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageType: received 0x%02hhx\n",mtype);
  
  return true;
}

bool waitForPDAMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived){
  return _waitForPDAMessageType(aq_data, mtype, numMessageReceived, false);
}
bool waitForPDANextMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived){
  return _waitForPDAMessageType(aq_data, mtype, numMessageReceived, true);
}




// Wait for Message, hit return on particular menu.
bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived, char *text, int line)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypes  0x%02hhx or 0x%02hhx\n",mtype1,mtype2);

  int i=0;
  bool gotmenu = false;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    if (gotmenu == false && line > 0 && text != NULL) {
      if (stristr(pda_m_line(line), text) != NULL) {
        send_pda_cmd(KEY_PDA_SELECT);
        gotmenu = true;
        LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypesOrMenu saw '%s' and line %d\n",text,line);
      }
    }
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypes 0x%02hhx | 0x%02hhx, last message type was 0x%02hhx (%d of %d)\n",mtype1,mtype2,aq_data->last_packet_type,i,numMessageReceived);

    if (aq_data->last_packet_type == mtype1 || aq_data->last_packet_type == mtype2) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (aq_data->last_packet_type != mtype1 && aq_data->last_packet_type != mtype2) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_ERR, "waitForPDAMessageTypes: did not receive 0x%02hhx or 0x%02hhx\n",mtype1,mtype2);
    return false;
  } else 
    LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessageTypes: received 0x%02hhx\n",aq_data->last_packet_type);
  
  return true;
}

bool waitForPDAMessageTypes(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived)
{
  return waitForPDAMessageTypesOrMenu(aq_data, mtype1, mtype2, numMessageReceived, NULL, 0);
}

/*
  Use -1 for cur_val if you want this to find the current value and change it.
  Use number for cur_val to  increase / decrease from known start point
*/
bool set_PDA_numeric_field_value(struct aqualinkdata *aq_data, int val, int cur_val, char *select_label, int step) {
  int i=0;

  LOG(PDA_LOG,LOG_DEBUG, "set_PDA_numeric_field_value %s from %d to %d step %d\n", select_label, cur_val, val, step);
  if (select_label != NULL) {
    if ( ! select_pda_menu_item(aq_data, select_label, false) ) {
      return false;
    }
  }

  if (cur_val == -1) {
    char *hghlight_chars;
    int hlight_length=0;
    int i=0;
    hghlight_chars = pda_m_hlightchars(&hlight_length); // NSF May need to take this out and there for the LOG entry after while
    while (hlight_length >= 15 || hlight_length <= 0) {
      delay(500);
      waitForPDANextMessageType(aq_data,CMD_PDA_HIGHLIGHTCHARS,5);
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

//bool set_PDA_aqualink_SWG_setpoint(struct aqualinkdata *aq_data, int val) {
void *set_PDA_aqualink_SWG_setpoint(void *ptr) {
  
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_SWG_PERCENT);

  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SWG_SETPOINT, val, aq_data);

  if (! goto_pda_menu(aq_data, PM_AQUAPURE)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding SWG setpoints menu\n");
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }
  // wait for menu to display to capture current value with process_pda_packet_msg_long_SWG
  waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS, 10);
/*
  if (pda_find_m_index("SET POOL") < 0) {
    // Single Setpoint Screen
     set_PDA_numeric_field_value(aq_data, val, -1, NULL, 5);
  } else*/ if (aq_data->aqbuttons[SPA_INDEX].led->state != OFF) {
    // Dual Setpoint Screen with SPA mode enabled
    // :TODO: aq_data should have 2 swg_precent values and GUI should be updated to
    //   display and modify both values.
     set_PDA_numeric_field_value(aq_data, val, -1, "SET SPA", 5);
  } else {
    // Dual Setpoint Screen with SPA mode disabled
     set_PDA_numeric_field_value(aq_data, val, -1, "SET POOL", 5);
  }
  
  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_boost(struct aqualinkdata *aq_data, bool val)
void *set_PDA_aqualink_boost(void *ptr)
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_BOOST);

  int val = atoi((char*)threadCtrl->thread_args);

  if (! goto_pda_menu(aq_data, PM_BOOST)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding BOOST menu\n");
    return false;
  }
  // Should be on the START menu item
  if (val == true) { // Turn on should just be enter
    select_pda_menu_item_loose(aq_data, "START", false);
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

    select_pda_menu_item_loose(aq_data, "STOP", false);
  }

  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}



bool set_PDA_aqualink_heater_setpoint(struct aqualinkdata *aq_data, int val, bool isPool) {
  char label[10];
  int cur_val;

  if ( isCOMBO_PANEL ) {
    if (isPool) {
      sprintf(label, "POOL HEAT");
      cur_val = aq_data->pool_htr_set_point;
    } else {
      sprintf(label, "SPA HEAT");
      cur_val = aq_data->spa_htr_set_point;
    }
  } else {
    if (isPool) {
      sprintf(label, "TEMP1");
      cur_val = aq_data->pool_htr_set_point;
    } else {
      sprintf(label, "TEMP2");
      cur_val = aq_data->spa_htr_set_point;
    }
  }

  if (val == cur_val) {
    LOG(PDA_LOG,LOG_INFO, "PDA %s setpoint : temp already %d\n", label, val);
    send_pda_cmd(KEY_PDA_BACK);
    return true;
  } 

  if (! goto_pda_menu(aq_data, PM_SET_TEMP)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding heater setpoints menu\n");
    return false;
  }

  set_PDA_numeric_field_value(aq_data, val, cur_val, label, 1);

  return true;
}

void *set_aqualink_PDA_pool_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //char *name;
  //char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_POOL_HEATER_TEMPS);
  
  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(POOL_HTR_SETOINT, val, aq_data);

  set_PDA_aqualink_heater_setpoint(aq_data, val, true);

  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}
void *set_aqualink_PDA_spa_heater_temps( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //char *name;
  //char *menu_name;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_SPA_HEATER_TEMPS);
  
  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SPA_HTR_SETOINT, val, aq_data);

  set_PDA_aqualink_heater_setpoint(aq_data, val, false);
  
  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_freezeprotect_setpoint(struct aqualinkdata *aq_data, int val) {
void *set_aqualink_PDA_freeze_protectsetpoint( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_FREEZE_PROTECT_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);

  val = setpoint_check(FREEZE_SETPOINT, val, aq_data);
  
  if (_PDA_Type != PDA) {
    LOG(PDA_LOG,LOG_INFO, "In PDA AquaPalm mode, freezepoints not supported\n");
    //return false;
  } else if (! goto_pda_menu(aq_data, PM_FREEZE_PROTECT)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding freeze protect setpoints menu\n");
    //return false;
  } else if (! set_PDA_numeric_field_value(aq_data, val, aq_data->frz_protect_set_point, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set freeze protect temp value\n");
    //return false;
  } else {
    waitForPDAnextMenu(aq_data);
  }

  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

//bool set_PDA_aqualink_time(struct aqualinkdata *aq_data) 
void *set_PDA_aqualink_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_SET_TIME);

  if (! goto_pda_menu(aq_data, PM_SET_TIME)) {
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
  if (! set_PDA_numeric_field_value(aq_data, tm.tm_mon, panel_tm.tm_mon, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set month\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x05|0x06|0x01|0x92|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aq_data, tm.tm_mday, panel_tm.tm_mday, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set day\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x08|0x09|0x01|0x98|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aq_data, tm.tm_year, panel_tm.tm_year, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set year\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x04|0x05|0x01|0x91|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aq_data, tm.tm_hour, panel_tm.tm_hour, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set hour\n");
  // PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x07|0x08|0x01|0x97|0x10|0x03|
  } else if (! set_PDA_numeric_field_value(aq_data, tm.tm_min, panel_tm.tm_min, NULL, 1)) {
    LOG(PDA_LOG,LOG_ERR, "Error failed to set min\n");
  }

  waitForPDAnextMenu(aq_data);
  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);

  f_end:
  
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

// Test ine this.
//bool get_PDA_aqualink_aux_labels(struct aqualinkdata *aq_data) {
void *get_PDA_aqualink_aux_labels( void *ptr ) {

  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
#ifdef BETA_PDA_AUTOLABEL 
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
 
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_GET_AUX_LABELS);

  int i=0;
  char label[10];

  LOG(PDA_LOG,LOG_INFO, "Finding PDA labels, (BETA ONLY)\n");

  if (! goto_pda_menu(aq_data, PM_AUX_LABEL)) {
    LOG(PDA_LOG,LOG_ERR, "Error finding aux label menu\n");
    goto f_end;
  }

  for (i=1;i<8;i++) {
    sprintf(label, "AUX%d",i);
    select_pda_menu_item(aq_data, label, true);
    send_pda_cmd(KEY_PDA_BACK);
    waitForPDAnextMenu(aq_data);
  }

  // Read first page of devices and make some assumptions.

  waitfor_pda_queue2empty();
  goto_pda_menu(aq_data, PM_HOME);
  
  f_end:
  
#else
  LOG(PDA_LOG,LOG_INFO, "Finding PDA labels, (NOT IMPLIMENTED)\n");
#endif
  
  cleanAndTerminateThread(threadCtrl);
  return ptr;
}

/*
bool waitForPDAMessage(struct aqualinkdata *aq_data, int numMessageReceived, unsigned char packettype)
{
  LOG(PDA_LOG,LOG_DEBUG, "waitForPDAMessage %s %d\n",message,numMessageReceived);
  int i=0;
  pthread_mutex_init(&aq_data->active_thread.thread_mutex, NULL);
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);
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
      LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
    else
      LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d waiting for next message, received '%s'\n",i,numMessageReceived,aq_data->last_message);
    if (message != NULL) {
      ptr = stristr(aq_data->last_message, msgS);
      if (ptr != NULL) { // match
        LOG(PDA_LOG,LOG_DEBUG, "Programming mode: String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //LOG(PDA_LOG,LOG_DEBUG, "Programming mode: looking for '%s' received message '%s'\n",message,aq_data->last_message);
    pthread_cond_init(&aq_data->active_thread.thread_cond, NULL);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //LOG(PDA_LOG,LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //LOG(PDA_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(PDA_LOG,LOG_DEBUG, "Programming mode: did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    LOG(PDA_LOG,LOG_DEBUG, "Programming mode: found message '%s' in '%s'\n",message,aq_data->last_message);
  
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
