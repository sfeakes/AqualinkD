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
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "aq_programmer.h"
#include "onetouch.h"
#include "aqualink.h"
#include "rs_msg_utils.h"
#include "config.h"
#include "devices_jandy.h"

unsigned char _ot_pgm_command = NUL;

// External command
bool ot_queue_cmd(unsigned char cmd) {

  if (_ot_pgm_command == NUL) {
    _ot_pgm_command = cmd;
    return true;
  }

  return false;
}

unsigned char pop_ot_cmd(unsigned char receive_type)
{
  unsigned char cmd = NUL;

  if (receive_type == CMD_STATUS) {
    cmd = _ot_pgm_command;
    _ot_pgm_command = NUL;
  } 

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch Sending '0x%02hhx' to controller\n", cmd);
  return cmd;
}

void waitfor_ot_queue2empty()
{
  int i=0;

  while ( (_ot_pgm_command != NUL) && ( i++ < PROGRAMMING_POLL_COUNTER) ) {
    delay(PROGRAMMING_POLL_DELAY_TIME);
  }

  if (_ot_pgm_command != NUL) {
      // Wait for longer interval
      while ( (_ot_pgm_command != NUL) && ( i++ < PROGRAMMING_POLL_COUNTER * 2) ) {
        delay(PROGRAMMING_POLL_COUNTER * 2);
      }
  }

  if (_ot_pgm_command != NUL) {
    LOG(ONET_LOG,LOG_WARNING, "OneTouch Send command Queue did not empty, timeout\n");
  }
}

// This is for internal use only.
// Will only work in programming mode
void send_ot_cmd(unsigned char cmd)
{
  waitfor_ot_queue2empty();
  
  ot_queue_cmd(cmd);

  LOG(ONET_LOG,LOG_INFO, "OneTouch Queue send '0x%02hhx' to controller (programming)\n", _ot_pgm_command);
}

bool waitForOT_MessageTypes(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived)
{
  //LOG(ONET_LOG,LOG_DEBUG, "waitForOT_MessageType  0x%02hhx || 0x%02hhx\n",mtype1,mtype2);

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(ONET_LOG,LOG_DEBUG, "waitForOT_MessageType 0x%02hhx||0x%02hhx, last message type was 0x%02hhx (%d of %d)\n",mtype1,mtype2,*last_onetouch_packet(),i,numMessageReceived);

    if (*last_onetouch_packet() == mtype1 || *last_onetouch_packet() == mtype2) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (*last_onetouch_packet() != mtype1 && *last_onetouch_packet() != mtype2) {
    //LOG(ONET_LOG,LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    LOG(ONET_LOG,LOG_DEBUG, "waitForOT_MessageType: did not receive 0x%02hhx||0x%02hhx\n",mtype1,mtype2);
    return false;
  } else 
    LOG(ONET_LOG,LOG_DEBUG, "waitForOT_MessageType: received 0x%02hhx\n",*last_onetouch_packet());
  
  return true;
}

bool waitForNextOT_Menu(struct aqualinkdata *aq_data) {
  //waitForOT_MessageTypes(aq_data,CMD_PDA_CLEAR,CMD_PDA_0x04,10);
  //return waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);

  int i=0;
  const int numMessageReceived = 20;

  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= 20)
  {
    LOG(ONET_LOG,LOG_DEBUG, "waitForNextOT_Menu (%d of %d)\n",i,numMessageReceived);

    //if(thread_kick_type() == KICKT_MENU) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    if(thread_kick_type() == KICKT_MENU) break;
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);

  if(thread_kick_type() == KICKT_MENU)
    return true;
  else
    return false;
}

