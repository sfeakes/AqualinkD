
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
#include <stdbool.h>

#include "iaqtouch_aq_programmer.h"
#include "aq_serial.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aqualink.h"
//#include "packetLogger.h"
#include "iaqtouch.h"
#include "rs_msg_utils.h"
#include "aq_panel.h"
#include "config.h"
#include "devices_jandy.h"
#include "packetLogger.h"
#include "color_lights.h"

// System Page is obfiously fixed and not dynamic loaded, so set buttons to stop confustion.

#define KEY_IAQTCH_SCHEDULE       KEY_IAQTCH_KEY01
#define KEY_IAQTCH_CUSTOMIZE_HOME KEY_IAQTCH_KEY02
#define KEY_IAQTCH_PROGRAM_GROUP  KEY_IAQTCH_KEY03
#define KEY_IAQTCH_SET_TEMP       KEY_IAQTCH_KEY04
#define KEY_IAQTCH_SYSTEM_SETUP   KEY_IAQTCH_KEY05
#define KEY_IAQTCH_TOUCH_SETUP    KEY_IAQTCH_KEY06
#define KEY_IAQTCH_SET_DATETIME   KEY_IAQTCH_KEY07
#define KEY_IAQTCH_LOCKOUT_PASSWD KEY_IAQTCH_KEY08
#define KEY_IAQTCH_SET_ACQUAPURE  KEY_IAQTCH_KEY09

bool _cansend = false;

unsigned char _iaqt_pgm_command = NUL;
static pthread_mutex_t _iaqt_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool _iaqt_control_announced = false;

/**************
 * 
 *  Command queue stuff
 * 
 */

// External command
bool iaqt_queue_cmd(unsigned char cmd) {
  pthread_mutex_lock(&_iaqt_queue_mutex);
  if (_iaqt_pgm_command == NUL) {
    _iaqt_pgm_command = cmd;
    pthread_mutex_unlock(&_iaqt_queue_mutex);
    return true;
  }
  pthread_mutex_unlock(&_iaqt_queue_mutex);
  return false;
}

void set_iaq_cansend(bool cansend){
  pthread_mutex_lock(&_iaqt_queue_mutex);
  _cansend = cansend;
  pthread_mutex_unlock(&_iaqt_queue_mutex);
}

unsigned char pop_iaqt_cmd(unsigned char receive_type)
{
  unsigned char cmd = NUL;

  pthread_mutex_lock(&_iaqt_queue_mutex);
  if (!_cansend) {
    pthread_mutex_unlock(&_iaqt_queue_mutex);
    return cmd;
  }

  if (receive_type == CMD_IAQ_POLL) {
    cmd = _iaqt_pgm_command;
    _iaqt_pgm_command = NUL;
    if (cmd == ACK_CMD_READY_CTRL)
      _iaqt_control_announced = true;
  } 
  pthread_mutex_unlock(&_iaqt_queue_mutex);

  if (cmd != NUL)
    LOG(IAQT_LOG,LOG_DEBUG, "Sending '0x%02hhx' to controller\n", cmd);
  return cmd;
}


static bool iaqt_command_pending(void)
{
  pthread_mutex_lock(&_iaqt_queue_mutex);
  bool pending = _iaqt_pgm_command != NUL;
  pthread_mutex_unlock(&_iaqt_queue_mutex);
  return pending;
}

static bool iaqt_can_send(void)
{
  pthread_mutex_lock(&_iaqt_queue_mutex);
  bool cansend = _cansend;
  pthread_mutex_unlock(&_iaqt_queue_mutex);
  return cansend;
}

void waitfor_iaqt_queue2empty()
{
  int i=0;

  while (iaqt_command_pending() && ( i++ < PROGRAMMING_POLL_COUNTER) ) {
    delay(PROGRAMMING_POLL_DELAY_TIME);
  }

  // Initial startup can take some time, _cansend should be false during this time.
  // If we start programming before we receive the first status page, nothing works, this forces that wait
  while(!iaqt_can_send()) {
    delay(PROGRAMMING_POLL_DELAY_TIME * 2);
  }

  if (iaqt_command_pending()) {
      // Wait for longer interval
      while (iaqt_command_pending() && ( i++ < PROGRAMMING_POLL_COUNTER * 2 ) ) {
        delay(PROGRAMMING_POLL_DELAY_TIME * 2);
      }
  }

  if (iaqt_command_pending()) {
    LOG(IAQT_LOG,LOG_WARNING, "Send command Queue did not empty, timeout\n");
  }
}

void send_aqt_cmd(unsigned char cmd)
{
  waitfor_iaqt_queue2empty();
  
  iaqt_queue_cmd(cmd);

  LOG(IAQT_LOG,LOG_DEBUG, "Queue send '0x%02hhx' to controller (programming)\n", cmd);
}

/**************
 * 
 *  Control Command queue stuff
 * 
 */

unsigned char _iaqt_control_cmd[AQ_MAXPKTLEN_SEND];
int _iaqt_control_cmd_len;


// Copy and remove under one lock: timeout cleanup must never modify a buffer
// while the serial thread is transmitting it. The caller owns the copy.
int pop_iaqt_control_cmd(unsigned char *cmd)
{
  pthread_mutex_lock(&_iaqt_queue_mutex);
  int len = _iaqt_control_announced ? _iaqt_control_cmd_len : 0;
  if (len > 0) {
    memcpy(cmd, _iaqt_control_cmd, len);
    _iaqt_control_cmd_len = 0;
    _iaqt_control_announced = false;
  }
  pthread_mutex_unlock(&_iaqt_queue_mutex);

  if (len > 0 && getLogLevel(IAQT_LOG) >= LOG_DEBUG) {
    char buff[1024];
    beautifyPacket(buff, 1024, cmd, len, false);
    LOG(IAQT_LOG,LOG_DEBUG, "Sending commandsed : %s\n", buff);
  }

  return len;
}

static bool iaqt_control_pending(void)
{
  pthread_mutex_lock(&_iaqt_queue_mutex);
  bool pending = _iaqt_control_cmd_len > 0;
  pthread_mutex_unlock(&_iaqt_queue_mutex);
  return pending;
}

bool waitfor_iaqt_ctrl_queue2empty()
{
  int i=0;

  while (iaqt_control_pending() && ( i++ < 100) ) {
    LOG(IAQT_LOG,LOG_DEBUG, "Waiting for commandset to send\n");
    delay(50);
  }

  LOG(IAQT_LOG,LOG_DEBUG, "Wait for commandset over!\n");

  pthread_mutex_lock(&_iaqt_queue_mutex);
  if (_iaqt_control_cmd_len > 0) {
    _iaqt_control_cmd_len = 0;
    _iaqt_control_announced = false;
    // Cancel an announcement not yet consumed by the serial thread as well.
    if (_iaqt_pgm_command == ACK_CMD_READY_CTRL)
      _iaqt_pgm_command = NUL;
    pthread_mutex_unlock(&_iaqt_queue_mutex);
    LOG(IAQT_LOG,LOG_WARNING, "Send control command timed out; discarded pending command, aborting operation\n");
    return false;
  }
  pthread_mutex_unlock(&_iaqt_queue_mutex);
  return true;
}
/*
unsigned const char _waitfor_iaqt_nextPage(struct aqualinkdata *aqdata, int numMessageReceived) {
  
  waitfor_iaqt_queue2empty();

  int i=0;

  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    //LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage (%d of %d)\n",i,numMessageReceived);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    if(wasiaqtThreadKickTypePage()) break;
  }

  LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage finished in (%d of %d)\n",i,numMessageReceived);

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  if(wasiaqtThreadKickTypePage())
    return iaqtCurrentPage();
  else
    return NUL;

}

unsigned const char shortwaitfor_iaqt_nextPage(struct aqualinkdata *aqdata) {
  return _waitfor_iaqt_nextPage(aqdata, 3);
}
*/
unsigned const char waitfor_iaqt_messages(struct aqualinkdata *aqdata, int numMessageReceived) {
  //return _waitfor_iaqt_nextPage(aqdata, 30);
  
  waitfor_iaqt_queue2empty();

  int i=0;

  LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_messages (%d of %d)\n",i,numMessageReceived);

  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    //LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage (%d of %d)\n",i,numMessageReceived);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_message (%d of %d) - received message 0x%02hhx\n",i,numMessageReceived,iaqtLastMsg());
  }

  LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_messages finished in (%d of %d)\n",i,numMessageReceived);

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  return iaqtLastMsg();
}

