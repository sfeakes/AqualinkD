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
#include <string.h>

#include "aq_serial.h"
#include "aqualink.h"
#include "utils.h"
#include "packetLogger.h"
#include "iaqtouch.h"
#include "iaqtouch_aq_programmer.h"
#include "aq_programmer.h"
#include "rs_msg_utils.h"
#include "devices_jandy.h"

void temp_debugprintExtraInfo(unsigned char *pk, int length);


#ifdef ATOUCH_TEST
void set_iaq_cansend(bool yes) {}
bool in_iaqt_programming_mode(struct aqualinkdata *aq_data) {return false;}
bool iaqt_queue_cmd(unsigned char cmd) {}
bool in_programming_mode(struct aqualinkdata *aq_data){return false;}
void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aq_data){}
void kick_aq_program_thread(struct aqualinkdata *aq_data, emulation_type source_type){}
void aq_programmer(program_type type, char *args, struct aqualinkdata *aq_data){}
#endif

unsigned char _button_keys[] = { KEY_IAQTCH_KEY01,
                            KEY_IAQTCH_KEY02,
                            KEY_IAQTCH_KEY03,
                            KEY_IAQTCH_KEY04,
                            KEY_IAQTCH_KEY05,
                            KEY_IAQTCH_KEY06,
                            KEY_IAQTCH_KEY07,
                            KEY_IAQTCH_KEY08,
                            KEY_IAQTCH_KEY09,
                            KEY_IAQTCH_KEY10,
                            KEY_IAQTCH_KEY11,
                            KEY_IAQTCH_KEY12,
                            KEY_IAQTCH_KEY13,
                            KEY_IAQTCH_KEY14,
                            KEY_IAQTCH_KEY15};


#define IAQ_STATUS_PAGE_LINES 18
#define IAQ_PAGE_BUTTONS      24
#define IAQ_MSG_TABLE_LINES   IAQ_STATUS_PAGE_LINES // No idea actual size, so just use this until figured out.
#define IAQT_TABLE_MSGLEN     32

unsigned char _currentPageLoading;
unsigned char _currentPage;

unsigned char _lastMsgType = 0x00;
//unsigned char _last_kick_type = -1;

int _deviceStatusLines = 0;
char _deviceStatus[IAQ_STATUS_PAGE_LINES][AQ_MSGLEN+1];
char _tableInformation[IAQ_MSG_TABLE_LINES][IAQT_TABLE_MSGLEN+1];
struct iaqt_page_button _pageButtons[IAQ_PAGE_BUTTONS];

// Need to cache these two pages, as only get updates after initial load.
struct iaqt_page_button _devicePageButtons[IAQ_PAGE_BUTTONS];
struct iaqt_page_button _devicePage2Buttons[IAQ_PAGE_BUTTONS];
struct iaqt_page_button _deviceSystemSetupButtons[IAQ_PAGE_BUTTONS];

unsigned char iaqtLastMsg()
{
  return _lastMsgType;
}

void set_iaqtouch_lastmsg(unsigned char msgtype)
{
  _lastMsgType = msgtype;
}

bool wasiaqtThreadKickTypePage()
{
  switch(_lastMsgType) {
    //case CMD_IAQ_PAGE_MSG:
    //case CMD_IAQ_PAGE_BUTTON:
    //case CMD_IAQ_PAGE_START:
    case CMD_IAQ_PAGE_END:
      return true;
    break;
    default:
      return false;
    break;
  }

  return false;
}


unsigned char iaqtCurrentPage()
{
  return _currentPage;
}

const char *iaqtGetMessageLine(int index) {
  if (index < IAQ_STATUS_PAGE_LINES)
    return _deviceStatus[index];

  return NULL;
}
const char *iaqtGetTableInfoLine(int index) {
  if (index < IAQ_MSG_TABLE_LINES)
    return _tableInformation[index];

  return NULL;
}

struct iaqt_page_button *iaqtFindButtonByIndex(int index) {
  //int i;
  struct iaqt_page_button *buttons;

  // NSF Need to merge this from iaqtFindButtonByLabel function
  if (_currentPage == IAQ_PAGE_DEVICES )
    buttons = _devicePageButtons;
  else if (_currentPage == IAQ_PAGE_DEVICES2 )
    buttons = _devicePage2Buttons;
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP )
    buttons = _deviceSystemSetupButtons;
  else
    buttons = _pageButtons;

  if (index>=0 && index < IAQ_PAGE_BUTTONS) {
      return &buttons[index];
  }

  return NULL;
}

struct iaqt_page_button *iaqtFindButtonByLabel(char *label) {
  int i;
  struct iaqt_page_button *buttons;