bool highlight_onetouch_menu_item(struct aqualinkdata *aq_data, char *item)
{
  int i;
  int index;
  int cnt;
  // Should probably to an UP as well as DOWN here.
  // Also need page up and down  "   ^^ More vv"
  if ( (index = onetouch_menu_find_index(item)) != -1) {
    cnt = index - onetouch_menu_hlightindex();
    LOG(ONET_LOG,LOG_DEBUG, "OneTouch menu caculator selected=%d, wanted=%d, move=%d times\n",onetouch_menu_hlightindex(),index,cnt);
    for (i=0; i < cnt; i ++) {
        send_ot_cmd(KEY_ONET_DOWN);
        waitfor_ot_queue2empty();
        waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,3);
        if (rsm_strcmp(onetouch_menu_hlight(), item) == 0) {
          // We got here early, probably because blank menu item
          break;
        }
    }
    // check if it's quicker to go up rather than down, if second page can't go up.
    // This doesn;t work yet, not all versions of control panels have the same amount of items.
    /*
    if (cnt <= 5 || rsm_strcmp(onetouch_menu_line(10), "   ^^ More", 10) == 0 ) { 
      LOG(ONET_LOG,LOG_DEBUG, "OneTouch device programmer pressing down %d times\n",cnt);
      for (i=0; i < cnt; i ++) {
        send_ot_cmd(KEY_ONET_DOWN);
        //waitfor_ot_queue2empty();
        //waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
      }
    } else {
      cnt = 11 - cnt;
      LOG(ONET_LOG,LOG_DEBUG, "OneTouch device programmer pressing up %d times\n",cnt);
      for (i=0; i < cnt; i ++) {
        send_ot_cmd(KEY_ONET_UP);
        //waitfor_ot_queue2empty();
        //waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
      }
    }*/
    // Not much quicker doing it this way that in the for loops above, may have to change back.
    //waitfor_ot_queue2empty();
    //waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
  } else {
    // Is their another page to search
    if (rsm_strcmp(onetouch_menu_line(10), "^^ More") == 0) {
      char first_item[AQ_MSGLEN+1];
      do {
        send_ot_cmd(KEY_ONET_PAGE_DN);
        waitForNextOT_Menu(aq_data);
        if (onetouch_menu_find_index(item) != -1) {
          return highlight_onetouch_menu_item(aq_data, item);
        }
      } while (strncpy(first_item, onetouch_menu_line(1), AQ_MSGLEN));
      // Need to get menu item 1.
      // Hit page down until menu item matches 1.
    } else
      LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer menu item '%s' does not exist\n",item);
    //print_onetouch_menu();
  }

  if ( rsm_strcmp(onetouch_menu_hlight(), item) != 0) {
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer menu item '%s' not selected\n",item);
    //print_onetouch_menu();
    return false;
  } else {
    return true;
  }
}

bool select_onetouch_menu_item(struct aqualinkdata *aq_data, char *item)
{
  if (highlight_onetouch_menu_item(aq_data, item)) {
    send_ot_cmd(KEY_ONET_SELECT);
    waitForNextOT_Menu(aq_data);
    return true;
  }
  return false;
}


bool goto_onetouch_system_menu(struct aqualinkdata *aq_data)
{
  int i=0;

  if (get_onetouch_menu_type() == OTM_SYSTEM)
    return true;

  // Get back to a known point, the system menu
  while (get_onetouch_menu_type() != OTM_SYSTEM && get_onetouch_menu_type() != OTM_ONETOUCH && i < 5 ) {
    send_ot_cmd(KEY_ONET_BACK);
    waitfor_ot_queue2empty();
    waitForNextOT_Menu(aq_data);
    i++;
  }

  if (get_onetouch_menu_type() == OTM_SYSTEM) {
    //printf("*** SYSTEM MENU ***\n");
    //return false;
  } else if (get_onetouch_menu_type() == OTM_ONETOUCH) {
    //printf("*** ONE TOUCH MENU ***\n");
    // Can only be one of 2 options in this menu, so if it's not the one we want, hit down first
    if ( rsm_strcmp(onetouch_menu_hlight(), "System") != 0) {
      send_ot_cmd(KEY_ONET_DOWN);
      waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
    }
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();
    waitForNextOT_Menu(aq_data);
    //return false;
  } else {
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer couldn't get to System menu\n");
    return false;
  }

  if (get_onetouch_menu_type() != OTM_SYSTEM) {
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer couldn't get to System menu\n");
    return false;
  }

  
  LOG(ONET_LOG,LOG_DEBUG, "Got to System menu\n");

  return true;
}