unsigned const char waitfor_iaqt_nextPage(struct aqualinkdata *aqdata) {
  //return _waitfor_iaqt_nextPage(aqdata, 30);
  
  waitfor_iaqt_queue2empty();

  int i=0;
  const int numMessageReceived = 30;

  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    //LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage (%d of %d)\n",i,numMessageReceived);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    if(wasiaqtThreadKickTypePage()) break;

    LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage (%d of %d) - received message 0x%02hhx\n",i,numMessageReceived,iaqtLastMsg());
  }

  LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextPage finished in (%d of %d)\n",i,numMessageReceived);

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  if(wasiaqtThreadKickTypePage())
    return iaqtCurrentPage();
  else
    return NUL;
}



unsigned const char waitfor_iaqt_nextMessage(struct aqualinkdata *aqdata, const unsigned char msg_type) {
  
  waitfor_iaqt_queue2empty();

  int i=0;
  const int numMessageReceived = 30;

  pthread_mutex_lock(&aqdata->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextMessage 0x%02hhx (%d of %d)\n",msg_type,i,numMessageReceived);
    pthread_cond_wait(&aqdata->active_thread.thread_cond, &aqdata->active_thread.thread_mutex);
    if( msg_type == NUL || iaqtLastMsg() == msg_type) break;

    LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextMessage (%d of %d) - received message 0x%02hhx\n",i,numMessageReceived,iaqtLastMsg());
  }

  pthread_mutex_unlock(&aqdata->active_thread.thread_mutex);

  LOG(IAQT_LOG,LOG_DEBUG, "waitfor_iaqt_nextMessage finished (%d of %d) - received message 0x%02hhx\n",i,numMessageReceived,iaqtLastMsg());
  return iaqtLastMsg();
}

//typedef enum iaqt_control_command_type()
typedef enum {icct_setrpm, icct_settime, icct_setdate} iaqtControlCmdYype;

// Type is always 0 at the moment, haven't found any 
bool queue_iaqt_control_command(iaqtControlCmdYype type, int num) {
  //unsigned char packets[AQ_MAXPKTLEN_SEND];
  //int cnt;

  if (waitfor_iaqt_ctrl_queue2empty() == false)
    return false;

  pthread_mutex_lock(&_iaqt_queue_mutex);
  _iaqt_control_announced = false;
  _iaqt_control_cmd[0] = DEV_MASTER;
  _iaqt_control_cmd[1] = 0x24;
  _iaqt_control_cmd[2] = 0x31;

  _iaqt_control_cmd_len = num2iaqtRSset(&_iaqt_control_cmd[3], num, true);

  // Pad with 0xcd for some reason.
  for(_iaqt_control_cmd_len = _iaqt_control_cmd_len+3; _iaqt_control_cmd_len <= 18; _iaqt_control_cmd_len++)
    _iaqt_control_cmd[_iaqt_control_cmd_len] = 0xcd;
  int len = _iaqt_control_cmd_len;
  pthread_mutex_unlock(&_iaqt_queue_mutex);

  // Tell the control panel we are ready to send this shit.
  send_aqt_cmd(ACK_CMD_READY_CTRL);
  
  LOG(IAQT_LOG,LOG_DEBUG, "Queued extended commandsed of length %d\n",len);
  //printHex(packets, 19);
  //printf("\n");

  //send_jandy_command(NULL, packets, cnt);
  return true;
}

bool queue_iaqt_control_command_str(iaqtControlCmdYype type, char *str) {
  if (waitfor_iaqt_ctrl_queue2empty() == false)
    return false;

  pthread_mutex_lock(&_iaqt_queue_mutex);
  _iaqt_control_announced = false;
  _iaqt_control_cmd[0] = DEV_MASTER;
  _iaqt_control_cmd[1] = 0x24;
  _iaqt_control_cmd[2] = 0x31;

  _iaqt_control_cmd_len = char2iaqtRSset(&_iaqt_control_cmd[3], str, strlen(str));

  // Need to bad time for some reason not yet known
  if (type == icct_settime) {
    //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x31|0x3a|0x30|0x31|0x00|0x30|0x32|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x60|0x10|0x03|
    // From position 11 (8 without pre) add 0x30|0x32|0x00
    _iaqt_control_cmd_len += 3;
    _iaqt_control_cmd[9] = 0x30;
    _iaqt_control_cmd[10] = 0x32;
    _iaqt_control_cmd[11] = 0x00;
  }
  // Pad with 0xcd for some reason.
  for(_iaqt_control_cmd_len = _iaqt_control_cmd_len+3; _iaqt_control_cmd_len <= 18; _iaqt_control_cmd_len++)
    _iaqt_control_cmd[_iaqt_control_cmd_len] = 0xcd;
  pthread_mutex_unlock(&_iaqt_queue_mutex);

  // Tell the control panel we are ready to send this shit.
  send_aqt_cmd(ACK_CMD_READY_CTRL);

  return true;
  //debuglogPacket()
}



bool goto_iaqt_page(const unsigned char pageID, struct aqualinkdata *aqdata) {
  if (iaqtCurrentPage() == pageID)
    return true;

  // If we go to Other Status Page, do that so we can quit
  if (pageID == IAQ_PAGE_STATUS) {
    send_aqt_cmd(KEY_IAQTCH_STATUS);
    unsigned char rtn = waitfor_iaqt_nextPage(aqdata); 
    if (rtn != IAQ_PAGE_STATUS && rtn != IAQ_PAGE_STATUS2) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Status page\n");
      return false;
    }
    LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch got to Status page\n");
    return true;
  }

  if (pageID == IAQ_PAGE_HOME || pageID == IAQ_PAGE_DEVICES) {
    if (iaqtCurrentPage() != IAQ_PAGE_HOME) {
      send_aqt_cmd(KEY_IAQTCH_HOME);
      if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_HOME) {
        //LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Home page\n");
        // We reset to home page after every programming cycle and error really don't care, so don't throw the error.
        return false;
      }
      LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch got to Home page\n");
    }

    if (pageID == IAQ_PAGE_DEVICES) {
      send_aqt_cmd(KEY_IAQTCH_HOMEP_KEY08);
      unsigned char page = waitfor_iaqt_nextPage(aqdata);
      if (page != IAQ_PAGE_DEVICES && page != IAQ_PAGE_DEVICES_REV_Yg) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Device page\n");
        return false;
      }
    }
    LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch got to Device page\n");
    return true;
  } else if (pageID == IAQ_PAGE_ONETOUCH) {
    send_aqt_cmd(KEY_IAQTCH_ONETOUCH);
    if (iaqtCurrentPage() != IAQ_PAGE_ONETOUCH) {
      unsigned char page = waitfor_iaqt_nextPage(aqdata);
      if (page != IAQ_PAGE_ONETOUCH ) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find OneTouch page\n");
        return false;
      }
    }
    LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch got to OneTouch page\n");
    return true;
  } else if (pageID == IAQ_PAGE_MENU || pageID == IAQ_PAGE_SET_TEMP || pageID == IAQ_PAGE_SET_TIME || pageID == IAQ_PAGE_SET_SWG ||
             pageID == IAQ_PAGE_SYSTEM_SETUP || pageID == IAQ_PAGE_FREEZE_PROTECT || pageID == IAQ_PAGE_LABEL_AUX || 
             pageID == IAQ_PAGE_VSP_SETUP) {
    // All other pages require us to go to Menu page
    send_aqt_cmd(KEY_IAQTCH_MENU);
    if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_MENU) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Menu page\n");
      return false;
    } else
      LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to Menu page\n");

    // There are several pages from the MENU page that have no other pages,
    // So hit those first
    if (pageID == IAQ_PAGE_SET_TEMP) {
      send_aqt_cmd(KEY_IAQTCH_SET_TEMP);
      if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_TEMP) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Set Temp page\n");
        return false;
      }
      LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to Set Temp page\n");
      return true;
    }

    if (pageID == IAQ_PAGE_SET_TIME) {
      send_aqt_cmd(KEY_IAQTCH_SET_DATETIME);
      if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_TIME) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Set Time page\n");
        return false;
      }
      LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to Set Time page\n");
      return true;
    }

    if (pageID == IAQ_PAGE_SET_SWG) {
      send_aqt_cmd(KEY_IAQTCH_SET_ACQUAPURE);
      if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_SWG) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Aquapure page\n");
        return false;
      }
      LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to Aquapure page\n");
      return true;
    }

    // All pages now require us to goto System Setup
    send_aqt_cmd(KEY_IAQTCH_SYSTEM_SETUP);
    if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SYSTEM_SETUP) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find System Setup page\n");
      return false;
    }
    LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to System Setup page\n");

    // Look for menu items for the next pages as they can change
    char *menuText;
    struct iaqt_page_button *button;

    switch (pageID) {
    case IAQ_PAGE_FREEZE_PROTECT:
      menuText = "Freeze Protection";
      break;
    case IAQ_PAGE_LABEL_AUX:
      menuText = "Label Aux";
      break;
    case IAQ_PAGE_VSP_SETUP:
      menuText = "VSP Setup";
      break;
    default:
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch unknown menu '0x%02hhx'\n", pageID);
      return false;
      break;
    }

    button = iaqtFindButtonByLabel(menuText);
    if (button == NULL) {
      //send_aqt_cmd(KEY_IAQTCH_NEXT_PAGE);
      // Try Next Page
      //unsigned char page = waitfor_iaqt_nextPage(aqdata);
      //LOG(IAQT_LOG, LOG_ERR, "PAGE RETURN IS 0x%02hhx\n",page);
      //if (waitfor_iaqt_nextPage(aqdata) != pageID) {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find '%s' button on page setup\n", menuText);
        return false;
      //}
    }
    // send_aqt_cmd(KEY_IAQTCH_KEY01);
    send_aqt_cmd(button->keycode);
    if (waitfor_iaqt_nextPage(aqdata) != pageID) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find %s page\n", menuText);
      return false;
    } else
      LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to %s page\n", menuText);

    return true;
  }

  LOG(IAQT_LOG, LOG_ERR, "IAQ Touch unknown menu '0x%02hhx'\n", pageID);
  return false;
}