  if (_currentPage == IAQ_PAGE_DEVICES )
    buttons = _devicePageButtons;
  else if (_currentPage == IAQ_PAGE_DEVICES2 )
    buttons = _devicePage2Buttons;
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP )
    buttons = _deviceSystemSetupButtons;
  else
    buttons = _pageButtons;

  for (i=0; i < IAQ_PAGE_BUTTONS; i++) {
    //if (_pageButtons[i].state != 0 || _pageButtons[i].type != 0 || _pageButtons[i].unknownByte != 0)
    if (rsm_strcmp((char *)buttons[i].name,label) == 0)
      return &buttons[i];
  }
  return NULL;
}

int num2iaqtRSset (unsigned char* packetbuffer, int num, bool pad4unknownreason)
{
  //unsigned int score = 42;   // Works for score in [0, UINT_MAX]
  //unsigned char tmp;
  //printf ("num via printf:     %u\n", num);   // For validation

  int bcnt = 0;
  int digits = 0;
  unsigned int div = 1;
  unsigned int digit_count = 1;
  while ( div <= num / 10 ) {
    digit_count++;
    div *= 10;
  }
  while ( digit_count > 0 ) {
    packetbuffer[bcnt] = (num / div + 0x30); // 48 = 0x30 base number for some reason. (ie 48=0)
    num %= div;
    div /= 10;
    digit_count--;
    bcnt++;
  }

  for (digits = bcnt; bcnt < 6; bcnt++) { // Note setting digits to bcnt is correct.  Saving current count to different int
    if (bcnt == 4 && digits <= 3 ) // Less than 4 digits (<1000), need to add a 0x30
      packetbuffer[bcnt] = 0x30;
    else
      packetbuffer[bcnt] = NUL;
  }

  return bcnt;
}

int char2iaqtRSset(unsigned char* packetbuffer, char *msg, int msg_len)
{
  int bcnt=0;

  for (bcnt=0; bcnt < msg_len; bcnt++ ){
    packetbuffer[bcnt] = msg[bcnt];
  }

  packetbuffer[bcnt] = 0x00;

  return ++bcnt;
}


void createDeviceUpdatePacket() {
  unsigned char packets[AQ_MAXPKTLEN];
  int cnt;

  packets[0] = DEV_MASTER;
  packets[1] = 0x24;
  packets[2] = 0x31;

  cnt = num2iaqtRSset(&packets[3], 1000, true);

  for(cnt = cnt+3; cnt <= 18; cnt++)
    packets[cnt] = 0xcd;

  //printHex(packets, 19);
  //printf("\n");

  //send_jandy_command(NULL, packets, cnt);
}

void processPageMessage(unsigned char *message, int length)
{
  if ( (int)message[PKT_IAQT_MSGINDX] >= IAQ_STATUS_PAGE_LINES ) {
    LOG(IAQT_LOG,LOG_ERR, "Run out of IAQT message buffer, need %d have %d\n",(int)message[PKT_IAQT_MSGINDX],IAQ_STATUS_PAGE_LINES);
    return;
  }
  // 2nd page of device status doesn;t gine us new page message
  if (_currentPageLoading == IAQ_PAGE_STATUS || _currentPage == IAQ_PAGE_STATUS) {
    //sprintf(_deviceStatus[(int)message[4]], message[5], AQ_MSGLEN);
    //strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], (char *)message + PKT_IAQT_MSGDATA, AQ_MSGLEN);
    rsm_strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], &message[PKT_IAQT_MSGDATA], AQ_MSGLEN, length-PKT_IAQT_MSGDATA-3);
  } else {
    //strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], (char *)message + PKT_IAQT_MSGDATA, AQ_MSGLEN);
    rsm_strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], &message[PKT_IAQT_MSGDATA], AQ_MSGLEN, length-PKT_IAQT_MSGDATA-3);
    //LOG(IAQT_LOG,LOG_ERR, "Request to assign message to unknown page,'%.*s'\n",AQ_MSGLEN,(char *)message + PKT_IAQT_MSGDATA);
  }

  //LOG(IAQT_LOG,LOG_DEBUG, "Message :- '%d' '%.*s'\n",(int)message[PKT_IAQT_MSGINDX], length-PKT_IAQT_MSGDATA-3, &message[PKT_IAQT_MSGDATA]);
}

void processTableMessage(unsigned char *message, int length)
{
  if ( (int)message[5] < IAQ_MSG_TABLE_LINES )
    rsm_strncpy(_tableInformation[(int)message[5]], &message[6], IAQT_TABLE_MSGLEN, length-PKT_IAQT_MSGDATA-3);
  else
    LOG(IAQT_LOG,LOG_ERR, "Run out of IAQT table buffer, need %d have %d\n",(int)message[5],IAQ_MSG_TABLE_LINES);
}

