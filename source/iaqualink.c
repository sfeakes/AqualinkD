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
#include <stdlib.h>

#include "aq_serial.h"
#include "aqualink.h"
#include "iaqualink.h"
#include "packetLogger.h"
#include "aq_serial.h"
#include "serialadapter.h"
#include "rs_msg_utils.h"

#define IAQUA_QLEN 20

typedef struct iaqulnkcmd
{
  unsigned char command[20];
  int length;
} iaqualink_cmd;

iaqualink_cmd _iqaua_queue[IAQUA_QLEN];
unsigned char _std_cmd[2];
int _iaqua_q_length = 0;
bool _aqua_last_cmdfrom_queue = false;

bool push_iaqualink_cmd(unsigned char *cmd, int length) {
  if (_iaqua_q_length >= IAQUA_QLEN ) {
    LOG(IAQL_LOG,LOG_ERR, "Queue overflow, last command ignored!\n");
    return false;
  }

  memcpy(_iqaua_queue[_iaqua_q_length].command, cmd, length);
  _iqaua_queue[_iaqua_q_length].length = length;
  _iaqua_q_length++;

  LOG(IAQL_LOG, LOG_NOTICE, "Queue cmd, size %d,  queu length=%d\n",length, _iaqua_q_length);

  //LOG(IAQL_LOG,LOG_DEBUG, "Added to message queue, position %d 0x%02hhx|0x%02hhx|0x%02hhx|0x%02hhx\n",_rssa_q_length-1,_rssa_queue[_rssa_q_length-1][0],_rssa_queue[_rssa_q_length-1][1],_rssa_queue[_rssa_q_length-1][2],_rssa_queue[_rssa_q_length-1][3]);
  return true;

}

//unsigned char *get_iaqualink_cmd(unsigned char source_message_type, unsigned char *dest_message, int *len) {
int get_iaqualink_cmd(unsigned char source_message_type, unsigned char **dest_message) {

  //LOG(IAQL_LOG, LOG_INFO, "Caculating cmd\n");

  _aqua_last_cmdfrom_queue = false;

  _std_cmd[0] = source_message_type;
  _std_cmd[1] = 0x00;
  *dest_message = _std_cmd;
  int len = 2;

  if (source_message_type == 0x73 && _iaqua_q_length > 0) 
  { // Send big/long message
    if ( _iqaua_queue[0].length >= 19 ) {
      *dest_message = _iqaua_queue[0].command;
      len = _iqaua_queue[0].length;
      _aqua_last_cmdfrom_queue = true;
    } else {
      LOG(IAQL_LOG,LOG_WARNING,"Next command in queue is not full command, ignoring\n");
    }
  }
  else if (source_message_type == 0x53 && _iaqua_q_length > 0) 
  { // Send small command
    if ( _iqaua_queue[0].length <= 2 ) {
      *dest_message = _iqaua_queue[0].command;
      len = _iqaua_queue[0].length;
      _aqua_last_cmdfrom_queue = true;
    } else {
      LOG(IAQL_LOG,LOG_WARNING,"Next command in queue is too large, ignoring\n");
    }
  }

  //LOG(IAQL_LOG, LOG_INFO, "Return cmd size %d\n",len);

  return len;
}

void remove_iaqualink_cmd() {
  if (_iaqua_q_length > 0 && _aqua_last_cmdfrom_queue == true) {
    LOG(IAQL_LOG,LOG_DEBUG, "Remove from message queue, length %d\n",_iaqua_q_length-1);
    memmove(&_iqaua_queue[0], &_iqaua_queue[1], (sizeof(iaqualink_cmd)) * _iaqua_q_length);
    _iaqua_q_length--;
  }
}


unsigned char iAqalnkDevID(aqkey *button) {

  // For the 3 actual vbuttons that exist, we have already set their codes so just return it with no conversion
  if ( ((button->special_mask & VIRTUAL_BUTTON) == VIRTUAL_BUTTON)  &&  button->rssd_code != NUL ) {
    return button->rssd_code;
  }

  switch (button->rssd_code) {
    case RS_SA_PUMP:
      return IAQ_PUMP;
    break;
    case RS_SA_SPA:
      return IAQ_SPA;
    break;
    case RS_SA_POOLHT:
      return IAQ_POOL_HTR;
    break;
    case RS_SA_SPAHT:
      return IAQ_SPA_HTR;
    break;
    case RS_SA_AUX1:
      return IAQ_AUX1;
    break;
    case RS_SA_AUX2:
      return IAQ_AUX2;
    break;
    case RS_SA_AUX3:
      return IAQ_AUX3;
    break;
    case RS_SA_AUX4:
      return IAQ_AUX4;
    break;
    case RS_SA_AUX5:
      return IAQ_AUX5;
    break;
    case RS_SA_AUX6:
      return IAQ_AUX6;
    break;
    case RS_SA_AUX7:
      return IAQ_AUX7;
    break;
    case RS_SA_AUX8:
      return IAQ_AUXB1;
    break;
    case RS_SA_AUX9:
      return IAQ_AUXB2;
    break;
    case RS_SA_AUX10:
      return IAQ_AUXB3;
    break;
    case RS_SA_AUX11:
      return IAQ_AUXB4;
    break;
    case RS_SA_AUX12:
      return IAQ_AUXB5;
    break;
    case RS_SA_AUX13:
      return IAQ_AUXB6;
    break;
    case RS_SA_AUX14:
      return IAQ_AUXB7;
    break; 
    case RS_SA_AUX15:
      return IAQ_AUXB8;
    break;
  }

  return 0xFF;
}