#ifdef NEW_AQ_PROGRAMMER
void *set_aqualink_iaqtouch_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //char *buf = (char*)threadCtrl->thread_args;
  //char device_name[15];
  struct iaqt_page_button *pButton;

  //unsigned int device = atoi(&buf[0]);
  //unsigned int state = atoi(&buf[5]);
  
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int state = pargs->value;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_DEVICE_ON_OFF);

  LOG(IAQT_LOG,LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",button->label,state);

  // See if it's on the current page
  pButton = iaqtFindButtonByLabel(button->label);
  
  if (pButton == NULL && isVBUTTON_ALTLABEL(button->special_mask) ) { // Try alt button name
    pButton = iaqtFindButtonByLabel(((altlabel_detail *)button->special_mask_ptr)->altlabel);
  }

  if (pButton == NULL) {
    // No luck, go to the device page
    if ( goto_iaqt_page(IAQ_PAGE_DEVICES, aqdata) == false )
      goto f_end;

    pButton = iaqtFindButtonByLabel(button->label);

    if (pButton == NULL && isVBUTTON_ALTLABEL(button->special_mask) ) { // Try alt button name
      pButton = iaqtFindButtonByLabel(((altlabel_detail *)button->special_mask_ptr)->altlabel);
    }
 
  // If not found, keep advancing through further device pages while each one says it has more.
  // Bounded to match _devicePageButtons' 3 known pages (this page + 2 more), so a
  // misbehaving/wrapping page sequence can't spin this loop forever.
    for (int _devPage = 0; pButton == NULL && _devPage < 2 && iaqtFindButtonByIndex(16)->type == 0x03; _devPage++) {
      iaqt_queue_cmd(KEY_IAQTCH_NEXT_PAGE);
      waitfor_iaqt_nextPage(aqdata);
      pButton = iaqtFindButtonByLabel(button->label);
    }
  }

  if (pButton == NULL) {  
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find '%s' button on device list\n", button->label);
    goto f_end;
  }

  send_aqt_cmd(pButton->keycode);
  //LOG(IAQT_LOG, LOG_ERR, "******* CURRENT MENU '0x%02hhx' *****\n",iaqtCurrentPage());
  // NSF NEED TO CHECK FOR POPUP MESSAGE, AND KILL IT.
  //page IAQ_PAGE_SET_TEMP hit button 0
  //page IAQ_PAGE_COLOR_LIGHT hit button ???????
  // Heater popup can be cleared with a home button and still turn on.
  // Color light can be cleared with a home button, but won;t turn on.

  waitfor_iaqt_queue2empty();
  //waitfor_iaqt_messages(aqdata,1);

  // OR maybe use waitfor_iaqt_messages(aqdata,1)

  // Turn spa on when pump off, ned to remove message "spa will turn on after safty delay", home doesn't clear.
  send_aqt_cmd(KEY_IAQTCH_HOME);

  // Turn spa off need to read message if heater is on AND hit ok......

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
#else
void *set_aqualink_iaqtouch_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  char *buf = (char*)threadCtrl->thread_args;
  //char device_name[15];
  struct iaqt_page_button *button;

  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_DEVICE_ON_OFF);

  if (device > aqdata->total_buttons) {
    LOG(IAQT_LOG,LOG_ERR, "(PDA mode) Device On/Off :- bad device number '%d'\n",device);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  LOG(IAQT_LOG,LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",aqdata->aqbuttons[device].label,state);

  // See if it's on the current page
  button = iaqtFindButtonByLabel(aqdata->aqbuttons[device].label);
  
  if (button == NULL && isVBUTTON_ALTLABEL(aqdata->aqbuttons[device].special_mask) ) { // Try alt button name
    button = iaqtFindButtonByLabel(((altlabel_detail *)aqdata->aqbuttons[device].special_mask_ptr)->altlabel);
  }

  if (button == NULL) {
    // No luck, go to the device page
    if ( goto_iaqt_page(IAQ_PAGE_DEVICES, aqdata) == false )
      goto f_end;

    button = iaqtFindButtonByLabel(aqdata->aqbuttons[device].label);

    if (button == NULL && isVBUTTON_ALTLABEL(aqdata->aqbuttons[device].special_mask) ) { // Try alt button name
      button = iaqtFindButtonByLabel(((altlabel_detail *)aqdata->aqbuttons[device].special_mask_ptr)->altlabel);
    }
 
  // If not found, keep advancing through further device pages while each one says it has more.
  // Bounded to match _devicePageButtons' 3 known pages (this page + 2 more), so a
  // misbehaving/wrapping page sequence can't spin this loop forever.
    for (int _devPage = 0; button == NULL && _devPage < 2 && iaqtFindButtonByIndex(16)->type == 0x03; _devPage++) {
      iaqt_queue_cmd(KEY_IAQTCH_NEXT_PAGE);
      waitfor_iaqt_nextPage(aqdata);
      button = iaqtFindButtonByLabel(aqdata->aqbuttons[device].label);
    }
  }

  if (button == NULL) {
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find '%s' button on device list\n", aqdata->aqbuttons[device].label);
    goto f_end;
  }

  send_aqt_cmd(button->keycode);
  //LOG(IAQT_LOG, LOG_ERR, "******* CURRENT MENU '0x%02hhx' *****\n",iaqtCurrentPage());
  // NSF NEED TO CHECK FOR POPUP MESSAGE, AND KILL IT.
  //page IAQ_PAGE_SET_TEMP hit button 0
  //page IAQ_PAGE_COLOR_LIGHT hit button ???????
  // Heater popup can be cleared with a home button and still turn on.
  // Color light can be cleared with a home button, but won;t turn on.

  waitfor_iaqt_queue2empty();
  //waitfor_iaqt_messages(aqdata,1);

  // OR maybe use waitfor_iaqt_messages(aqdata,1)

  // Turn spa on when pump off, ned to remove message "spa will turn on after safty delay", home doesn't clear.
  send_aqt_cmd(KEY_IAQTCH_HOME);

  // Turn spa off need to read message if heater is on AND hit ok......

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
#endif

/*
 // Not complete, and not needed with 
void *set_aqualink_iaqtouch_onetouch_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  char *buf = (char*)threadCtrl->thread_args;
  //char device_name[15];
  struct iaqt_page_button *button;

  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_ONETOUCH_ON_OFF);

  LOG(IAQT_LOG,LOG_INFO, "OneTouch Device On/Off, device '%s', state %d\n",aqdata->aqbuttons[device].label,state);

  if ( goto_iaqt_page(IAQ_PAGE_ONETOUCH, aqdata) == false )
    goto f_end;

  // See if it's on the current page
  button = iaqtFindButtonByLabel(aqdata->aqbuttons[device].label);


  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}
*/



void *set_aqualink_iaqtouch_light_colormode( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
 
  //char device_name[15];
  struct iaqt_page_button *pButton;
  const char *mode_name;
  bool use_current_mode = false;
  bool turn_off = false;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE);


#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  aqkey *key = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int val = pargs->value;
  
  if (!isPLIGHT(key->special_mask)) {
    LOG(ALLB_LOG, LOG_ERR, "Can't program light for button '%d', configuration is incorrect\n", key->label);
    cleanAndTerminateThread(threadCtrl);
    return ptr;
  }

  int typ = ((clight_detail *)key->special_mask_ptr)->lightType;

#else
  char *buf = (char*)threadCtrl->thread_args;
  int val = atoi(&buf[0]);
  int btn = atoi(&buf[5]);
  int typ = atoi(&buf[10]);
  aqkey *key = NULL;

  struct programmerArgs *pargs = &threadCtrl->pArgs;
  if (pargs->button != NULL) {
    key = threadCtrl->pArgs.button;
    val = pargs->value;
    if (isPLIGHT(key->special_mask)) {
      typ = ((clight_detail *)key->special_mask_ptr)->lightType;
    } else {
      LOG(IAQT_LOG, LOG_ERR, "Can't can't get light type for button %s\n", key->label);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
  } else {
    if (btn < 0 || btn >= aqdata->total_buttons ) {
      LOG(IAQT_LOG, LOG_ERR, "Can't program light mode on button %d\n", btn);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    }
    key = &aqdata->aqbuttons[btn];
  }
#endif

  //unsigned char code = key->code;
  
  // We also need to cater for light being ON AND changing the color mode.  we have extra OK to hit.
  if (val == 0) {
    use_current_mode = true;
    LOG(IAQT_LOG, LOG_INFO, "Light Programming #: %d, button: %s, color light type: %d, using current mode\n", val, key->label, typ);
    // NOT SURE WHAT TO DO HERE..... No color mode and iaatouch doesn;t support last color in PDA mode.
   } else if (val == -1) {
    turn_off = true;
    LOG(IAQT_LOG, LOG_INFO, "Light Programming #: %d, button: %s, color light type: %d, Turning off\n", val, key->label, typ);
   } else {
    mode_name = light_mode_name(typ, val, IAQTOUCH);
    use_current_mode = false;
    if (mode_name == NULL) {
      LOG(IAQT_LOG, LOG_ERR, "Light Programming #: %d, button: %s, color light type: %d, couldn't find mode name '%s'\n", val, key->label, typ, mode_name);
      cleanAndTerminateThread(threadCtrl);
      return ptr;
    } else {
      LOG(IAQT_LOG, LOG_INFO, "Light Programming #: %d, button: %s, color light type: %d, name '%s'\n", val, key->label, typ, mode_name);
    }
  }
  
  
  // See if it's on the current page
  pButton = iaqtFindButtonByLabel(key->label);

//DPRINTF("First button find = %s\n",pButton==NULL?"null":pButton->name);

  if (pButton == NULL) {
    // No luck, go to the device page
    if ( goto_iaqt_page(IAQ_PAGE_DEVICES, aqdata) == false )
      goto f_end;

    pButton = iaqtFindButtonByLabel(key->label);
 
//DPRINTF("Second button find = %s\n",pButton==NULL?"null":pButton->name);
  // If not found, keep advancing through further device pages while each one says it has more.
  // Bounded to match _devicePageButtons' 3 known pages (this page + 2 more), so a
  // misbehaving/wrapping page sequence can't spin this loop forever.
    for (int _devPage = 0; pButton == NULL && _devPage < 2 && iaqtFindButtonByIndex(16)->type == 0x03; _devPage++) {
      iaqt_queue_cmd(KEY_IAQTCH_NEXT_PAGE);
      waitfor_iaqt_nextPage(aqdata);
      pButton = iaqtFindButtonByLabel(key->label);
//DPRINTF("Third button find = %s\n",pButton==NULL?"null":pButton->name);
    }
  }

  if (pButton == NULL) {  
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find '%s' button on device list, please check your config line '%s'\n", 
                           key->label,
                          isMASK_SET(key->special_mask, VIRTUAL_BUTTON)?"virtual_button_??_label":"button_??_label" );
    goto f_end;
  }
//DPRINTF("FOUND button = %s\n",pButton==NULL?"null":pButton->name);
  // WE have a iaqualink button, press it.
  LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch found '%s' sending keycode '0x%02hhx'\n", key->label, pButton->keycode);
  send_aqt_cmd(pButton->keycode);

  // See if we want to use the last color, or turn it off
  if (use_current_mode || turn_off) {
    // After pressing the button, Just need to wait for 5 seconds and it will :- 
    // a) if off turn on and default to last color.
    // b) if on, turn off. (pain that we need to wait 5 seconds.)
//DPRINTF("******** WAIT for next page\n");
    waitfor_iaqt_queue2empty();
    waitfor_iaqt_nextPage(aqdata);
    if (use_current_mode) {
      // Their is no message for this, so give one.
      sprintf(aqdata->last_display_message, "Light will turn %s in 5 seconds", turn_off?"off":"on");
      aqdata->is_display_message_programming = true;
      SET_DIRTY(aqdata->is_dirty);
    }
    // Wait for next page maybe?
    // Below needs a timeout.
    while (waitfor_iaqt_nextPage(aqdata) == IAQ_PAGE_COLOR_LIGHT);

    // Pre set key to off since it'll take ages
    if (turn_off)
      key->led->state = OFF;

    goto f_end;
  }

  // BELOW WE NEED TO CATER FOR OK POPUP IF LIGHT IS ALREADY ON
  if (pButton->state == 0x01) { // Light is on, need to select the BUTTON
    waitfor_iaqt_queue2empty();
    // We Should wait for popup message, before sending code
    send_aqt_cmd(KEY_IAQTCH_OK);
  }

  if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_COLOR_LIGHT) {
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find color light page\n");
    goto f_end;
  }

  // Now find the light mode and press it.
  pButton = iaqtFindButtonByLabel(mode_name);
  
  if (pButton == NULL) {
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch didn't find color '%s' in color light page\n",mode_name);
    goto f_end;
  }
  
  LOG(IAQT_LOG, LOG_DEBUG, "IAQ Touch found '%s' sending keycode '0x%02hhx' = '0x%02hhx' for light selection\n", mode_name, pButton->keycode, pButton->keycode+16);
  //DPRINTF("******** current page is '0x%02hhx'\n",iaqtCurrentPage());
  // NSF Key code is +16 for some reason.  ie key 0x07=(send 0x17).  0x0a=(send 0x1a)
  send_aqt_cmd(pButton->keycode + IAQ_COLOR_LIGHT_OFFSET);
  waitfor_iaqt_queue2empty();
  // Wait for popup message to disapera
  // This is iAq Popup messag    | HEX: 0x10|0x02|0x33|0x2c|0x00|0x01|0x50|0x6c|0x65|0x61|0x73|0x65|0x20|0x77|0x61|0x69|0x74|0x2e|0x2e|0x2e|0x0a|0x20|0x43|0x79|0x63|0x6c|0x69|0x6e|0x67|0x20|0x74|0x6f|0x20|0x63|0x68|0x6f|0x73|0x65|0x6e|0x20|0x63|0x6f|0x6c|0x6f|0x72|0x2e|0x00|0x00|0x00|0x00|0x2e|0x10|0x03| 
  // This is popup message clear | HEX: 0x10|0x02|0x33|0x2c|0x00|0x00|0x20|0x00|0x00|0x00|0x00|0x91|0x10|0x03|
  // 5th bit 0x00 = clear. (anything else message)

  // Pre set light to on, since it'll take ages after this finished.
  if (!turn_off)
    key->led->state = ON;

  unsigned char msg;
  int i=0;
  while ( (msg = waitfor_iaqt_nextMessage(aqdata, CMD_IAQ_MSG_LONG)) != NUL) {
    const char *pmsg = iaqtPopupMsg();
    //DPRINTF("******** popupMsg = %s ********\n",pmsg);
    if (pmsg[0] == ' ') {
      //DPRINTF("******** popupMsg clear ********\n");
      break;
    }
    if (++i > 5) {
       LOG(IAQT_LOG, LOG_WARNING, "IAQ Touch didn't see correct messages, color may no change\n");
       break;
    }
  }
/*
  unsigned char page;
  DPRINTF("******** current page is '0x%02hhx'\n",iaqtCurrentPage());
  while ( (page = waitfor_iaqt_nextPage(aqdata)) != NUL) {
     DPRINTF("******** next page is '0x%02hhx'\n",page);       // page = 0x48 IAQ_PAGE_COLOR_LIGHT
  }
*/
  //LOG(IAQT_LOG, LOG_ERR, "IAQ Touch WAIYING FOR 1 MESSAGES\n");
  //waitfor_iaqt_messages(aqdata, 1);

  // Finally found the color.  select it
  //send_aqt_cmd(button->keycode);
  //waitfor_iaqt_nextPage(aqdata);
  
  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}