void processPageButton(unsigned char *message, int length)
{
  struct iaqt_page_button *button;
  int index = (int)message[PKT_IAQT_BUTINDX];

  if (_currentPageLoading == IAQ_PAGE_DEVICES )
    button = &_devicePageButtons[index];
  else if (_currentPageLoading == IAQ_PAGE_DEVICES2 )
    button = &_devicePage2Buttons[index];
  else if (_currentPageLoading == IAQ_PAGE_SYSTEM_SETUP )
    button = &_deviceSystemSetupButtons[index];
  else
    button = &_pageButtons[index];

  button->state  = message[PKT_IAQT_BUTSTATE];
  button->type  = message[PKT_IAQT_BUTTYPE];
  button->unknownByte = message[PKT_IAQT_BUTUNKNOWN];

  if (message[PKT_IAQT_BUTSTATE] == 0x0d)
    button->keycode = message[PKT_IAQT_BUTTYPE];
  else if (index < 15) {
    button->keycode = _button_keys[index];
  }
  // This doesn't work with return which is 0x00
  
  //strncpy(&button->name, (char *)message + PKT_IAQT_BUTDATA, AQ_MSGLEN);
  rsm_strncpy_nul2sp((char *)button->name, &message[PKT_IAQT_BUTDATA], IAQT_MSGLEN, length-PKT_IAQT_BUTDATA-3);

  LOG(IAQT_LOG,LOG_DEBUG, "Added Button %d %s\n",index,button->name);

/*
  _pageButtons[index].state = (int)message[5];
  _pageButtons[index].type  = (int)message[7];
  _pageButtons[index].unknownByte = (int)message[6];
  // This doesn't work with return which is 0x00
  strncpy(&_pageButtons[index].name, (char *)message + 8, AQ_MSGLEN);

  LOG(IAQT_LOG,LOG_NOTICE, "Added Button %d %s\n",index,_pageButtons[index].name);
*/
}



// Log if we saw a pump in a device page cycle.
void iaqt_pump_update(struct aqualinkdata *aq_data, int updated) {
  const int bitmask[MAX_PUMPS] = {1,2,4,8};
  static unsigned char updates = '\0';
  int i;

  if (updated == -1) {
    for(i=0; i < MAX_PUMPS; i++) {
      if ((updates & bitmask[i]) != bitmask[i]) {
        aq_data->pumps[i].rpm = PUMP_OFF_RPM;
        aq_data->pumps[i].gpm = PUMP_OFF_GPM;
        aq_data->pumps[i].watts = PUMP_OFF_WAT;
        LOG(IAQT_LOG,LOG_DEBUG, "Clearing pump %d\n",i);
        aq_data->updated  =true;
      }
    }
    updates = '\0';
  } else if (updated >=0 && updated < MAX_PUMPS) {
     updates |= bitmask[updated];
     LOG(IAQT_LOG,LOG_DEBUG, "Got pump update message for pump %d\n",updated);
  }
}