void lastchecksum(unsigned char *packet, int length){
  static unsigned char last70checksum = 0x00;
  static unsigned char last71checksum = 0x00;
  static unsigned char last72checksum = 0x00;
   
   switch (packet[PKT_CMD]){
    case 0x70:
      if (last70checksum != packet[length-3] && last70checksum != 0x00) {
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
        LOG(IAQL_LOG, LOG_NOTICE,"******* CHECKSUM CHANGED for 0x70 *******\n");
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
      }
      last70checksum = packet[length-3];
    break;
    case 0x71:
      if (last71checksum != packet[length-3] && last71checksum != 0x00) {
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
        LOG(IAQL_LOG, LOG_NOTICE,"******* CHECKSUM CHANGED for 0x71 *******\n");
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
      }
      last71checksum = packet[length-3];
    break;
    case 0x72:
      if (last72checksum != packet[length-3] && last72checksum != 0x00) {
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
        LOG(IAQL_LOG, LOG_NOTICE,"******* CHECKSUM CHANGED for 0x72 *******\n");
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
      }
      last72checksum = packet[length-3];
    break;
   }
}



/*

All taken from panel Yg, but only heater setpoints seem to work.
Only setpoints seem to work, 

RPM to 2750
Bit 6 = 0x5e 
Bit 10 * 256 + Bit 11
Bit 7 or 9 probably pump index.
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x5e|0x04|0x00|0x01|0x0a|0xbe|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xd5|0x10|0x03|

RPM to 2995
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x5e|0x04|0x00|0x01|0x0b|0xb3|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xcb|0x10|0x03|

SWG 50%
Byte 6 = 0x19
Byte 7 = 50%
Byte 9 & 10 ????
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x19|0x32|0x00|0x18|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x0e|0x10|0x03|

SWG 51%
Byte 7 = 51%
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x19|0x33|0x00|0x18|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x0f|0x10|0x03|

Spa Setpoint 102
Byte 6 = 0x06
Byte 8 = 0x66=102 
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x06|0x00|0x66|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x16|0x10|0x03|

Pool Setpoint 72
Byte 6 = 0x05
Byte 8 = 0x48=72
HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x05|0x00|0x48|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xf7|0x10|0x03|

*/