void *set_aqualink_iaqtouch_pump_rpm( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  char VSPstr[20];
  struct iaqt_page_button *pButton;
  char *pumpName = NULL;
  int structIndex;

  //printf("**** program string '%s'\n",buf);
#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  //aqkey *button = threadCtrl->pArgs.button;
  int pumpIndex = pargs->alt_value;
  int pumpRPM = pargs->value;
#else
  char *buf = (char*)threadCtrl->thread_args;
  
  int pumpIndex = atoi(&buf[0]);
  int pumpRPM = atoi(&buf[2]);
#endif

  //int pumpRPM = atoi(&buf[2]);
  for (structIndex=0; structIndex < aqdata->num_pumps; structIndex++) {
    if (aqdata->pumps[structIndex].pumpIndex == pumpIndex) {
      pumpName = aqdata->pumps[structIndex].pumpName;
      if (aqdata->pumps[structIndex].pumpType == PT_UNKNOWN) {
        LOG(IAQT_LOG,LOG_ERR, "Can't set Pump RPM/GPM until type is known\n");
        cleanAndTerminateThread(threadCtrl);
        return ptr;
      }
      pumpRPM = RPM_check(aqdata->pumps[structIndex].pumpType, pumpRPM, aqdata);
      break;
    }
  }

  //int pumpRPM = atoi(&buf[2]);
  //int pumpIndex = 1;


  // NSF Should probably check pumpRPM is not -1 here

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_PUMP_RPM);

  LOG(IAQT_LOG,LOG_NOTICE, "IAQ Touch Set Pump %d to RPM %d\n",pumpIndex,pumpRPM);

  if ( goto_iaqt_page(IAQ_PAGE_DEVICES, aqdata) == false )
    goto f_end;

  sprintf(VSPstr, "VSP%1d Spd ADJ",pumpIndex);
  pButton = iaqtFindButtonByLabel(VSPstr);
  if (pButton == NULL && pumpName[0] != '\0') {
    // Try by name
    sprintf(VSPstr, "%s ADJ",pumpName);
    pButton = iaqtFindButtonByLabel(VSPstr);
    // Try by name with multiple spaces between pumpName and ADJ
    if (pButton == NULL) {
      pButton = iaqtFindButtonByLabelStartEnd(pumpName, "ADJ");
    } 
  }

  if (pButton == NULL) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not Pump by index 'VSP%1d Spd ADJ' or by name '%s' button on page setup\n", pumpIndex, VSPstr);
      goto f_end;
  }

  send_aqt_cmd(pButton->keycode);
  if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_VSP) {
    LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find set speed page for %s\n", VSPstr);
    goto f_end;
  }
  LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to %s page\n", VSPstr);

  //send_aqt_cmd(ACK_CMD_READY_CTRL);
  if (!queue_iaqt_control_command(0, pumpRPM) ||
      !waitfor_iaqt_ctrl_queue2empty())
    goto f_end;

  LOG(IAQT_LOG, LOG_INFO, "IAQ Touch got to %s page\n", VSPstr);

  // We should get the device page back.
  waitfor_iaqt_nextPage(aqdata);

  //goto_iaqt_page(IAQ_PAGE_STATUS, aqdata);