bool goto_onetouch_menu(struct aqualinkdata *aq_data, ot_menu_type menu)
{
  bool equErr = false;
  char *second_menu = false;
  char *third_menu = false;

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch device programmer request for menu %d\n",menu);

  if ( ! goto_onetouch_system_menu(aq_data) ) {
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get system menu\n");
    return false;
  }

  // Now we are at main menu, we should have 3 options
  /* Debug:  OneTouch Menu Line 9 = Equipment ON/OFF
     Debug:  OneTouch Menu Line 10 = OneTouch  ON/OFF
     Debug:  OneTouch Menu Line 11 =    Menu / Help 
  */

  // First setup any secondary menu's to hit.
  switch(menu){
    case OTM_SET_TEMP:
      second_menu = "Set Temp";
    break;
    case OTM_SET_TIME:
      second_menu = "Set Time";
    break;
    case OTM_SET_AQUAPURE:
      second_menu = "Set AQUAPURE";
    break;
    case OTM_FREEZE_PROTECT:
      second_menu = "System Setup";
      third_menu = "Freeze Protect";
    break;
    case OTM_SYSTEM_SETUP:
     second_menu = "System Setup";
    break;
     case OTM_BOOST:
      second_menu = "Boost";
    break;
    default:
    break;
  }

  // Now actually get the menu
  switch(menu){
    case OTM_SYSTEM:
      // We are already here
      return true;
    break;
    case OTM_EQUIPTMENT_ONOFF:
      if ( select_onetouch_menu_item(aq_data, "Equipment ON/OFF") == false ) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer couldn't select 'Equipment ON/OFF' menu %d\n",menu);
        equErr = true;
      }
    break;
    case OTM_MENUHELP:
    case OTM_SET_TEMP:
    case OTM_SET_TIME:
    case OTM_SET_AQUAPURE:
    case OTM_FREEZE_PROTECT:
    case OTM_BOOST:
    case OTM_SYSTEM_SETUP:
      if ( select_onetouch_menu_item(aq_data, "Menu / Help") == false ) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer couldn't select menu %d\n",menu);
        break;
      }
      if (second_menu)
        select_onetouch_menu_item(aq_data, second_menu);
      if (third_menu)
        select_onetouch_menu_item(aq_data, third_menu);
    break;
    default:
      LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer doesn't know how to access menu %d\n",menu);
    break;
  }

  // We can't detect Equiptment menu yet, so use the find test above not get_onetouch_menu_type() below
  if (menu == OTM_EQUIPTMENT_ONOFF ) {
    return !equErr;
  }

  // Need to wait a bit longer for set temp menu if single device (nag screen temp1 higher than temp2)
  if ( menu == OTM_SET_TEMP && get_onetouch_menu_type() != menu) {
    waitForNextOT_Menu(aq_data);
  }

  if (get_onetouch_menu_type() != menu)
    return false;

  return true;
}


// Return the digit at factor
// num=12 factor=10 would return 2
// num=12 factor=100 would return 1
int digit(int num, int factor)
{
  return ( (num % factor) - (num % (factor/10)) ) / (factor/10) ;
}

// REturn the differance at a particular digit.
// new=120, old=130, factor=100 return  2-3 = -1
int digitDiff(int new, int old, int factor)
{
   return ( digit(old, factor) - digit(new, factor) );
}

bool intPress(int diff) {
  int i;
  unsigned char key = KEY_ONET_UP;

  if (diff < 0) {
    diff = 0 - diff;
    key = KEY_ONET_DOWN;
    //printf ("**** Pressing down %d times\n",diff);
  } else {
    //printf ("**** Pressing UP %d times\n",diff);
  }

  for (i=0; i < diff; i++) {
    send_ot_cmd(key);
    waitfor_ot_queue2empty();
  }

  return true;
}

/*
  PROGRAMMING FUNCTIONS
*/