void passDeviceStatusPage(struct aqualinkdata *aq_data)
{
  int i;
  int pi;
  pump_detail *pump = NULL;
  //bool found_swg = false;
  //int pump_index = 0;

  for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ ) {
    //LOG(IAQT_LOG,LOG_NOTICE, "Passing message %.2d| %s\n",i,_deviceStatus[i]);
    if (rsm_strcmp(_deviceStatus[i],"Intelliflo VS") == 0 ||
        rsm_strcmp(_deviceStatus[i],"Intelliflo VF") == 0 ||
        rsm_strcmp(_deviceStatus[i],"Jandy ePUMP") == 0 ||
        rsm_strcmp(_deviceStatus[i],"ePump AC") == 0)
    {
      int pump_index = rsm_atoi(&_deviceStatus[i][14]);
      if (pump_index <= 0)
        pump_index = rsm_atoi(&_deviceStatus[i][10]);  // ePump AC seems to display index in different position 
      for (pi=0; pi < aq_data->num_pumps; pi++) {
        if (aq_data->pumps[pi].pumpIndex == pump_index) {
          iaqt_pump_update(aq_data, pi); // Log that we saw a pump
          pump = &aq_data->pumps[pi];
          aq_data->updated  =true;
          if (pump->pumpType == PT_UNKNOWN){
            if (rsm_strcmp(_deviceStatus[i],"Intelliflo VS") == 0)
              pump->pumpType = VSPUMP;
            else if (rsm_strcmp(_deviceStatus[i],"Intelliflo VF") == 0)
              pump->pumpType = VFPUMP;
            else if (rsm_strcmp(_deviceStatus[i],"Jandy ePUMP") == 0 ||
                     rsm_strcmp(_deviceStatus[i],"ePump AC") == 0)
              pump->pumpType = EPUMP;

            LOG(IAQT_LOG,LOG_DEBUG, "Pump %d set to type %s\n",pump->pumpIndex, (pump->pumpType==EPUMP?"Jandy ePUMP":(pump->pumpType==VFPUMP?"Intelliflo VF":"Intelliflo VS")) );
          }
        }
      }
      if (pump == NULL)
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump at index %d\n",_deviceStatus[i],pump_index);
        
      continue;

    } else if (rsm_strcmp(_deviceStatus[i],"RPM:") == 0) {
      if (pump != NULL)
        pump->rpm = rsm_atoi(&_deviceStatus[i][9]);
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"GPM:") == 0) {
      if (pump != NULL)
        pump->gpm = rsm_atoi(&_deviceStatus[i][9]);
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"Watts:") == 0) {
      if (pump != NULL)
        pump->watts = rsm_atoi(&_deviceStatus[i][9]);
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"*** Priming ***") == 0) {
      if (pump != NULL)
        pump->rpm = PUMP_PRIMING;
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"(Offline)") == 0) {
      if (pump != NULL)
        pump->rpm = PUMP_OFFLINE;
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"(Priming Error)") == 0) {
      if (pump != NULL)
        pump->rpm = PUMP_ERROR;
      else
        LOG(IAQT_LOG,LOG_WARNING, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
      // Need to catch messages like 
      // *** Priming ***
      // (Priming Error)
      // (Offline)
    } else {
      pump = NULL;
    }

    if (rsm_strcmp(_deviceStatus[i],"Chemlink") == 0) {
      /*   Info:  =    Chemlink 1   
           Info:  =  ORP 750/PH 7.0  */
      i++;
      if (rsm_strcmp(_deviceStatus[i],"ORP") == 0) {
        int orp = rsm_atoi(&_deviceStatus[i][4]);
        char *indx = strchr(_deviceStatus[i], '/');
        float ph = rsm_atof(indx+3);
        if (aq_data->ph != ph || aq_data->orp != orp) {
          aq_data->ph = ph;
          aq_data->orp = orp;
        }
        LOG(IAQT_LOG,LOG_INFO, "Set Cemlink ORP = %d PH = %f from message '%s'\n",orp,ph,_deviceStatus[i]);
      }
    }
#ifdef READ_SWG_FROM_EXTENDED_ID
    else if (rsm_strcmp(_deviceStatus[i],"AQUAPURE") == 0) {
      //aq_data->swg_percent = rsm_atoi(&_deviceStatus[i][9]);
      if (changeSWGpercent(aq_data, rsm_atoi(&_deviceStatus[i][9])))
        LOG(IAQT_LOG,LOG_DEBUG, "Set swg %% to %d from message'%s'\n",aq_data->swg_percent,_deviceStatus[i]);
    } else if (rsm_strcmp(_deviceStatus[i],"salt") == 0) {
      aq_data->swg_ppm = rsm_atoi(&_deviceStatus[i][5]);
      LOG(IAQT_LOG,LOG_DEBUG, "Set swg PPM to %d from message'%s'\n",aq_data->swg_ppm,_deviceStatus[i]);
    } else if (rsm_strcmp(_deviceStatus[i],"Boost Pool") == 0) {
      aq_data->boost = true;
      // Let RS pickup time remaing message.
    }  
#endif

  } // for
}

void debugPrintButtons(struct iaqt_page_button buttons[])
{
  int i;
  for (i=0; i < IAQ_PAGE_BUTTONS; i++) {
    if (buttons[i].state != 0 || buttons[i].type != 0 || buttons[i].unknownByte != 0 || buttons[i].keycode != 0)
      LOG(IAQT_LOG,LOG_INFO, "Button %.2d| %21.21s | type=0x%02hhx | state=0x%02hhx | unknown=0x%02hhx | keycode=0x%02hhx\n",i,buttons[i].name,buttons[i].type,buttons[i].state,buttons[i].unknownByte,buttons[i].keycode);
  }
}

void processPage(struct aqualinkdata *aq_data)
{
  int i;

  LOG(IAQT_LOG,LOG_INFO, "Page: %s | 0x%02hhx\n",iaqt_page_name(_currentPage),_currentPage);

  switch(_currentPage) {
    case IAQ_PAGE_STATUS:
    case IAQ_PAGE_STATUS2:
      //LOG(IAQT_LOG,LOG_INFO, "Status Page:-\n");
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (strlen(_deviceStatus[i]) > 1)
          LOG(IAQT_LOG,LOG_INFO, "Status page %.2d| %s\n",i,_deviceStatus[i]);
      
      debugPrintButtons(_pageButtons);
      passDeviceStatusPage(aq_data);
      // If button 1 is type 0x02 then there is a next page.  Since status page isn't used for programming, hit the next page button.
      if (_pageButtons[1].type == 0x02)
        iaqt_queue_cmd(KEY_IAQTCH_KEY02);
      else
        iaqt_pump_update(aq_data, -1); // Reset pumps.
    break;
    case IAQ_PAGE_DEVICES:
      //LOG(IAQT_LOG,LOG_INFO, "Devices Page #1:-\n");
      debugPrintButtons(_devicePageButtons);
    break;
    case IAQ_PAGE_DEVICES2:
      //LOG(IAQT_LOG,LOG_INFO, "Devices Page #2:-\n");
      debugPrintButtons(_devicePage2Buttons);
    break;
    case IAQ_PAGE_COLOR_LIGHT:
      //LOG(IAQT_LOG,LOG_INFO, "Color Light Page :-\n");
      debugPrintButtons(_pageButtons);
    break;
    case IAQ_PAGE_HOME:
      //LOG(IAQT_LOG,LOG_INFO, "Home Page :-\n");
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (_deviceStatus[i][0] != 0)
          LOG(IAQT_LOG,LOG_INFO, "Status page %.2d| %s\n",i,_deviceStatus[i]);

      debugPrintButtons(_pageButtons);
    break;
    case IAQ_PAGE_SYSTEM_SETUP:
      //LOG(IAQT_LOG,LOG_INFO, "System Setup :-\n");
      debugPrintButtons(_deviceSystemSetupButtons);
    break;
    case IAQ_PAGE_SET_VSP:
      debugPrintButtons(_pageButtons);
    break;

    default:
      //LOG(IAQT_LOG,LOG_INFO, "** UNKNOWN PAGE 0x%02hhx **\n",_currentPage);
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (_deviceStatus[i][0] != 0)
          LOG(IAQT_LOG,LOG_INFO, "Page %.2d| %s\n",i,_deviceStatus[i]);

      debugPrintButtons(_pageButtons);
    break;
  }

  for (i=0; i < IAQ_MSG_TABLE_LINES; i++) {
    if (strlen((char *)_tableInformation[i]) > 0)
      LOG(IAQT_LOG,LOG_INFO, "Table Messages %.2d| %s\n",i,_tableInformation[i]);
  }
}