/*
  // Send Devices pButton.
  send_aqt_cmd(KEY_IAQTCH_HOME);
  waitfor_iaqt_queue2empty();
  sleep(1);

  send_aqt_cmd(KEY_IAQTCH_HOMEP_KEY08);
  waitfor_iaqt_queue2empty();
  sleep(1);

  send_aqt_cmd(KEY_IAQTCH_KEY03);
*/
  //send_aqt_cmd(0x80);

  // Go to status page on startup to read devices
  // This is too soon
  //goto_iaqt_page(IAQ_PAGE_STATUS, aqdata);
  //waitfor_iaqt_nextPage(aqdata);

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *set_aqualink_iaqtouch_vsp_assignments( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_IAQTOUCH_VSP_ASSIGNMENT);

  if ( goto_iaqt_page(IAQ_PAGE_VSP_SETUP, aqdata) == false )
    goto f_end;

  /* Info:   Button 00|         ePump   | type=0xff | state=0x00 | unknown=0xff  
   * Info:   Button 01| Intelliflo VF   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 02| Intelliflo VS   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 03|         ePump   | type=0xff | state=0x00 | unknown=0xff 
   * ---- This is next column
   * Info:   Button 04|    Filtration   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 05|    Filtration   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 06|    Filtration   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 07| Not Installed   | type=0xff | state=0x00 | unknown=0xff 
   * ----- Next column
   * Info:   Button 08|        600 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 09|         15 GPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 10|        600 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 11|        600 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * ----- Next colums
   * Info:   Button 12|       3450 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 13|        130 GPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 14|       3450 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 15|       3450 RPM  | type=0xff | state=0x00 | unknown=0xff 
   * ----- Next column
   * Info:   Button 16|          2500   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 17|           N/A   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 18|          2500   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 19|          2500   | type=0xff | state=0x00 | unknown=0xff 
   * ----- Next column
   * Info:   Button 20|           :03   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 21|           N/A   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 22|           :03   | type=0xff | state=0x00 | unknown=0xff 
   * Info:   Button 23|           :03   | type=0xff | state=0x00 | unknown=0xff 
   *  
*/
  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