void *set_aqualink_onetouch_pump_rpm( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  char *buf = (char*)threadCtrl->thread_args;
  char VSPstr[20];
  int i, structIndex;

  //printf("**** program string '%s'\n",buf);
  
  int pumpIndex = atoi(&buf[0]);
  int pumpRPM = -1;
  //int pumpRPM = atoi(&buf[2]);
  for (structIndex=0; structIndex < aq_data->num_pumps; structIndex++) {
    if (aq_data->pumps[structIndex].pumpIndex == pumpIndex) {
      if (aq_data->pumps[structIndex].pumpType == PT_UNKNOWN) {
        LOG(ONET_LOG,LOG_ERR, "Can't set Pump RPM/GPM until type is known\n");
        cleanAndTerminateThread(threadCtrl);
        return ptr;
      }
      pumpRPM = RPM_check(aq_data->pumps[structIndex].pumpType, atoi(&buf[2]), aq_data);
      break;
    }
  }
  // NSF Should probably check pumpRPM is not -1 here

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_PUMP_RPM);

  LOG(ONET_LOG,LOG_INFO, "OneTouch Set Pump %d to RPM %d\n",pumpIndex,pumpRPM);


  if (! goto_onetouch_menu(aq_data, OTM_EQUIPTMENT_ONOFF) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer didn't get Equiptment on/off menu\n");
  }

  sprintf(VSPstr, "VSP%1d Spd Adj",pumpIndex);