#define REQUEST_STATUS_POLL_COUNT 50

bool process_iaqtouch_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data)
{
  static int cnt = 0;
  static bool gotStatus = true;
  //char buff[1024];
  
  if (packet[PKT_CMD] == CMD_IAQ_PAGE_START) {
    LOG(IAQT_LOG,LOG_DEBUG, "Turning IAQ SEND off\n");
    set_iaq_cansend(false);
    _currentPageLoading = packet[PKT_IAQT_PAGTYPE];
    _currentPage = NUL;
    memset(_pageButtons, 0, IAQ_PAGE_BUTTONS * sizeof(struct iaqt_page_button));
    memset(_deviceStatus, 0, sizeof(char) * IAQ_STATUS_PAGE_LINES * AQ_MSGLEN+1 );
    memset(_tableInformation, 0, sizeof(char) * IAQ_MSG_TABLE_LINES * AQ_MSGLEN+1 );
    // Fix bug with control panel where after a few hours status page disapears and you need to hit menu.
    if (gotStatus == false)
      gotStatus = true;
    //[IAQ_STATUS_PAGE_LINES][AQ_MSGLEN+1];
  } else if (packet[PKT_CMD] == CMD_IAQ_PAGE_END) {
    set_iaq_cansend(true);
    LOG(IAQT_LOG,LOG_DEBUG, "Turning IAQ SEND on\n");
    if (_currentPageLoading != NUL) {
      _currentPage = _currentPageLoading;
      _currentPageLoading = NUL;
    } else {
      LOG(IAQT_LOG,LOG_DEBUG, "Page end message without proceding page start, ignoring!\n");
    }
    processPage(aq_data);
  } else if (packet[PKT_CMD] == CMD_IAQ_TABLE_MSG) {
    processTableMessage(packet, length);
  } else if (packet[PKT_CMD] == CMD_IAQ_PAGE_MSG) {
    processPageMessage(packet, length);
  } else if (packet[PKT_CMD] == CMD_IAQ_PAGE_BUTTON) {
    processPageButton(packet, length);
    // Second page on status doesn't send start & end, but button is message, so use that to kick off next page. 
    if (_currentPage == IAQ_PAGE_STATUS) {
      /* Notice: Added Button 1 
       * Notice: To 0x33 of type iAq pBut | HEX: 0x10|0x02|0x33|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x6a|0x10|0x03|
       *                   Button | 1 | 0x00 |  |-|  |-|  -off-
       * 
       *   SHOULD PROBABLY USE ABOVE TO CHECK.
       */
      //if (packet[PKT_IAQT_BUTTYPE] == 0x02 )
       processPage(aq_data);
    }
    
    // if we get a button with 0x00 state on Light Page, that's the end of page.
    if (_currentPageLoading == IAQ_PAGE_COLOR_LIGHT) {
      if (packet[7] == 0x00) {
        //printf("** MANUAL PAGE END\n");
        LOG(IAQT_LOG,LOG_DEBUG, "MANUAL PAGE END\n");
        _currentPage = _currentPageLoading;
        _currentPageLoading = NUL;
        processPage(aq_data);
        set_iaq_cansend(true);
        // Also END page here, as you can send commands.
        // NEED to rethink this approach.  ie, selecting light needs to hold open while showing page, no page end, then select light color, then message "please wait", the finally done 
      }
    }
  } 
  
  if (packet[3] == 0x29) {
    //printf("*****  iAqualink Touch STARTUP Message ******* \n");
    LOG(IAQT_LOG,LOG_DEBUG, "STARTUP Message\n");
    queueGetProgramData(IAQTOUCH, aq_data);

    //aq_programmer(AQ_SET_IAQTOUCH_SET_TIME, NULL, aq_data);
  }
  // Standard ack/poll not interested in printing or kicking threads
  if (packet[3] == 0x30) {
    //LOG(IAQT_LOG,LOG_DEBUG, "poll count %d\n",cnt);
    // Load status page every 50 messages
    if (cnt++ > REQUEST_STATUS_POLL_COUNT && in_programming_mode(aq_data) == false ) {
      iaqt_queue_cmd(KEY_IAQTCH_STATUS);
      gotStatus = false; // Reset if we got status page, for fix panel bug.
      //aq_programmer(AQ_GET_IAQTOUCH_VSP_ASSIGNMENT, NULL, aq_data);
      cnt = 0;
    } else if (gotStatus == false && cnt > 3) {
      // Fix bug with control panel where after a few hours status page disapears and you need to hit menu.
      LOG(IAQT_LOG,LOG_INFO, "Overcomming Jandy control panel bug, (missing status, goto menu)\n",cnt);
      iaqt_queue_cmd(KEY_IAQTCH_HOME);
      iaqt_queue_cmd(KEY_IAQTCH_STATUS);
    } else if (in_programming_mode(aq_data) == true) {
      // Set count to something close to above, so we will pull latest info once programming has finished.
      // This is goot for VSP GPM programming as it takes number of seconds to register once finished programming.
      // -5 seems to be too quick for VSP/GPM so using 10
      cnt = REQUEST_STATUS_POLL_COUNT - 10; 
    }
    
    return false;
  }

  //debuglogPacket(IAQT_LOG ,packet, length);

  //_lastMsgType = packet[PKT_CMD];
  set_iaqtouch_lastmsg(packet[PKT_CMD]);
  //debuglogPacket(IAQT_LOG ,packet, length);
  //beautifyPacket(buff, packet, length);
  //LOG(IAQT_LOG,LOG_DEBUG, "%s", buff);

  //temp_debugprintExtraInfo(packet, length);

  kick_aq_program_thread(aq_data, IAQTOUCH);

  return false;
}