void *get_aqualink_iaqtouch_freezeprotect( void *ptr )
{
  LOG(IAQT_LOG,LOG_ERR, "IAQ Touch get_aqualink_iaqtouch_freezeprotect has not beed tested\n");
  LOG(IAQT_LOG,LOG_ERR, "**** We should not be here ****\n");

  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_IAQTOUCH_FREEZEPROTECT);

  if ( goto_iaqt_page(IAQ_PAGE_FREEZE_PROTECT, aqdata) == false )
    goto f_end;

  // The Message at index 0 is the deg that freeze protect is set to.
  int frz = rsm_atoi(iaqtGetMessageLine(0));
  if (frz >= 0) {
    //aqdata->frz_protect_set_point = frz;
    //aqdata->frz_protect_state = ON;
    SET_IF_CHANGED( aqdata->frz_protect_set_point, frz, aqdata->is_dirty);
    SET_IF_CHANGED( aqdata->frz_protect_state, ON, aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_NOTICE, "IAQ Touch Freeze Protection setpoint %d\n",frz);
  }

  // Need to run over table messages and check ens with X for on off.

  // Go to status page on startup to read devices
  goto_iaqt_page(IAQ_PAGE_STATUS, aqdata);

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}
void *get_aqualink_iaqtouch_setpoints( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  struct iaqt_page_button *button;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_IAQTOUCH_SETPOINTS);

  // Get product info.
  send_aqt_cmd(KEY_IAQTCH_HELP);
  waitfor_iaqt_nextPage(aqdata);
  
  if ( goto_iaqt_page(IAQ_PAGE_SET_TEMP, aqdata) == false )
    goto f_end;

  // Button 0 is "Pool Heat 50"
  // Button 2 is "Spa Heat 100"
  button = iaqtFindButtonByLabel("Pool Heat");
  if (button != NULL) {
    //aqdata->pool_htr_set_point = rsm_atoi((char *)&button->name + 9);
    SET_IF_CHANGED( aqdata->pool_htr_set_point, rsm_atoi((char *)&button->name + 9), aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch got to Pool heater setpoint %d\n",aqdata->pool_htr_set_point);
  } else {
    button = iaqtFindButtonByLabel("Temp1");
    if (button != NULL) {
      //aqdata->pool_htr_set_point = rsm_atoi((char *)&button->name + 5);
      SET_IF_CHANGED( aqdata->pool_htr_set_point, rsm_atoi((char *)&button->name + 5), aqdata->is_dirty);
      LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch got to Temp1 setpoint %d\n",aqdata->pool_htr_set_point);
      if (isSINGLE_DEV_PANEL != true)
      {
        changePanelToMode_Only();
        LOG(IAQT_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
      }
    }
  }

  button = iaqtFindButtonByLabel("Spa Heat");
  if (button != NULL) {
    //aqdata->spa_htr_set_point = rsm_atoi((char *)&button->name + 8);
    SET_IF_CHANGED( aqdata->spa_htr_set_point, rsm_atoi((char *)&button->name + 8), aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch got to Spa heater setpoint %d\n",aqdata->spa_htr_set_point);
  } else {
    button = iaqtFindButtonByLabel("Temp2");
    if (button != NULL) {
      //aqdata->spa_htr_set_point = rsm_atoi((char *)&button->name + 5);
      SET_IF_CHANGED( aqdata->spa_htr_set_point, rsm_atoi((char *)&button->name + 5), aqdata->is_dirty);
      LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch got to Temp2 setpoint %d\n",aqdata->spa_htr_set_point);
      if (isSINGLE_DEV_PANEL != true)
      {
        changePanelToMode_Only();
        LOG(IAQT_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
      }
    }
  }

  button = iaqtFindButtonByLabel("Chiller");
  if (button != NULL) {
    //aqdata->chiller_set_point = rsm_atoi((char *)&button->name + 8);
    SET_IF_CHANGED( aqdata->chiller_set_point, rsm_atoi((char *)&button->name + 8), aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch got to Chiller setpoint %d\n",aqdata->chiller_set_point);
  }

  if ( goto_iaqt_page(IAQ_PAGE_FREEZE_PROTECT, aqdata) == false )
    goto f_end;

  // The Message at index 0 is the deg that freeze protect is set to.
  int frz = rsm_atoi(iaqtGetMessageLine(0));
  if (frz >= 0) {
   // aqdata->frz_protect_set_point = frz;
    SET_IF_CHANGED( aqdata->frz_protect_set_point, frz, aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_NOTICE, "IAQ Touch Freeze Protection setpoint %d\n",frz);
  }

  // Get the temperature units if we are in iaq touch PDA mode 
  if (isPDA_PANEL) {
    // If we are here, hit back then next button to get button with degrees on it.
    // Only if in PDA mode
    send_aqt_cmd(KEY_IAQTCH_BACK); // Clear the feeze protect menu and go back to system setup

    if ( waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SYSTEM_SETUP )
    {
      LOG(IAQT_LOG,LOG_ERR, "Couldn't get back to setup page, Temperature units unknown, default to DegF\n");
      //aqdata->temp_units = FAHRENHEIT;
      SET_IF_CHANGED( aqdata->temp_units, FAHRENHEIT, aqdata->is_dirty);
      goto f_end;
    }

    send_aqt_cmd(KEY_IAQTCH_NEXT_PAGE_ALTERNATE); // Go to page 2

    if ( waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SYSTEM_SETUP2 )
    {
      LOG(IAQT_LOG,LOG_ERR, "Couldn't get back to setup page, Temperature units unknown, default to DegF\n");
      //aqdata->temp_units = FAHRENHEIT;
      SET_IF_CHANGED( aqdata->temp_units, FAHRENHEIT, aqdata->is_dirty);
      goto f_end;
    }

    button = iaqtFindButtonByLabel("Degrees");

    if (button != NULL) {
      LOG(IAQT_LOG,LOG_NOTICE, "Temperature units are '%s'\n",button->name);
      if (button->name[8] == 'C') {
        LOG(IAQT_LOG,LOG_NOTICE, "Temperature unit message is '%s' set to degC\n",button->name);
        //aqdata->temp_units = CELSIUS;
        SET_IF_CHANGED( aqdata->temp_units, CELSIUS, aqdata->is_dirty);
      } else {
        LOG(IAQT_LOG,LOG_NOTICE, "Temperature unit message is '%s' set to degF\n",button->name);
        //aqdata->temp_units = FAHRENHEIT;
        SET_IF_CHANGED( aqdata->temp_units, FAHRENHEIT, aqdata->is_dirty);
      }
      

      /*
      printf("************** DEG IS '%c'\n", (uintptr_t)button->name[8]);
      if ( (uintptr_t)button->name[8] == 'C') { // NSF THIS DOES NOT WORK, LEAVE COMPILER WARNING TO COME BACK AND FIX
        aqdata->temp_units = CELSIUS;
      } else {
        aqdata->temp_units = FAHRENHEIT;
      }
      */
    }
  }

  // Need to run over table messages and check ens with X for on off.

  // Go to status page on startup to read devices
  goto_iaqt_page(IAQ_PAGE_STATUS, aqdata);

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  // just stop compiler error, ptr is not valid as it's just been freed
  return ptr;
}

// THIS IS NOT FINISHED.   
void *get_aqualink_iaqtouch_aux_labels( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  int i;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_GET_IAQTOUCH_AUX_LABELS);

  if ( goto_iaqt_page(IAQ_PAGE_LABEL_AUX, aqdata) == false )
    goto f_end;

  // Need to loop over messages.  Tab 0x09 is next in each message
  /*
   * Info:   Table Messages 00| Circuit      Circuit Name    Freeze
   * Info:   Table Messages 01| Aux1 Aux1    No 
   * Info:   Table Messages 02| Aux2 Aux2    No 
   * Info:   Table Messages 03| Aux3 Pool Light      No 
   * Info:   Table Messages 04| Aux4 Aux4    No 
   * Info:   Table Messages 05| Aux5 Aux5    No 
   * Info:   Table Messages 06| Aux6 Aux6    No 
   * Info:   Table Messages 07| Aux7 Aux7    No 
   * 
   * Info:   Table Messages ??| Aux B7 Aux B7    No 
   */
  const char *buf;
  int aux;
  // Loop over panel buttons or lines which ever is lowest.
  //for(i=1; i < 18; i++) // NSF Need to take out hard code of 18
  for(i=1; i < PANEL_SIZE(); i++)
  {
    buf = iaqtGetTableInfoLine(i);
    LOG(IAQT_LOG,LOG_INFO, "From Messages %.2d| %s\n",i,buf);
    //printf("**** BUF '%s'\n",aux,buf);
    aux = rsm_atoi(buf + 3);
    LOG(IAQT_LOG,LOG_INFO, "**** AUX %d = '%s'\n",aux,buf + 5);
  }

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

bool set_aqualink_iaqtouch_aquapure( struct aqualinkdata *aqdata, bool boost, int val )
{
  struct iaqt_page_button *button;
  static unsigned char b_pool = NUL;
  static unsigned char b_spa = NUL;
  static unsigned char b_boost = NUL;

  if ( goto_iaqt_page(IAQ_PAGE_SET_SWG, aqdata) == false )
    return false;

  // IF pool button not set, first time here, need to cache the buttons
  // as we only get them once.
  if (b_pool == NUL) {
    button = iaqtFindButtonByLabel("Pool");
    if (button != NULL)
      b_pool = button->keycode;
    button = iaqtFindButtonByLabel("Spa");
    if (button != NULL)
      b_spa = button->keycode;
    button = iaqtFindButtonByLabel("Quick Boost");
    if (button != NULL)
      b_boost = button->keycode;
  }
  // If spa is on, set SWG for spa, if not set SWG for pool

  if (boost) {
    if (b_boost != NUL)
      send_aqt_cmd(b_boost);
    else {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Boost button on SWG page\n");
      return false;
    }
    waitfor_iaqt_queue2empty();
    if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_QBOOST) {
      LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Boost Start button on SWG page\n");
    }
    if(val==true)
      button = iaqtFindButtonByLabel("Start");
    else
      button = iaqtFindButtonByLabel("Stop");
    send_aqt_cmd(button->keycode);
    waitfor_iaqt_queue2empty();
    waitfor_iaqt_nextPage(aqdata);
    aqdata->boost = val;
  } else {
    if (aqdata->aqbuttons[SPA_INDEX].led->state != OFF) {
      if (b_spa != NUL)
        send_aqt_cmd(b_spa);
      else {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Spa button on SWG page\n");
        return false;
      }
    } else {
      if (b_pool != NUL)
        send_aqt_cmd(b_pool);
      else {
        LOG(IAQT_LOG, LOG_ERR, "IAQ Touch did not find Pool button on SWG page\n");
        return false;;
      }
    }

    waitfor_iaqt_queue2empty();
    if (!queue_iaqt_control_command(0, val) ||
        !waitfor_iaqt_ctrl_queue2empty())
      return false;
    waitfor_iaqt_nextMessage(aqdata, CMD_IAQ_PAGE_BUTTON);
  }
  //LOG(IAQT_LOG, LOG_NOTICE, "IAQ Touch got to %s page\n", VSPstr);

  // We should get the device page back.
  //waitfor_iaqt_nextPage(aqdata);

  goto_iaqt_page(IAQ_PAGE_STATUS, aqdata);

  return true;
}


void *set_aqualink_iaqtouch_swg_percent( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_SWG_PERCENT);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif
 
  val = setpoint_check(SWG_SETPOINT, val, aqdata);

  if (set_aqualink_iaqtouch_aquapure(aqdata, false, val))
    setSWGpercent(aqdata, val);

  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_iaqtouch_swg_boost( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_SWG_BOOST);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  //logMessage(LOG_DEBUG, "programming BOOST to %s\n", val==true?"On":"Off");

  set_aqualink_iaqtouch_aquapure(aqdata, true, val);

  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}


bool set_aqualink_iaqtouch_heater_setpoint( struct aqualinkdata *aqdata, SP_TYPE type, int val)
{
  struct iaqt_page_button *button;
  char *name;

  if ( goto_iaqt_page(IAQ_PAGE_SET_TEMP, aqdata) == false )
    return false;
  
  if (isCOMBO_PANEL) {
    if (type == SP_POOL)
      name = "Pool Heat";
    else
      name = "Spa Heat";
  } else {
    if (type == SP_POOL)
      name = "Temp1";
    else
      name = "Temp2";
  }

  if (type == SP_CHILLER) {
    name = "Chiller";
  }

  button = iaqtFindButtonByLabel(name);

  if (button == NULL) {
    //aqdata->pool_htr_set_point = rsm_atoi((char *)&button->name + 9);
    LOG(IAQT_LOG,LOG_ERR, "IAQ Touch did not find heater setpoint '%s' on page\n",name);
    return false;
  }

  send_aqt_cmd(button->keycode);
  waitfor_iaqt_queue2empty();

  if (!queue_iaqt_control_command(0, val) ||
      !waitfor_iaqt_ctrl_queue2empty())
    return false;
  waitfor_iaqt_nextMessage(aqdata, CMD_IAQ_PAGE_BUTTON);

  button = iaqtFindButtonByLabel(name);

  if (button != NULL) {
    int value = 0;
    if (type == SP_POOL) {
      aqdata->pool_htr_set_point = rsm_atoi((char *)&button->name + strlen(name));
      value = aqdata->pool_htr_set_point;
    } else if (type == SP_SPA) {
      aqdata->spa_htr_set_point = rsm_atoi((char *)&button->name + strlen(name));
      value = aqdata->spa_htr_set_point;
    } else if (type == SP_CHILLER) {
      aqdata->chiller_set_point = rsm_atoi((char *)&button->name + strlen(name));
      value = aqdata->chiller_set_point;
    }
    LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch set %s heater setpoint to %d\n",name,value);
  }

  return true;
}

void *set_aqualink_iaqtouch_spa_heater_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_SPA_HEATER_TEMP);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  val = setpoint_check(SPA_HTR_SETPOINT, val, aqdata);

  set_aqualink_iaqtouch_heater_setpoint(aqdata, SP_SPA, val);

  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_iaqtouch_pool_heater_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_POOL_HEATER_TEMP);
  