/*  
  if ( (index = onetouch_menu_find_index(VSPstr)) != -1) {
    int cnt = index - onetouch_menu_hlightindex();
    for (i=0; i < cnt; i ++) {
      send_ot_cmd(KEY_ONET_DOWN);
      waitfor_ot_queue2empty();
    }
    send_ot_cmd(KEY_ONET_SELECT);
    waitForNextOT_Menu(aq_data);
*/
  if ( select_onetouch_menu_item(aq_data, VSPstr) ) {
    if ( get_onetouch_menu_type() == OTM_SELECT_SPEED) {
      // Now fine menu item with X as last digit, and select that menu.
      //Pool           X
      for (i=0; i < 12; i++) {
        if ( onetouch_menu_hlight()[15] != 'X' ) {
          send_ot_cmd(KEY_ONET_DOWN);
          waitfor_ot_queue2empty();
          waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
        } else {
          send_ot_cmd(KEY_ONET_SELECT);
          waitfor_ot_queue2empty();
          waitForNextOT_Menu(aq_data);
          break;
        }
      }
      //OneTouch Menu Line 3 =  set to  50 GPM 
      //OneTouch Menu Line 3 =  set to  50 GPM
      //OneTouch Menu Line 3 = set to 1750 RPM
      //if ( strstr(onetouch_menu_hlight(), "set to") != NULL ) {
      if ( rsm_strstr(onetouch_menu_hlight(), "set to") != NULL ) {
        //printf("FOUND MENU")
        //if (strstr(onetouch_menu_hlight(), "RPM") != NULL ) {
        if (rsm_strstr(onetouch_menu_hlight(), "RPM") != NULL ) {
          // RPM 3450 & 600 max & min
          // Panel will change 2nd,3rd & 4th digits depending on previos digit
          // so reget the RPM after every change.
          int RPM = rsm_atoi(&onetouch_menu_hlight()[7]);
          intPress(digitDiff(RPM, pumpRPM, 10000));
          send_ot_cmd(KEY_ONET_SELECT);
          waitfor_ot_queue2empty();
          RPM = rsm_atoi(&onetouch_menu_hlight()[7]);
          intPress(digitDiff(RPM, pumpRPM, 1000));
          send_ot_cmd(KEY_ONET_SELECT);
          waitfor_ot_queue2empty();
          RPM = rsm_atoi(&onetouch_menu_hlight()[7]);
          intPress(digitDiff(RPM, pumpRPM, 100));
          send_ot_cmd(KEY_ONET_SELECT);
          waitfor_ot_queue2empty();
          RPM = rsm_atoi(&onetouch_menu_hlight()[7]);
          intPress(digitDiff(RPM, pumpRPM, 10));
          // Get the new RPM.
          aq_data->pumps[structIndex].rpm = rsm_atoi(&onetouch_menu_hlight()[7]);
          send_ot_cmd(KEY_ONET_SELECT);
          waitfor_ot_queue2empty();
          waitForOT_MessageTypes(aq_data,CMD_MSG_LONG,CMD_PDA_HIGHLIGHTCHARS,5);
        //} else if (strstr(onetouch_menu_hlight(), "GPM") != NULL ) {
        } else if (rsm_strstr(onetouch_menu_hlight(), "GPM") != NULL ) {
          // GPM 130 max, GPM 15 min
          for (i=0; i < 24 ; i++) { // Max of 23 key presses to get from max to min
            int GPM = rsm_atoi(&onetouch_menu_hlight()[8]);
            //printf ("*** GPM = %d | Setting to %d\n",GPM,pumpRPM);
            if ( GPM > pumpRPM ) {
              send_ot_cmd(KEY_ONET_DOWN);
            } else if (GPM < pumpRPM) {
              send_ot_cmd(KEY_ONET_UP);
            } else {
              aq_data->pumps[structIndex].gpm = rsm_atoi(&onetouch_menu_hlight()[8]);;
              send_ot_cmd(KEY_ONET_SELECT);
              waitfor_ot_queue2empty();
              break;
            }
            waitfor_ot_queue2empty();
            // This really does slow it down, but we hit up.down once too ofter without it, need to fix.
            //waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,5);
            waitForOT_MessageTypes(aq_data,CMD_MSG_LONG,CMD_PDA_HIGHLIGHTCHARS,5);
            // Reset the pump GPM
            //aq_data->pumps[structIndex].rpm = GPM;
            //waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_MSG_LONG,5);
            //waitForNextOT_Menu(aq_data);
          }
        } else {
          LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer Not sure how to set '%s'\n",onetouch_menu_hlight());
        }
      } else {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer didn't select VSP\n");
      }
    } else {
      LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer Couldn't find Select Speed menu\n");
    }
  } else {
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer Couldn't find VSP in Equiptment on/off menu\n");
  }
  //printf( "Menu Index %d\n", onetouch_menu_find_index(VSPstr));

  //printf("**** GOT THIS FAR, NOW LET'S GO BACK ****\n");

  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_WARNING, "OneTouch device programmer didn't get back to System menu\n");
  }

  //printf("**** CLEAN EXIT ****\n");

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_onetouch_freezeprotect( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_FREEZEPROTECT);

  LOG(ONET_LOG,LOG_ERR, "***** OneTouch set freeze protect not implimented *****\n");

  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_onetouch_macro( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  //struct aqualinkdata *aq_data = threadCtrl->aq_data;

  //sprintf(msg, "%-5d%-5d",index, (strcmp(value, "on") == 0)?ON:OFF);
  // Use above to set
  char *buf = (char*)threadCtrl->thread_args;
  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_MACRO);
  
  LOG(ONET_LOG,LOG_DEBUG, "OneTouch Marco\n");

  LOG(ONET_LOG,LOG_ERR, "OneTouch Macro not implimented (device=%d|state=%d)\n",device,state);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_aqualink_onetouch_freezeprotect( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_ONETOUCH_FREEZEPROTECT);

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch get Freezeprotect\n");

  if ( !goto_onetouch_menu(aq_data, OTM_SET_TEMP) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get heater temp menu\n");
  }

  if ( !goto_onetouch_menu(aq_data, OTM_FREEZE_PROTECT) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get freeze protect menu\n");
  }

  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer didn't get back to System menu\n");
  }

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
/*
  This will get all setpoints, including freeze protect, above just gets freeze protect.
*/
void *get_aqualink_onetouch_setpoints( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_ONETOUCH_SETPOINTS);
  
  LOG(ONET_LOG,LOG_DEBUG, "OneTouch get heater temps\n");

  if ( !goto_onetouch_menu(aq_data, OTM_SET_TEMP) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get heater temp menu\n");
  }

  if ( !goto_onetouch_menu(aq_data, OTM_FREEZE_PROTECT) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get freeze protect menu\n");
  }

  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer didn't get back to System menu\n");
  }

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void set_aqualink_onetouch_heater_setpoint( struct aqualinkdata *aq_data, bool ispool, int val )
{
  int cval;
  int diff;
  int i;
  int len;
  //char *st;
  unsigned char direction;

  if ( !goto_onetouch_menu(aq_data, OTM_SET_TEMP) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get heater temp menu\n");
  }

  if(ispool){
    if (isCOMBO_PANEL) {
      if (!highlight_onetouch_menu_item(aq_data, "Pool Heat")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get pool heater temp menu\n");
        return;
      }
    } else {
      if (!highlight_onetouch_menu_item(aq_data, "Temp1")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get Temp1 temp menu\n");
        return;
      }
    }
  } else {
    if (isCOMBO_PANEL) {
      if (!highlight_onetouch_menu_item(aq_data, "Spa Heat")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get spa heater temp menu\n");
        return;
      }
    } else {
      if (!highlight_onetouch_menu_item(aq_data, "Temp2")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get Temp2 temp menu\n");
        return;
      }
    }
  }

  send_ot_cmd(KEY_ONET_SELECT);
  waitfor_ot_queue2empty();
  waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.
 
  {
    char *st = onetouch_menu_hlightchars(&len);
    LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp highlighted='%.*s'\n", len, st);
  }
  
  cval = atoi(onetouch_menu_hlightchars(&len));
  diff = val - cval;
  if (diff > 0) {
    direction = KEY_ONET_UP;
  } else if (diff < 0) {
    direction = KEY_ONET_DOWN;
    diff=-diff;
  }

  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp diff='%d'\n", diff);

  for (i=0; i < diff; i++) {
    send_ot_cmd(direction);
    waitfor_ot_queue2empty();
  }

  send_ot_cmd(KEY_ONET_SELECT);
  waitfor_ot_queue2empty();
  send_ot_cmd(KEY_ONET_BACK);
  waitfor_ot_queue2empty();
  
/*
  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp line='%s'\n", onetouch_menu_line(line));
  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp highlight='%d'\n", onetouch_menu_hlightindex());
  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp highlightline='%s'\n", onetouch_menu_line(onetouch_menu_hlightindex()));
  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set heater temp highlightchars='%s'\n", onetouch_menu_hlight());
*/
  //onetouch_menu_line(line)
  //onetouch_menu_hlightindex
  //onetouch_menu_hlight
}