const char *iaqt_page_name(const unsigned char page)
{
  switch (page){
    case IAQ_PAGE_HOME:
      return "HOME";
    break;
    case IAQ_PAGE_STATUS:
      return "Status";
    break;
    case IAQ_PAGE_STATUS2:
      return "Status (diff ID)";
    break;
    case IAQ_PAGE_DEVICES:
      return "Devices #1";
    break;
    case IAQ_PAGE_DEVICES2:
      return "Devices #2";
    break;
    case IAQ_PAGE_SET_TEMP:
      return "Set Temp";
    break;
    case IAQ_PAGE_MENU:
      return "MENU";
    break;
    case IAQ_PAGE_SET_VSP:
      return "Set VSP";
    break;
    case IAQ_PAGE_SET_TIME:
      return "Set Time";
    break;
    case IAQ_PAGE_SET_DATE:
      return "Set Date";
    break;
    case IAQ_PAGE_SET_SWG:
      return "Set Aquapure";
    break;
    case IAQ_PAGE_SET_BOOST:
      return "Set Boost";
    break;
    case IAQ_PAGE_SET_QBOOST:
      return "Set Quick Boost";
    break;
    case IAQ_PAGE_ONETOUCH:
      return "OneTouch";
    break;
    case IAQ_PAGE_COLOR_LIGHT:
      return "Color Lights";
    break;
    case IAQ_PAGE_SYSTEM_SETUP:
      return "System Setup";
    break;
    case IAQ_PAGE_VSP_SETUP:
      return "VSP Setup";
    break;
    case IAQ_PAGE_FREEZE_PROTECT:
      return "Freeze Protect";
    break;
    case IAQ_PAGE_LABEL_AUX:
      return "Label Aux";
    break;
    default:
      return "** Unknown **";
    break;
  }
  return "";
}