unsigned char _cmd_readyCommand[] = {0x3f, 0x20};
unsigned char _fullcmd[] = {0x00, 0x24, 0x73, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void set_iaqualink_aux_state(aqkey *button, bool isON) {
  
  _fullcmd[4] = iAqalnkDevID(button);

  if (_fullcmd[4] != 0xFF) {
    push_iaqualink_cmd(_cmd_readyCommand, 2);
    push_iaqualink_cmd(_fullcmd, 19);
  } else {
     LOG(IAQL_LOG, LOG_ERR, "Couldn't find iaqualink keycode for button %s\n",button->label);
  }

}

void set_iaqualink_heater_setpoint(int value, bool isPool) {
  

  if (isPool) {
    _fullcmd[4] = 0x05;
  } else {
    _fullcmd[4] = 0x06;
  }

  // Should check value is valid here.
  _fullcmd[6] = value;

  push_iaqualink_cmd(_cmd_readyCommand, 2);
  push_iaqualink_cmd(_fullcmd, 19);
}

/*  
    Status packets are requested on iAqualink ID 0xA? but received on AqualinkTouch ID 0x3?
    They are also sent when iAqualink is connected and a device changes.
    So good to catch in PDA mode when a physical iAqualink device is connected to PDA panel.
    packet has cmd of 0x70, 0x71, 0x72
*/
bool process_iAqualinkStatusPacket(unsigned char *packet, int length, struct aqualinkdata *aq_data)
{
  if (packet[PKT_CMD] == CMD_IAQ_MAIN_STATUS)
  {
    logPacket(IAQL_LOG, LOG_NOTICE, packet, length, true);
    int startIndex = 4 + 1;
    int numberBytes = packet[4];
    int offsetIndex = startIndex + numberBytes;
    bool foundSpaSP = false;
    bool foundWaterTemp = false;
    bool foundAirTemp = false;

    for (int i = 0; i <= numberBytes; i++)
    {
      int byteType = packet[5 + i];
      int byte = packet[offsetIndex + i];
      char *label;

      // Some panels have blanks for the last 3 buys, the first of which is "water temp" (not sure on others 0x20, 0x21)
      // So if we saw 0x1d break loop if not force next as water temp.
      if (foundWaterTemp && i == numberBytes)
      {
        break;
      }
      else if (i == numberBytes)
      {
        byteType = 0x1d;
      }

      if (byteType == 0) {
        label = "Filter Pump      ";
      } else if (byteType == 1) {
        label = "Pool Heater      "; // 0x01=on&heating, 0x03=ena
      } else if (byteType == 2) {
        label = "Spa              ";
      } else if (byteType == 3) {
        label = "Spa Heater       "; // 0x01=on&heating, 0x03=ena
      } else if (byteType == 6) {
        label = "Pool Htr setpoint";
      } else if (byteType == 8 || byteType == 9) {// 8 usually, also get 9 & 14 (different spa/heater modes not sorted out yet. 14 sometimes blank as well)
        label = "Spa Htr setpoint ";
        foundSpaSP=true;
      } else if ( (/*byteType == 14 ||*/ byteType == 12) && foundSpaSP==false && byte != 0) {
        label = "Spa Htr setpoint ";
      } else if ( (byteType == 14 || byteType == 15 || byteType == 26) && byte != 0 && byte != 255 && foundAirTemp == false ) {// 0x0f
        label = "Air Temp         ";  // we also see this as 14 (RS16) ONLY
        foundAirTemp = true;
      } else if ( (byteType == 27 || byteType == 28 || byteType == 29) && byte != 0 && byte != 255 && foundWaterTemp == false) {
       // Last 3 bytes don't have type on some panels
        label = "Water Temp       ";
        foundWaterTemp = true;
      }
      else
        label = "                 ";

      LOG(IAQL_LOG, LOG_NOTICE, "%-17s = %3d | index=%d type=(%0.2d 0x%02hhx) value=0x%02hhx offset=%d\n", label, byte, i, byteType, byteType, byte, (offsetIndex + i));
    }
    LOG(IAQL_LOG, LOG_NOTICE, "Status from other protocols Pump %s, Spa %s, SWG %d, PumpRPM %d, PoolSP=%d, SpaSP=%d, WaterTemp=%d, AirTemp=%d\n",
        aq_data->aqbuttons[0].led->state == OFF ? "Off" : "On ",
        aq_data->aqbuttons[1].led->state == OFF ? "Off" : "On ",
        aq_data->swg_percent,
        aq_data->pumps[0].rpm,
        aq_data->pool_htr_set_point,
        aq_data->spa_htr_set_point,
        (aq_data->aqbuttons[1].led->state == OFF ? aq_data->pool_temp : aq_data->spa_temp),
        aq_data->air_temp);
  }
  else if (packet[PKT_CMD] == CMD_IAQ_1TOUCH_STATUS)
  {
    logPacket(IAQL_LOG, LOG_NOTICE, packet, length, true);
    int numLabels = packet[4];
    int start = numLabels + 4 + 1;

    if (numLabels == 1)
    {
      LOG(IAQL_LOG, LOG_NOTICE, "**** !!! haven't decoded above packet yet !!! *****\n");
      return false;
    }

    for (int i = 0; i < numLabels; i++)
    {
      int status = packet[start];
      int length = packet[start + 1];
      int byteType = packet[5 + i];
      LOG(IAQL_LOG, LOG_NOTICE, "%-15.*s = %s | index %d type=(%0.2d 0x%02hhx) status=0x%02hhx start=%d length=%d\n", length, &packet[start + 2], (status == 0x00 ? "Off" : "On "), i, byteType, byteType, status, start, length);
      // Check against virtual onetouch buttons.
      for (int bi=aq_data->virtual_button_start ; bi < aq_data->total_buttons ; bi++) {
        if (rsm_strcmp((char *)&packet[start + 2], aq_data->aqbuttons[bi].label) == 0) {
          //LOG(IAQL_LOG, LOG_NOTICE, "Status for %s is %s\n",aq_data->aqbuttons[bi].label,(status == 0x00 ? "Off" : "On "));
          // == means doesn;t match, RS 1=on 0=off / LED enum 1=off 0=on
          if (aq_data->aqbuttons[bi].led->state == status) {
            LOG(IAQL_LOG, LOG_NOTICE, "Updated Status for %s is %s\n",aq_data->aqbuttons[bi].label,(status == 0x00 ? "Off" : "On "));
            aq_data->aqbuttons[bi].led->state = (status == 0x00 ? OFF:ON);
            aq_data->updated = true;
          }
        }
      }
      start = start + packet[start + 1] + 2;
    }
  }
  else if (packet[PKT_CMD] == CMD_IAQ_AUX_STATUS)
  {
    logPacket(IAQL_LOG, LOG_NOTICE, packet, length, true);
    // Look at notes in iaqualink.c for how this packet is made up
    // Since this is so similar to above CMD_IAQ_1TOUCH_STATUS, we should look at using same logic for both.
    int start = packet[4];
    start = start + 5;
    for (int i = start; i < length - 3; i = i)
    {
      int status = i;
      int labelstart = status + 5;
      int labellen = packet[status + 4];
      if (labelstart + labellen < length)
      {
        LOG(IAQL_LOG, LOG_NOTICE, "%-15.*s = %s | bit1=0x%02hhx bit2=0x%02hhx bit3=0x%02hhx bit4=0x%02hhx\n", labellen, &packet[labelstart], (packet[status] == 0x00 ? "Off" : "On "), packet[status], packet[status + 1], packet[status + 2], packet[status + 3]);
      }
      if (isPDA_PANEL) {
        for (int bi=2 ; bi < aq_data->total_buttons ; bi++) {
          if (rsm_strcmp((char *)&packet[labelstart], aq_data->aqbuttons[bi].label) == 0) {
            if (aq_data->aqbuttons[bi].led->state == packet[status]) {
              LOG(IAQL_LOG, LOG_NOTICE, "Updated Status for %s is %s\n",aq_data->aqbuttons[bi].label,(packet[status] == 0x00 ? "Off" : "On "));
              aq_data->aqbuttons[bi].led->state = (packet[status] == 0x00 ? OFF:ON);
              aq_data->updated = true;
            }
            //LOG(IAQL_LOG, LOG_NOTICE, "Match %s to %.*s state(aqd=%d pnl=%d)\n",aq_data->aqbuttons[bi].label, labellen, (char *)&packet[labelstart], aq_data->aqbuttons[bi].led->state, packet[status]);
          }
        }
      }

      i = labelstart + labellen;
    }
  }

  return true;
}

bool process_iaqualink_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data)
{

  lastchecksum(packet, length);

  unsigned char cmd_getMainstatus[] = {0x3f, 0x08};
  unsigned char cmd_getTouchstatus[] = {0x3f, 0x10};
  unsigned char cmd_getAuxstatus[] = {0x3f, 0x18};
  //unsigned char cmd_readyCommand[] = {0x3f, 0x20};
  //unsigned char fullcmd[] = {0x00, 0x24, 0x73, 0x01, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  // byte 4 is ID.  0x00 pump, 0x02 spa etc

  
  if (packet[PKT_DEST] == _aqconfig_.extended_device_id2)
  {
    static int cnt = 0;
    //static unsigned char ID = 0;
    //static cur_swg = 0;

    if (packet[PKT_CMD] == 0x53)
    {
      cnt++;
      if (cnt == 20) {
        cnt=0;
        push_iaqualink_cmd(cmd_getMainstatus, 2);
        push_iaqualink_cmd(cmd_getTouchstatus, 2);
        push_iaqualink_cmd(cmd_getAuxstatus, 2);
/*
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
        LOG(IAQL_LOG, LOG_NOTICE,"********** Send %d 0x%02hhx ************\n",ID,ID);
        LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");

        _fullcmd[4] = ID++;
        push_iaqualink_cmd(_cmd_readyCommand, 2);
        push_iaqualink_cmd(_fullcmd, 19);

        push_iaqualink_cmd(cmd_getMainstatus, 2);
        push_iaqualink_cmd(cmd_getTouchstatus, 2);
        push_iaqualink_cmd(cmd_getAuxstatus, 2);

        push_iaqualink_cmd(_cmd_readyCommand, 2);
        push_iaqualink_cmd(_fullcmd, 19);
*/

        //fullcmd[4] = ID;
        //fullcmd[4] = ID;
        //fullcmd[5] = 0x32; 
        
        //fullcmd[7] = ID; 
        //fullcmd[8] = 0x01; 
        

        //set_iaqualink_heater_setpoint(50, true);

        //push_iaqualink_cmd(cmd_getMainstatus, 2);
        //push_iaqualink_cmd(cmd_getTouchstatus, 2);
        //push_iaqualink_cmd(cmd_getAuxstatus, 2);
        /*
        if (aq_data->swg_percent != cur_swg && cur_swg != 0) {
          LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
          LOG(IAQL_LOG, LOG_NOTICE,"********** SWG Changed to %d ************\n",aq_data->swg_percent);
          LOG(IAQL_LOG, LOG_NOTICE,"*****************************************\n");
          exit(0);
        }
        cur_swg = aq_data->swg_percent;
        LOG(IAQL_LOG, LOG_NOTICE,"******* QUEUE SWG Comand of %d | 0x%02hhx *************\n",ID,ID);
        ID++;*/

      }
    }
    // Packets sent to iAqualink protocol
    // debuglogPacket(IAQL_LOG, packet, length, true, true);
  }
  else if (packet[PKT_DEST] == _aqconfig_.extended_device_id || (isPDA_PANEL && packet[PKT_DEST] == _aqconfig_.device_id) )
  {
    process_iAqualinkStatusPacket(packet, length, aq_data);
  }

  return true;
}