#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  val = setpoint_check(POOL_HTR_SETPOINT, val, aqdata);

  set_aqualink_iaqtouch_heater_setpoint(aqdata, SP_POOL, val);

  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_iaqtouch_chiller_temp( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_POOL_HEATER_TEMP);
  
#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  int val = pargs->value;
#else
  int val = atoi((char*)threadCtrl->thread_args);
#endif

  //val = setpoint_check(POOL_HTR_SETPOINT, val, aqdata);

  set_aqualink_iaqtouch_heater_setpoint(aqdata, SP_CHILLER, val);

  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_iaqtouch_pump_vs_program( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  //char *buf = (char*)threadCtrl->thread_args;
  char VSPstr[20];
  //int structIndex;
  struct iaqt_page_button *pButton;
  int structIndex;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM);

#ifdef NEW_AQ_PROGRAMMER
  struct programmerArgs *pargs = &threadCtrl->pArgs;
  //aqkey *button = threadCtrl->pArgs.button;
  //unsigned char code = pargs->button->code;
  int vspindex = pargs->alt_value;
  int pumpIndex = pargs->value;
#else
  char *buf = (char*)threadCtrl->thread_args;
  int pumpIndex = atoi(&buf[0]);
  //int pumpRPM = -1;
  int vspindex = atoi(&buf[2]);