void temp_debugprintExtraInfo(unsigned char *pk, int length)
{
  if (pk[PKT_CMD] == CMD_IAQ_PAGE_MSG) {
    int i;
    printf("                         Message | %d | ",(int)pk[4]);
    // Byte #4 is line index on status page at least
    // 1 bytes unknown #4.
    // Message starts at 5
    for (i=5;i<length-3;i++)
      printf("%c",pk[i]);
    printf("\n");
  }
  else if (pk[PKT_CMD] == CMD_IAQ_TABLE_MSG) {
    int i;
    printf("                       Table Msg | %d | ",(int)pk[5]);
    for (i=6;i<length-3;i++)
      printf("%c",pk[i]);
    printf("\n");
  }
  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_BUTTON) {
    int i;
    printf("                          Button | %d | 0x%02hhx | ",(int)pk[4],pk[7]);
    // byte 4 is button index.
    // byte 5 is status 0x00 off (0x01, 0x02, 0x03)
    // byte 6
    // byte 7
    // 6 & 7 0x01|0x00 for normal stuff
    // 6 & 7 0x00|0x0c for lights with 7 incrementing.
    // 7 = Key to send in ACK to select, at least for Light modes.
    // (Might also be in pk5=0x0d) Think if pk6 = 0x01, pk7 not used, if pk=0x00 then pk7 is keycode to send if you select that option.
    // Byte 7 0x03 PAGE DOWN / 0x02 PAGE UP
    // 2 bytes unknown #5 and #6.
    // Message starts at 8
    // if pk[5] == 0x0d start at 8
    for (i=8;i<length-3;i++) {
      // If we get a 0x00 looks like a return.
      if (pk[i] == 0x00)
        printf(" |-| ");
      
      printf("%c",pk[i]);
    }

    if (pk[5] == 0x00)
      printf(" -off- ");
    else if (pk[5] == 0xff)
      printf(" -blank/unknown- ");
    else if (pk[5] == 0x03)
      printf(" -Enabeled- ");
    else if (pk[5] == 0x01)
      printf(" -on- ");
    else if (pk[5] == 0x0d)
      printf(" -Light something 0x0d|0x%02hhx|0x%02hhx %d - ",pk[6],pk[7],(int)pk[7]);
    else
      printf(" -?- 0x%02hhx ",pk[5]);

    printf("\n");
  }

  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_START) {
    if (pk[4] == IAQ_PAGE_STATUS)
      printf("                        New Page | Status\n");
    else if (pk[4] == IAQ_PAGE_HOME)
      printf("                        New Page | Home\n");
    else if (pk[4] == IAQ_PAGE_DEVICES)
      printf("                        New Page | Other Devices\n");
    else if (pk[4] == IAQ_PAGE_DEVICES2)
      printf("                        New Page | Other Devices page 2\n");
    else if (pk[4] == IAQ_PAGE_SET_TEMP)
      printf("                        New Page | Set Temp\n");
    else if (pk[4] == IAQ_PAGE_MENU)
      printf("                        New Page | MENU\n");
    else if (pk[4] == IAQ_PAGE_SET_VSP)
      printf("                        New Page | VSP Adjust\n");
    else if (pk[4] == IAQ_PAGE_SET_TIME)
      printf("                        New Page | Set Time\n");
    else if (pk[4] == IAQ_PAGE_SET_DATE)
      printf("                        New Page | Set Date\n");
    else if (pk[4] == IAQ_PAGE_SET_SWG)
      printf("                        New Page | Set Aquapure\n");
    else if (pk[4] == IAQ_PAGE_SET_BOOST)
      printf("                        New Page | Set BOOST\n");
    else if (pk[4] == IAQ_PAGE_SET_QBOOST)
      printf("                        New Page | Set Quick BOOST\n");
    else if (pk[4] == IAQ_PAGE_ONETOUCH)
      printf("                        New Page | One Touch\n");
    else if (pk[4] == IAQ_PAGE_COLOR_LIGHT)
      printf("                        New Page | Color Light\n");
    else
      printf("                        New Page | unknown 0x%02hhx\n",pk[4]);
  }
  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_END) {
    printf("                        New Page | Finished\n");
  }
}



/*


Notice: Turning IAQ SEND off
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x23|0x5b|0xc1|0x10|0x03|
                        New Page | Status
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x2d|0x45|0x71|0x75|0x69|0x70|0x6d|0x65|0x6e|0x74|0x20|0x53|0x74|0x61|0x74|0x75|0x73|0x00|0xcc|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x00|0x46|0x69|0x6c|0x74|0x65|0x72|0x20|0x50|0x75|0x6d|0x70|0x00|0x90|0x10|0x03|
                         Message | 0 | Filter Pump
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x01|0x53|0x6f|0x6c|0x61|0x72|0x20|0x48|0x65|0x61|0x74|0x20|0x45|0x4e|0x41|0x00|0x00|0x10|0x03|
                         Message | 1 | Solar Heat ENA
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x02|0x41|0x75|0x78|0x31|0x00|0xc9|0x10|0x03|
                         Message | 2 | Aux1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x03|0x41|0x75|0x78|0x32|0x00|0xcb|0x10|0x03|
                         Message | 3 | Aux2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x04|0x50|0x6f|0x6f|0x6c|0x20|0x4c|0x69|0x67|0x68|0x74|0x00|0x1e|0x10|0x03|
                         Message | 4 | Pool Light
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x05|0x41|0x75|0x78|0x36|0x00|0xd1|0x10|0x03|
                         Message | 5 | Aux6
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x06|0x4a|0x61|0x6e|0x64|0x79|0x20|0x65|0x50|0x55|0x4d|0x50|0x20|0x20|0x20|0x31|0x00|0xbc|0x10|0x03|
                         Message | 6 | Jandy ePUMP   1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x07|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x37|0x35|0x30|0x00|0x06|0x10|0x03|
                         Message | 7 |     RPM: 2750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x08|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x30|0x00|0x4d|0x10|0x03|
                         Message | 8 |   Watts: 0
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x09|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x46|0x20|0x32|0x00|0x91|0x10|0x03|
                         Message | 9 | Intelliflo VF 2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0a|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x32|0x35|0x30|0x00|0x04|0x10|0x03|
                         Message | 10 |     RPM: 2250
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0b|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x31|0x30|0x30|0x00|0xb1|0x10|0x03|
                         Message | 11 |   Watts: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0c|0x20|0x20|0x20|0x20|0x47|0x50|0x4d|0x3a|0x20|0x31|0x30|0x30|0x00|0xc3|0x10|0x03|
                         Message | 12 |     GPM: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0d|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x53|0x20|0x33|0x00|0xa3|0x10|0x03|
                         Message | 13 | Intelliflo VS 3
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0e|0x28|0x4f|0x66|0x66|0x6c|0x69|0x6e|0x65|0x29|0x00|0x8a|0x10|0x03|
                         Message | 14 | (Offline)
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0f|0x20|0x00|0x97|0x10|0x03|
                         Message | 15 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x10|0x20|0x00|0x98|0x10|0x03|
                         Message | 16 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x11|0x20|0x00|0x99|0x10|0x03|
                         Message | 17 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 |  |-|  |-|  |-|  -off- 
Notice: Turning IAQ SEND on

*/