void *set_aqualink_onetouch_pool_heater_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_POOL_HEATER_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(POOL_HTR_SETOINT, val, aq_data);

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch set pool heater temp to %d\n", val);
  set_aqualink_onetouch_heater_setpoint(aq_data, true, val);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_onetouch_spa_heater_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_SPA_HEATER_TEMP);
  
  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SPA_HTR_SETOINT, val, aq_data);

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch set spa heater temp to %d\n", val);
  set_aqualink_onetouch_heater_setpoint(aq_data, false, val);

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_onetouch_boost( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_BOOST);
  
  int val = atoi((char*)threadCtrl->thread_args);

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch request set Boost to '%d'\n",val==true?"On":"Off");

  if ( !goto_onetouch_menu(aq_data, OTM_BOOST) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get BOOST menu\n");
  } else { 
    if ( rsm_strcmp(onetouch_menu_hlight(), "Start") == 0 ) {
      if (val) {
        LOG(ONET_LOG,LOG_DEBUG, "OneTouch Boost is Off, turning On\n");
        send_ot_cmd(KEY_ONET_SELECT);
        waitfor_ot_queue2empty();
      } else {
        LOG(ONET_LOG,LOG_INFO, "OneTouch Boost is Off, ignore request\n");
      }
    } else if ( rsm_strcmp(onetouch_menu_hlight(), "Pause") == 0 ) {
      if (! val) {
        LOG(ONET_LOG,LOG_DEBUG, "OneTouch set Boost is ON, turning Off\n");
        highlight_onetouch_menu_item(aq_data, "Stop");
        send_ot_cmd(KEY_ONET_SELECT);
        waitfor_ot_queue2empty();
        // Takes ages to see bost is off from menu, to set it here.
        setSWGboost(aq_data, false);
      } else {
        LOG(ONET_LOG,LOG_INFO, "OneTouch Boost is On, ignore request\n");
      }
    } else {
      LOG(ONET_LOG,LOG_ERR, "OneTouch Boost unknown menu\n");
    } 
  }

  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_WARNING, "OneTouch device programmer didn't get back to System menu\n");
  }

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_onetouch_swg_percent( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  int cval;
  int diff;
  int i;
  int len;
  unsigned char direction = KEY_ONET_UP;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_SWG_PERCENT);
  int val = atoi((char*)threadCtrl->thread_args);
  val = setpoint_check(SWG_SETPOINT, val, aq_data);

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch set SWG Percent to %d\n",val);

  if ( !goto_onetouch_menu(aq_data, OTM_SET_AQUAPURE) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get Aquapure menu\n");
    goto f_end;
  }
    
  //OneTouch Menu Line 3 = Set Pool to: 50%
  //OneTouch Menu Line 4 =  Set Spa to:  0%

  // Only have option for both on ComboPanel, and us
  if (isCOMBO_PANEL) {
    if (aq_data->aqbuttons[SPA_INDEX].led->state == OFF) {
      if (!highlight_onetouch_menu_item(aq_data, "Set Pool")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get Pool swg percent\n");
        goto f_end;
      }
    } else {
      if (!highlight_onetouch_menu_item(aq_data, "Set Spa")) {
        LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get Spa swg percent\n");
        goto f_end;
      }
    }
  } else {
    // NSF Need to check text for congle panel.
  }

  send_ot_cmd(KEY_ONET_SELECT);
  waitfor_ot_queue2empty();
  waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.
 
  {
    i=0;
    char *st = onetouch_menu_hlightchars(&len);
    LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set SWG Percent highlighted='%.*s'  len=%d  st=%s\n", len, st, len, st);
    while (len > 5 || (len < 0 && i < 5)) {
      LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set SWG Percent highlighted waiting again\n");
      delay(50);
      waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,5); // CMD_PDA_0x04 is just a packer.
      st = onetouch_menu_hlightchars(&len);
      LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set SWG Percent highlighted='%.*s'  len=%d  st=%s\n", len, st, len, st);
      i++;
    }
  }
  
  cval = atoi(onetouch_menu_hlightchars(&len));
  diff = (val - cval) / 5;
  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set SWG Percent val=%d cval=%d diff=%d \n", val,cval,diff);
  if (diff > 0) {
    direction = KEY_ONET_UP;
  } else if (diff < 0) {
    direction = KEY_ONET_DOWN;
    diff=-diff;
  }

  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set SWG Percent diff='%d'\n", diff);

  for (i=0; i < diff; i++) {
    send_ot_cmd(direction);
    waitfor_ot_queue2empty();
  }
  
  /*
  waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.
  setSWGpercent(aq_data,atoi(onetouch_menu_hlightchars(&len)));
  */
 
  send_ot_cmd(KEY_ONET_SELECT);
  waitfor_ot_queue2empty();
  send_ot_cmd(KEY_ONET_BACK);
  waitfor_ot_queue2empty();

  f_end:
  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_WARNING, "OneTouch device programmer didn't get back to System menu\n");
  }

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}