#ifdef DONOTCOMPILE

// This is onle here temporarly until we figure out the protocol.
void send_iaqualink_ack(int rs_fd, unsigned char *packet_buffer)
{
  /*
        Probe | HEX: 0x10|0x02|0xa3|0x00|0xb5|0x10|0x03|
          Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
       Unknown '0x61' | HEX: 0x10|0x02|0xa3|0x61|0x00|0x00|0x00|0x04|0x00|0x27|0x41|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x61|0x00|0x74|0x10|0x03|
       Unknown '0x50' | HEX: 0x10|0x02|0xa3|0x50|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x25|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x50|0x00|0x63|0x10|0x03|
       Unknown '0x51' | HEX: 0x10|0x02|0xa3|0x51|0x00|0x06|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x51|0x00|0x64|0x10|0x03|
       Unknown '0x59' | HEX: 0x10|0x02|0xa3|0x59|0x00|0x0e|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x59|0x00|0x6c|0x10|0x03|
       Unknown '0x52' | HEX: 0x10|0x02|0xa3|0x52|0x00|0x07|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x52|0x00|0x65|0x10|0x03|
       Unknown '0x53' | HEX: 0x10|0x02|0xa3|0x53|0x08|0x10|0x03|
                  Ack | HEX: 0x10|0x02|0x00|0x01|0x3f|0x00|0x52|0x10|0x03|
        Use byte 3 as return ack, except for 0x53=0x3f
        */
  if (packet_buffer[PKT_CMD] == CMD_PROBE)
  {
    LOG(IAQL_LOG, LOG_NOTICE, "Got probe on '0x%02hhx' 2nd iAqualink Protocol\n", packet_buffer[PKT_DEST]);
    send_extended_ack(rs_fd, packet_buffer[PKT_CMD], 0x00);
  }
  else if (packet_buffer[PKT_CMD] == 0x53)
  {
    static int cnt = 0;
    cnt++;
    if (cnt == 10)
    {
      //cnt = 5;
      LOG(IAQL_LOG, LOG_NOTICE, "Sent accept next packet Comand\n");
      send_extended_ack(rs_fd, 0x3f, 0x20);
    }
    if (cnt == 20)
    {
      LOG(IAQL_LOG, LOG_NOTICE, "Sending get status\n");
      send_extended_ack(rs_fd, 0x3f, 0x08);
    }
    else if (cnt == 22)
    {
      LOG(IAQL_LOG, LOG_NOTICE, "Sending get other status\n");
      send_extended_ack(rs_fd, 0x3f, 0x10);
    }
    else if (cnt == 24)
    {
      LOG(IAQL_LOG, LOG_NOTICE, "Sending get aux button status\n");
      send_extended_ack(rs_fd, 0x3f, 0x18);
    }
    else
    {
      // Use 0x3f
      if (cnt > 24)
      {
        cnt = 0;
      }
      send_extended_ack(rs_fd, 0x3f, 0x00);
    }
    // send_jandy_command(rs_fd, get_rssa_cmd(packet_buffer[PKT_CMD]), 4);
  }
  else if (packet_buffer[PKT_CMD] == 0x73)
  {
    static int id = 10;
    // unsigned char pb1[] = {PCOL_JANDY,0x10,0x02,0x00,0x24,0x73,0x01,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc2,0x10,0x03,0x00};
    // unsigned char pb2[] = {PCOL_JANDY,0x10,0x02,0x00,0x24,0x73,0x01,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x10,0x03,0x00};
    // 0x21 turns on filter_pump and aux 1
    //unsigned char pb3[] = {0x00, 0x24, 0x73, 0x01, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char swg[] = {0x00, 0x24, 0x73, 0x01, 0x19, 0x32, 0x00, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //unsigned char swg[] = {0x00, 0x24, 0x73, 0x01, 0x19, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char rpm[] = {0x00,0x24,0x73,0x01,0x5e,0x04,0x00,0x01,0x0a,0xbe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xd5,0x10,0x03};
    //pb3[4] = id++;
    //swg[5] = ++id;

    LOG(IAQL_LOG, LOG_NOTICE, "*** Sending SWG dec=%d hex=0x%02hhx\n", swg[5], swg[5]);
    // send_packet(rs_fd, pb2, 25);
    send_jandy_command(rs_fd, swg, 19);
  }
  else
  {
    // Use packet_buffer[PKT_CMD]
    send_extended_ack(rs_fd, packet_buffer[PKT_CMD], 0x00);
  }
}
#endif