/*
Notice: Turning IAQ SEND off
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x23|0x5b|0xc1|0x10|0x03|
                        New Page | Status
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x2d|0x45|0x71|0x75|0x69|0x70|0x6d|0x65|0x6e|0x74|0x20|0x53|0x74|0x61|0x74|0x75|0x73|0x00|0xcc|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x00|0x46|0x69|0x6c|0x74|0x65|0x72|0x20|0x50|0x75|0x6d|0x70|0x00|0x90|0x10|0x03|
                         Message | 0 | Filter Pump
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x01|0x41|0x75|0x78|0x31|0x00|0xc8|0x10|0x03|
                         Message | 1 | Aux1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x02|0x41|0x75|0x78|0x32|0x00|0xca|0x10|0x03|
                         Message | 2 | Aux2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x03|0x50|0x6f|0x6f|0x6c|0x20|0x4c|0x69|0x67|0x68|0x74|0x00|0x1d|0x10|0x03|
                         Message | 3 | Pool Light
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x04|0x41|0x75|0x78|0x36|0x00|0xd0|0x10|0x03|
                         Message | 4 | Aux6
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x05|0x4a|0x61|0x6e|0x64|0x79|0x20|0x65|0x50|0x55|0x4d|0x50|0x20|0x20|0x20|0x31|0x00|0xbb|0x10|0x03|
                         Message | 5 | Jandy ePUMP   1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x06|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x37|0x35|0x30|0x00|0x05|0x10|0x03|
                         Message | 6 |     RPM: 2750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x07|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x30|0x00|0x4c|0x10|0x03|
                         Message | 7 |   Watts: 0
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x08|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x46|0x20|0x32|0x00|0x90|0x10|0x03|
                         Message | 8 | Intelliflo VF 2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x09|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x32|0x35|0x30|0x00|0x03|0x10|0x03|
                         Message | 9 |     RPM: 2250
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0a|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x31|0x30|0x33|0x00|0xb3|0x10|0x03|
                         Message | 10 |   Watts: 103
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0b|0x20|0x20|0x20|0x20|0x47|0x50|0x4d|0x3a|0x20|0x31|0x30|0x30|0x00|0xc2|0x10|0x03|
                         Message | 11 |     GPM: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0c|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x53|0x20|0x33|0x00|0xa2|0x10|0x03|
                         Message | 12 | Intelliflo VS 3
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0d|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x31|0x37|0x35|0x30|0x00|0x0b|0x10|0x03|
                         Message | 13 |     RPM: 1750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0e|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x33|0x34|0x00|0x8a|0x10|0x03|
                         Message | 14 |   Watts: 34
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0f|0x41|0x51|0x55|0x41|0x50|0x55|0x52|0x45|0x20|0x33|0x30|0x25|0x00|0x83|0x10|0x03|
                         Message | 15 | AQUAPURE 30%
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x10|0x53|0x61|0x6c|0x74|0x20|0x33|0x38|0x30|0x30|0x20|0x50|0x50|0x4d|0x00|0x04|0x10|0x03|
                         Message | 16 | Salt 3800 PPM
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x11|0x20|0x00|0x99|0x10|0x03|
                         Message | 17 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 |  |-|  |-|  |-|  -off- 
Notice: Turning IAQ SEND on
*/


/*

Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|


No next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 

Get next page
Debug:  To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x12|0x25|0x10|0x03|

Has next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|
                          Button | 1 | 0x02 |  |-|  |-|  -off- 

Can go up
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 
*/

/*

No Next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 
Next Page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|
                          Button | 1 | 0x02 |  |-|  |-|  -off- 


*/