bool set_numeric_value(struct aqualinkdata *aq_data, int val) {
  int len;
  int cval;
  int diff;
  int i;
  unsigned char direction = KEY_ONET_UP;

  cval = atoi(onetouch_menu_hlightchars(&len));
  diff = val - cval;

  LOG(ONET_LOG,LOG_DEBUG, "** OneTouch set value val=%d cval=%d diff=%d \n", val,cval,diff);

  if (diff > 0) {
    direction = KEY_ONET_UP;
  } else if (diff < 0) {
    direction = KEY_ONET_DOWN;
    diff=-diff;
  }

  if (diff > 0) {
    for (i=0; i < diff; i++) {
      send_ot_cmd(direction);
      waitfor_ot_queue2empty();
    }

    waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,5); // CMD_PDA_0x04 is just a packer.
    cval = atoi(onetouch_menu_hlightchars(&len));
    if ( val != cval ) {
      LOG(ONET_LOG,LOG_ERR, "** OneTouch set value failed, val=%d cval=%d\n", val,cval);
      return false;
    }
  }

  return true;
}

void *set_aqualink_onetouch_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_ONETOUCH_TIME);
  
  LOG(ONET_LOG,LOG_ERR, "OneTouch set time not implimented\n");

  LOG(ONET_LOG,LOG_DEBUG, "OneTouch set time\n");

  if ( !goto_onetouch_menu(aq_data, OTM_SET_TIME) ){
    LOG(ONET_LOG,LOG_ERR, "OneTouch device programmer failed to get time menu\n");
  } else {
    
    // MM/DD/YY   MON   Just change MM/DD/YY
    //  H:MM  AM        Have to cycle H to get AM/PM, just one digit
    time_t now = time(0);   // get time now
    struct tm *result = localtime(&now);
    int hour;
    char ap;

    if (result->tm_hour == 0) { //12 AM ie midnight
      hour = 12;
      ap = 'A';
    } else if (result->tm_hour == 12) {// 12 PM
      hour = 12;
      ap = 'P';
    } else if (result->tm_hour <= 11) {
      hour = result->tm_hour;
      ap = 'A';
    } else {// Must be 13 or more
      hour = result->tm_hour - 12;
      ap = 'P';
    }

    LOG(ONET_LOG,LOG_DEBUG, "OneTouch set time to :-\n");
    LOG(ONET_LOG,LOG_DEBUG, " %d/%d/%d\n",(result->tm_mon + 1), result->tm_mday, result->tm_year % 100 );
    LOG(ONET_LOG,LOG_DEBUG, " %d:%d %c\n",hour,result->tm_min, ap);

    //Line 3 startchar 2 for Month
    //Line 3 startchar 5 for Day
    //Line 3 startchat 8 for year

    while ( (onetouch_menu_hlightindex() != 3) || (onetouch_menu_hlightcharindex() != 2) )
      waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.

    printf("*** Setting month.  line=%d, char=%d\n",onetouch_menu_hlightindex(), onetouch_menu_hlightcharindex());
    set_numeric_value(aq_data, (result->tm_mon + 1) );
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();
    
    while ( (onetouch_menu_hlightindex() != 3) || (onetouch_menu_hlightcharindex() != 5) )
      waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.

    printf("*** Setting day.  line=%d, char=%d\n",onetouch_menu_hlightindex(), onetouch_menu_hlightcharindex());
    set_numeric_value(aq_data, result->tm_mday );
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();

    while ( (onetouch_menu_hlightindex() != 3) || (onetouch_menu_hlightcharindex() != 8) )
      waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.
    printf("*** Setting year.  line=%d, char=%d\n",onetouch_menu_hlightindex(), onetouch_menu_hlightcharindex());
    set_numeric_value(aq_data, result->tm_year % 100 );
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();

    // highlightline 4 char 3 or 4 for Hour
    // highlightline 4 char 5 or 6 for Min
    // highlightline 4 char 9
    waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.

    // Need to check AM/PM here
    set_numeric_value(aq_data, hour );
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();
    waitForOT_MessageTypes(aq_data,CMD_PDA_HIGHLIGHTCHARS,0x00,15); // CMD_PDA_0x04 is just a packer.

    set_numeric_value(aq_data, result->tm_min );
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();

    
    send_ot_cmd(KEY_ONET_SELECT);
    waitfor_ot_queue2empty();
    
    send_ot_cmd(KEY_ONET_BACK);
    waitfor_ot_queue2empty();
  }

  if (! goto_onetouch_menu(aq_data, OTM_SYSTEM) ){
    LOG(ONET_LOG,LOG_WARNING, "OneTouch device programmer didn't get back to System menu\n");
  }

  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