/*
----
Retrieve poll 0x53
Reply ack w/ command  0x01|0x3f|0x20. (0x20 command)
Retrieve 0x73 command Same ID iAqualink
Reply iAQ pButton ( turn something on / set something)

Retrieve poll 0x53
Reply ack w/ command  0x01|0x3f|0x08. (0x08 command)
Retrieve 0x70 command on AqualinkTouch ID
Reply iAQ pButton (status / last part of frame is board cpu)
-- Byte 4 = offset.
-- Byte 21 look like spa heat 0x00=off, 0x03=enabled
-- offest+ 8 spa setpoint
-- offset+ 10 air temp
-- offest+ 13 pool temp
-- offset+ 14 SWG% ?????
-- Byte 27 looks like air temp
-- Byte 28 air temp????
-- Byte 30 pool temp
-- Byte 34 looks like SWG%
-- after byte 0xff is board

Retrieve poll 0x53
Reply ack w/ command  0x01|0x3f|0x10. (0x10 command)
Retrieve 0x71 command on AqualinkTouch ID
Reply iAQ pButton (not sure)
    Byte 4 & 5 seems to indicate a different sub type
    0x71|0x01|0x19 = status about pool / spa / speed 1,2,3,4 / pool & spa heat
    0x71|0x01|0x1a = no idea
    0x71|0x01|0x17 = Pump numbers (doesn't seem to have any status, just pump names)

    0x71|0x06|0x01 = Touch like spa mode / all off
    0x71|0x07|0x01 = Same as above. but more trailing packets.

Retrieve poll 0x53
Reply ack w/ command   0x3f 0x18.  (0x18 is command)
Retrieve 0x72 command on AqualinkTouch ID
Get a massive packet Aux status

----

Retrieve poll 0x53
Reply ack w/ command  0x01|0x3f|0x20. (0x20 command)
Retrieve 0x72 command
Reply iAQ pButton (turn something on / set something).  Below are examples.

RPM to 2750
Bit 10 * 256 + Bit 11
Bit 6 & 7 probably pump index.
 HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x5e|0x04|0x00|0x01|0x0a|0xbe|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xd5|0x10|0x03|

RPM to 2995
 HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x5e|0x04|0x00|0x01|0x0b|0xb3|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xcb|0x10|0x03|

SWG 50%
Byte 7 = 50%
 HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x19|0x32|0x00|0x18|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x0e|0x10|0x03|

SWG 51%
Byte 7 = 51%
 HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x19|0x33|0x00|0x18|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x0f|0x10|0x03|

---------

Send command 0x18 to 0xa1 returns below to 0x31.  Looks like status of aux devices.
Retrieve poll 0x53
Reply ack w/ command   0x3f 0x18.  (0x18 is command)
Get a massive packet back, of aux status

Filter pump does not effect the below.
Heater on/off does not effect below.
HEater setpoints does not effect.
Only Aux on/off seem to effect the status.

4th bit tells you where to start,
  after start 4 bits are status
  last bit of status tell you chars in label.
  after label repeat start 4 bits.

For Aux1 10th bit 0=off 1=on 11 to 14  15 to 19=name
    Aux2 19th
    Aux3 28
    Aux4 37

In Below Aux 3 color light / Aux 4 dimmer / Aux 5 color light (different type to aux3)

Aux 1 off / Aux 3 on
  HEX: 0x10|0x02|0x31|0x72|0x05|0x01|0x02|0x03|0x04|0x05|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x31|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x32|0x01|0x07|0x02|0x03|0x04|0x41|0x75|0x78|0x33|0x00|0x01|0x01|0x00|0x04|0x41|0x75|0x78|0x34|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x35|0xd5|0x10|0x03|
Dec   |  16|   2|  49| 114|   5|   1|   2|   3|   4|   5|   0|   1|   0|   0|   4|  65| 117| 120|  49|   1|   1|   0|   0|   4|  65| 117| 120|  50|   1|   7|   2|   3|   4|  65| 117| 120|  51|   0|   1|   1|   0|   4|  65| 117| 120|  52|   0|   1|   0|   0|   4|  65| 117| 120|  53| 213|  16|   3
Ascii |    |    |   1|   r|    |    |    |    |    |    |    |    |    |    |    |   A|   u|   x|   1|    |    |    |    |    |   A|   u|   x|   2|    |    |    |    |    |   A|   u|   x|   3|    |    |    |    |    |   A|   u|   x|   4|    |    |    |    |    |   A|   u|   x|   5|    |    |

Aux 1 on / Aux 3 on
| HEX: 0x10|0x02|0x31|0x72|0x05|0x01|0x02|0x03|0x04|0x05|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x31|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x32|0x01|0x07|0x02|0x03|0x04|0x41|0x75|0x78|0x33|0x00|0x01|0x01|0x00|0x04|0x41|0x75|0x78|0x34|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x35|0xd6|0x10|0x03|
Dec   |  16|   2|  49| 114|   5|   1|   2|   3|   4|   5|   1|   1|   0|   0|   4|  65| 117| 120|  49|   1|   1|   0|   0|   4|  65| 117| 120|  50|   1|   7|   2|   3|   4|  65| 117| 120|  51|   0|   1|   1|   0|   4|  65| 117| 120|  52|   0|   1|   0|   0|   4|  65| 117| 120|  53| 214|  16|   3
Ascii |    |    |   1|   r|    |    |    |    |    |    |    |    |    |    |    |   A|   u|   x|   1|    |    |    |    |    |   A|   u|   x|   2|    |    |    |    |    |   A|   u|   x|   3|    |    |    |    |    |   A|   u|   x|   4|    |    |    |    |    |   A|   u|   x|   5|    |    |

Aux 1 off Aux3 to different light color
Hex   |0x10|0x02|0x31|0x72|0x05|0x01|0x02|0x03|0x04|0x05|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x31|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x32|0x00|0x07|0x02|0x03|0x04|0x41|0x75|0x78|0x33|0x00|0x01|0x01|0x00|0x04|0x41|0x75|0x78|0x34|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x35|0xd4|0x10|0x03|
Dec   |  16|   2|  49| 114|   5|   1|   2|   3|   4|   5|   0|   1|   0|   0|   4|  65| 117| 120|  49|   1|   1|   0|   0|   4|  65| 117| 120|  50|   0|   7|   2|   3|   4|  65| 117| 120|  51|   0|   1|   1|   0|   4|  65| 117| 120|  52|   0|   1|   0|   0|   4|  65| 117| 120|  53| 212|  16|   3
Ascii |    |    |   1|   r|    |    |    |    |    |    |    |    |    |    |    |   A|   u|   x|   1|    |    |    |    |    |   A|   u|   x|   2|    |    |    |    |    |   A|   u|   x|   3|    |    |    |    |    |   A|   u|   x|   4|    |    |    |    |    |   A|   u|   x|   5|    |    |



RS16 panel.
  HEX: 0x10|0x02|0x31|0x72|0x0f|0x01|0x02|0x03|0x04|0x05|0x06|0x07|0x08|0x09|0x0a|0x0b|0x0c|0x0d|0x0e|0x0f|0x00|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x31|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x32|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x33|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x34|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x35|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x36|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x37|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x31|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x32|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x33|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x34|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x35|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x36|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x37|0x01|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x38|0x77|0x10|0x03|
Dec   |  16|   2|  49| 114|  15|   1|   2|   3|   4|   5|   6|   7|   8|   9|  10|  11|  12|  13|  14|  15|   0|   1|   0|   0|   4|  65| 117| 120|  49|   1|   1|   0|   0|   4|  65| 117| 120|  50|   1|   1|   0|   0|   4|  65| 117| 120|  51|   1|   1|   0|   0|   4|  65| 117| 120|  52|   1|   1|   0|   0|   4|  65| 117| 120|  53|   1|   1|   0|   0|   4|  65| 117| 120|  54|   1|   1|   0|   0|   4|  65| 117| 120|  55|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  49|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  50|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  51|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  52|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  53|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  54|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  55|   1|   1|   0|   0|   6|  65| 117| 120|  32|  66|  56| 119|  16|   3
Ascii |    |    |   1|   r|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |   A|   u|   x|   1|    |    |    |    |    |   A|   u|   x|   2|    |    |    |    |    |   A|   u|   x|   3|    |    |    |    |    |   A|   u|   x|   4|    |    |    |    |    |   A|   u|   x|   5|    |    |    |    |    |   A|   u|   x|   6|    |    |    |    |    |   A|   u|   x|   7|    |    |    |    |    |   A|   u|   x|    |   B|   1|    |    |    |    |    |   A|   u|   x|    |   B|   2|    |    |    |    |    |   A|   u|   x|    |   B|   3|    |    |    |    |    |   A|   u|   x|    |   B|   4|    |    |    |    |    |   A|   u|   x|    |   B|   5|    |    |    |    |    |   A|   u|   x|    |   B|   6|    |    |    |    |    |   A|   u|   x|    |   B|   7|    |    |    |    |    |   A|   u|   x|    |   B|   8|   w|    |

Hex   |0x10|0x02|0x31|0x72|

*/