#endif

  for (structIndex=0; structIndex < aqdata->num_pumps; structIndex++) {
    if (aqdata->pumps[structIndex].pumpIndex == pumpIndex) {
      if (aqdata->pumps[structIndex].pumpType == PT_UNKNOWN) {
        LOG(IAQT_LOG,LOG_ERR, "Can't set Pump RPM/GPM until type is known\n");
        cleanAndTerminateThread(threadCtrl);
        return ptr;
      }
      //pumpRPM = RPM_check(aqdata->pumps[structIndex].pumpType, atoi(&buf[2]), aqdata);
      break;
    }
  }

  //int pumpRPM = atoi(&buf[2]);
  //int pumpIndex = 1;

  sprintf(VSPstr, "VSP%1d Spd ADJ",pumpIndex);

  LOG(IAQT_LOG,LOG_NOTICE, "Set Pump %d to VSP Index %d\n",pumpIndex,vspindex);

  if ( goto_iaqt_page(IAQ_PAGE_DEVICES, aqdata) == false )
    goto f_end;

  pButton = iaqtFindButtonByLabel(VSPstr);
  if (pButton == NULL) {
    LOG(IAQT_LOG, LOG_ERR, "Did not find '%s' button on page setup\n", VSPstr);
    goto f_end;
  }

  send_aqt_cmd(pButton->keycode);
  if (waitfor_iaqt_nextPage(aqdata) != IAQ_PAGE_SET_VSP) {
    LOG(IAQT_LOG, LOG_ERR, "Did not find %s page\n", VSPstr);
    goto f_end;
  }
  LOG(IAQT_LOG, LOG_INFO, "Got to %s page\n", VSPstr);

  // Select the button index.
  pButton = iaqtFindButtonByIndex(vspindex);
  if (pButton == NULL) {
    LOG(IAQT_LOG, LOG_ERR, "Did not find '%d' button on page\n", vspindex);
    goto f_end;
  }

  send_aqt_cmd(pButton->keycode);
  waitfor_iaqt_queue2empty();
  // Probably wait.

  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

void *set_aqualink_iaqtouch_time( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aqdata = threadCtrl->aqdata;
  struct iaqt_page_button *button;
  char buf[20];
  //int len;
  int i;
  //bool AM;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_SET_IAQTOUCH_SET_TIME);

  time_t now = time(0);   // get time now
  struct tm *result = localtime(&now);
 

  if ( goto_iaqt_page(IAQ_PAGE_SET_TIME, aqdata) == false ) {
    LOG(IAQT_LOG,LOG_ERR, "IAQ Touch didn't find set time page\n");
    goto f_end;
  }

  // Button 0 is date.
  button = iaqtFindButtonByIndex(0);  
  if (button == NULL) {
    LOG(IAQT_LOG,LOG_ERR, "IAQ Touch date button on set time page\n");
    goto f_end;
  }

  // Print DD/MM/YY into string
  strftime(buf, 20, "%D", result);
  // Do we need to set the date
  if (rsm_strcmp(buf, (char *)button->name) != 0) {
    // Press date button.
    send_aqt_cmd(button->keycode);
    waitfor_iaqt_queue2empty();
    // Queue the date string
    if ( queue_iaqt_control_command_str(icct_setdate, buf)) {
      LOG(IAQT_LOG,LOG_NOTICE, "Set date to %s\n",buf);
      if (!waitfor_iaqt_ctrl_queue2empty())
        goto f_end;
    } else {
      LOG(IAQT_LOG,LOG_ERR, "Failed to queue commandset for setting date\n");
      goto f_end;
    }
    
  } else {
    LOG(IAQT_LOG,LOG_DEBUG, "Date %s is accurate, not changing\n",button->name);
  }
  
  // Always assume time is incorrect if wer have been called.
  // Button 1 is time.
  button = iaqtFindButtonByIndex(1);
  if (button == NULL) {
    LOG(IAQT_LOG,LOG_ERR, "IAQ Touch time button on set time page\n");
    goto f_end;
  }
  // Press time button.
  send_aqt_cmd(button->keycode);
  waitfor_iaqt_queue2empty();

  // Set AM/PM button, not sure how to get default state since it's blank to start, so keep pressing till we get AM or PM we want
  i = 0;
  strftime(buf, 20, "%p", result);
  LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch AM/PM check '%s' to '%s'\n",buf,iaqtGetMessageLine(2));
  while ( rsm_strcmp(buf, iaqtGetMessageLine(2)) != 0 && i < 3) {
    send_aqt_cmd(0x13);
    waitfor_iaqt_queue2empty();
    waitfor_iaqt_nextMessage(aqdata, CMD_IAQ_PAGE_MSG);
    i++;
  }
  LOG(IAQT_LOG,LOG_DEBUG, "IAQ Touch AM/PM is now '%s'\n",iaqtGetMessageLine(2));

  // Print HH:MM into string
  strftime(buf, 20, "%I:%M", result);
  if (queue_iaqt_control_command_str(icct_settime, buf)) {
    LOG(IAQT_LOG,LOG_NOTICE, "Set time to %s\n",buf);
    if (!waitfor_iaqt_ctrl_queue2empty())
      goto f_end;
  } else {
    LOG(IAQT_LOG,LOG_ERR, "Failed to queue commandset for setting time\n");
  }


  f_end:
  goto_iaqt_page(IAQ_PAGE_HOME, aqdata);
  cleanAndTerminateThread(threadCtrl);

  return ptr;
}

/* Infor on setting time

  //Info:    iAQ Touch: Button 00|            07/05/20   | type=0x01 | state=0x00 | unknown=0x00 | keycode=0x11
  //Info:    iAQ Touch: Button 01|             1:02 PM   | type=0x01 | state=0x00 | unknown=0x00 | keycode=0x12

  // Send button to select time / date.
  // Date
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x11|0x24|0x10|0x03|
  // Set Time
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x12|0x25|0x10|0x03|

  // Wait for Time page comes back
  //Info:    iAQ Touch: Page: Set Time | 0x4b

  // Set time to 01/01/01
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x80|0x93|0x10|0x03|
  //Debug:   iAQ Touch: To 0x33 of type Unknown  | HEX: 0x10|0x02|0x33|0x31|0x01|0x77|0x10|0x03|
  //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x31|0x2f|0x30|0x31|0x2f|0x30|0x31|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x83|0x10|0x03|
                                                                                 //0x30|0x31|0x2f|0x30|0x31|0x2f|0x30|0x31|
  // Set Time to 02/02/02
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x80|0x93|0x10|0x03|
  //Debug:   iAQ Touch: To 0x33 of type Unknown  | HEX: 0x10|0x02|0x33|0x31|0x01|0x77|0x10|0x03|
  //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x32|0x2f|0x30|0x32|0x2f|0x30|0x32|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x86|0x10|0x03|

  // So 0x30 is base numbers again (ie 0)
  //0x24|0x31 Set time & start?
  //0x2f is seperator or /
  //0x00 is end
  //0xcd padding

  // Set time 01:01 AM
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x80|0x93|0x10|0x03|
  //Debug:   iAQ Touch: To 0x33 of type Unknown  | HEX: 0x10|0x02|0x33|0x31|0x01|0x77|0x10|0x03|
  //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x31|0x3a|0x30|0x31|0x00|0x30|0x32|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x60|0x10|0x03|
  // Set time 01:01 PM
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x80|0x93|0x10|0x03|
  //Debug:   iAQ Touch: To 0x33 of type Unknown  | HEX: 0x10|0x02|0x33|0x31|0x01|0x77|0x10|0x03|
  //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x31|0x3a|0x30|0x31|0x00|0x30|0x32|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x60|0x10|0x03|
  // Set time 09:55
  //Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x80|0x93|0x10|0x03|
  //Debug:   iAQ Touch: To 0x33 of type Unknown  | HEX: 0x10|0x02|0x33|0x31|0x01|0x77|0x10|0x03|
  //Debug:   RS Serial: To 0x00 of type iAq pBut | HEX: 0x10|0x02|0x00|0x24|0x31|0x30|0x39|0x3a|0x35|0x35|0x00|0x30|0x32|0x00|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0xcd|0x71|0x10|0x03|


  // 0x3a seperator :
  // Select PM
  // Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x13|0x26|0x10|0x03|
  // Debug:   iAQ Touch: To 0x33 of type iAq pMes | HEX: 0x10|0x02|0x33|0x25|0x02|0x50|0x4d|0x00|0x09|0x10|0x03|
  // Select AM
  // Debug:   RS Serial: To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x13|0x26|0x10|0x03|
  // Debug:   iAQ Touch: To 0x33 of type iAq pMes | HEX: 0x10|0x02|0x33|0x25|0x02|0x41|0x4d|0x00|0xfa|0x10|0x03|

  //LOG(IAQT_LOG,LOG_DEBUG "Setting time to %d/%d/%d %d:%d\n", result->tm_mon + 1, result->tm_mday, result->tm_year + 1900, result->tm_hour + 1, result->tm_min);
*/