/* Startup sequences

RS16 combo
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type            Probe | HEX: 0x10|0x02|0xa1|0x00|0xb3|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x61' | HEX: 0x10|0x02|0xa1|0x61|0x00|0x00|0x00|0x01|0x00|0x1d|0x32|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x61|0x00|0x74|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x50' | HEX: 0x10|0x02|0xa1|0x50|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x23|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x50|0x00|0x63|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x51' | HEX: 0x10|0x02|0xa1|0x51|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x00|0xa2|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x51|0x00|0x64|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x59' | HEX: 0x10|0x02|0xa1|0x59|0x00|0x0c|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x59|0x00|0x6c|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x52' | HEX: 0x10|0x02|0xa1|0x52|0x00|0x05|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x52|0x00|0x65|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type     iAqalnk Poll | HEX: 0x10|0x02|0xa1|0x53|0x06|0x10|0x03|

RS8 Combo rev T.2
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type            Probe | HEX: 0x10|0x02|0xa1|0x00|0xb3|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x61' | HEX: 0x10|0x02|0xa1|0x61|0x00|0x00|0x00|0x01|0x00|0x1d|0x32|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x61|0x00|0x74|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x50' | HEX: 0x10|0x02|0xa1|0x50|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x23|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x50|0x00|0x63|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x51' | HEX: 0x10|0x02|0xa1|0x51|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x00|0xa2|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x51|0x00|0x64|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x59' | HEX: 0x10|0x02|0xa1|0x59|0x00|0x0c|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x59|0x00|0x6c|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x52' | HEX: 0x10|0x02|0xa1|0x52|0x00|0x05|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x52|0x00|0x65|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type     iAqalnk Poll | HEX: 0x10|0x02|0xa1|0x53|0x06|0x10|0x03|

RS4 rev Yg
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type            Probe | HEX: 0x10|0x02|0xa3|0x00|0xb5|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x61' | HEX: 0x10|0x02|0xa3|0x61|0x00|0x00|0x00|0x04|0x00|0x27|0x41|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x61|0x00|0x74|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x50' | HEX: 0x10|0x02|0xa3|0x50|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x25|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x50|0x00|0x63|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x51' | HEX: 0x10|0x02|0xa3|0x51|0x00|0x06|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x51|0x00|0x64|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x59' | HEX: 0x10|0x02|0xa3|0x59|0x00|0x0e|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x59|0x00|0x6c|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x52' | HEX: 0x10|0x02|0xa3|0x52|0x00|0x07|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x52|0x00|0x65|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0xa3 of type   Unknown '0x53' | HEX: 0x10|0x02|0xa3|0x53|0x08|0x10|0x03|
Notice: Serial Log:Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x3f|0x00|0x52|0x10|0x03|

RS6 rev T2 RS8 (home)
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type            Probe | HEX: 0x10|0x02|0xa1|0x00|0xb3|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x61' | HEX: 0x10|0x02|0xa1|0x61|0x00|0x00|0x00|0x01|0x00|0x1d|0x32|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x61|0x00|0x74|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x50' | HEX: 0x10|0x02|0xa1|0x50|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x23|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x50|0x00|0x63|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x51' | HEX: 0x10|0x02|0xa1|0x51|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x00|0xa2|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x51|0x00|0x64|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x59' | HEX: 0x10|0x02|0xa1|0x59|0x01|0x0d|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x59|0x00|0x6c|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type   Unknown '0x52' | HEX: 0x10|0x02|0xa1|0x52|0x03|0x08|0x10|0x03|
Debug:   RS Serial: Write Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x52|0x00|0x65|0x10|0x03|
Debug:   RS Serial: Read  Jandy   packet To 0xa1 of type     iAqalnk Poll | HEX: 0x10|0x02|0xa1|0x53|0x06|0x10|0x03|
*/


/*


0x10|0x02|0x31|0x72|0x0f|0x01|0x02|0x03|0x04|0x05|0x06|0x07|0x08|0x09|0x0a|0x0b|0x0c|0x0d|0x0e|0x0f|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x31|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x32|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x33|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x34|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x35|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x36|0x01|0x01|0x00|0x00|0x04|0x41|0x75|0x78|0x37|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x31|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x32|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x33|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x34|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x35|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x36|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x37|0x00|0x01|0x00|0x00|0x06|0x41|0x75|0x78|0x20|0x42|0x38|0x70|0x10|0x03|


Only panel (RS8)
Air   index 3 type 15 (pump off)
Air   index 3 type 14 (pump off)
Air   index 3 type  8 (pump on)
Water index 5 type 15 (pump on)
Temp2 ndex  8 type 20 
always 1 | index=4 type=(15 0x0f)

Comb0
Spa setpoint index 8 type 14
Air          index 8 type 26
*/
