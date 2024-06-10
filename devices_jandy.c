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

#include "devices_jandy.h"
#include "aq_serial.h"
#include "aqualink.h"
#include "utils.h"
#include "aq_mqtt.h"
#include "packetLogger.h"

/*
  All button errors
  'Check AQUAPURE No Flow'
  'Check AQUAPURE Low Salt'
  'Check AQUAPURE High Salt'
  'Check AQUAPURE General Fault'
*/

static int _swg_noreply_cnt = 0;

bool processJandyPacket(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  static rsDeviceType interestedInNextAck = DRS_NONE;
  static unsigned char previous_packet_to = NUL; // bad name, it's not previous, it's previous that we were interested in.
  int rtn = false;

  // We received the ack from a Jandy device we are interested in
  if (packet_buffer[PKT_DEST] == DEV_MASTER && interestedInNextAck != DRS_NONE)
  {
    if (interestedInNextAck == DRS_SWG)
    {
      rtn = processPacketFromSWG(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_EPUMP)
    {
      rtn = processPacketFromJandyPump(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_JXI)
    {
      rtn = processPacketFromJandyJXiHeater(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_LX)
    {
      rtn = processPacketFromJandyLXHeater(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_CHEM)
    {
      rtn = processPacketFromJandyChemFeeder(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
  // We were expecting an ack from Jandy device but didn't receive it.
  else if (packet_buffer[PKT_DEST] != DEV_MASTER && interestedInNextAck != DRS_NONE)
  {
    if (interestedInNextAck == DRS_SWG && aqdata->ar_swg_device_status != SWG_STATUS_OFF)
    { // SWG Offline
      processMissingAckPacketFromSWG(previous_packet_to, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_EPUMP)
    { // ePump offline
      processMissingAckPacketFromJandyPump(previous_packet_to, aqdata, previous_packet_to);
    }
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
  else if (READ_RSDEV_SWG && packet_buffer[PKT_DEST] >= JANDY_DEC_SWG_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_SWG_MAX)
  {
    interestedInNextAck = DRS_SWG;
    rtn = processPacketToSWG(packet_buffer, packet_length, aqdata, _aqconfig_.swg_zero_ignore);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_ePUMP && packet_buffer[PKT_DEST] >= JANDY_DEC_PUMP_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_PUMP_MAX)
  {
    interestedInNextAck = DRS_EPUMP;
    rtn = processPacketToJandyPump(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_JXI && packet_buffer[PKT_DEST] >= JANDY_DEC_JXI_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_JXI_MAX)
  {
    interestedInNextAck = DRS_JXI;
    rtn = processPacketToJandyJXiHeater(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_LX && packet_buffer[PKT_DEST] >= JANDY_DEC_LX_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_LX_MAX)
  {
    interestedInNextAck = DRS_LX;
    rtn = processPacketToJandyLXHeater(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_CHEM && packet_buffer[PKT_DEST] >= JANDY_DEC_CHEM_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_CHEM_MAX)
  {
    interestedInNextAck = DRS_CHEM;
    rtn = processPacketToJandyChemFeeder(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else
  {
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
/*
  if (packet_buffer[PKT_CMD] != CMD_PROBE && getLogLevel(DJAN_LOG) >= LOG_DEBUG) {
    char msg[1000];
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "Jandy : %s\n", msg);
  }
*/
  return rtn;
}

bool processPacketToSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata, int swg_zero_ignore) {
  static int swg_zero_cnt = 0;
  bool changedAnything = false;

    // Only log if we are jandy debug move and not serial (otherwise it'll print twice)
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char buff[1024];
    beautifyPacket(buff, packet, packet_length, true);
    LOG(DJAN_LOG,LOG_DEBUG, "To     SWG: %s", buff);
  }

  // Only read message from controller to SWG to set SWG Percent if we are not programming, as we might be changing this
  if (packet[3] == CMD_PERCENT && aqdata->active_thread.thread_id == 0 && packet[4] != 0xFF) {
    // In service or timeout mode SWG set % message is very strange. AR %% | HEX: 0x10|0x02|0x50|0x11|0xff|0x72|0x10|0x03|
    // Not really sure what to do with this, just ignore 0xff / 255 for the moment. (if statment above)

    // SWG can get ~10 messages to set to 0 then go back again for some reason, so don't go to 0 until 10 messages are received
    if (swg_zero_cnt <= swg_zero_ignore && packet[4] == 0x00) {
      LOG(DJAN_LOG, LOG_DEBUG, "Ignoring SWG set to %d due to packet packet count %d <= %d from control panel to SWG 0x%02hhx 0x%02hhx\n", (int)packet[4],
                 swg_zero_cnt, swg_zero_ignore, packet[4], packet[5]);
      swg_zero_cnt++;
    } else if (swg_zero_cnt > swg_zero_ignore && packet[4] == 0x00) {
      if (aqdata->swg_percent != (int)packet[4]) {
        //aqdata->swg_percent = (int)packet[4];
        setSWGpercent(aqdata, (int)packet[4]);
        changedAnything = true;
        aqdata->updated = true;
        LOG(DJAN_LOG, LOG_DEBUG, "Set SWG %% to %d from reading control panel packet sent to SWG (received %d messages)\n", aqdata->swg_percent, swg_zero_cnt);
      }
      // LOG(DJAN_LOG, LOG_DEBUG, "SWG set to %d due to packet packet count %d <= %d from control panel to SWG 0x%02hhx 0x%02hhx\n",
      // (int)packet[4],swg_zero_cnt,SWG_ZERO_IGNORE_COUNT,packet[4],packet[5]);  swg_zero_cnt++;
    } else {
      swg_zero_cnt = 0;
      if (aqdata->swg_percent != (int)packet[4]) {
        //aqdata->swg_percent = (int)packet[4];
        setSWGpercent(aqdata, (int)packet[4]);
        changedAnything = true;
        aqdata->updated = true;
        LOG(DJAN_LOG, LOG_DEBUG, "Set SWG %% to %d from control panel packet to SWG\n", aqdata->swg_percent);
      }
      // LOG(DJAN_LOG, LOG_DEBUG, "SWG set to %d due to packet from control panel to SWG 0x%02hhx 0x%02hhx\n",
      // aqdata.swg_percent,packet[4],packet[5]);
    }

    if (aqdata->swg_percent > 100)
      aqdata->boost = true;
    else
      aqdata->boost = false;
  }
  return changedAnything;
}

unsigned char _SWG_ID = NUL;

bool processPacketFromSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to) {
  bool changedAnything = false;
  _swg_noreply_cnt = 0;

  // Capture the SWG ID.  We could have more than one, but for the moment AqualinkD only supports one so we'll pick the first one.
  if (_SWG_ID == NUL) {
    _SWG_ID = previous_packet_to;
  } else if (_SWG_ID != NUL && _SWG_ID != previous_packet_to) {
    LOG(DJAN_LOG, LOG_WARNING, "We have two SWG, AqualinkD only supports one. using ID 0x%02hhx, ignoring 0x%02hhx\n", _SWG_ID, previous_packet_to);
    return changedAnything;
  }

  // Only log if we are jandy debug move and not serial (otherwise it'll print twice)
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char buff[1024];
    beautifyPacket(buff, packet, packet_length, true);
    LOG(DJAN_LOG,LOG_DEBUG, "From   SWG: %s", buff);
  }

  if (packet[PKT_CMD] == CMD_PPM) {
    //aqdata->ar_swg_device_status = packet[5];
    setSWGdeviceStatus(aqdata, JANDY_DEVICE, packet[5]);
    if (aqdata->swg_delayed_percent != TEMP_UNKNOWN && aqdata->ar_swg_device_status == SWG_STATUS_ON) { // We have a delayed % to set.
      char sval[10];
      snprintf(sval, 9, "%d", aqdata->swg_delayed_percent);
      aq_programmer(AQ_SET_SWG_PERCENT, sval, aqdata);
      LOG(DJAN_LOG, LOG_NOTICE, "Setting SWG %% to %d, from delayed message\n", aqdata->swg_delayed_percent);
      aqdata->swg_delayed_percent = TEMP_UNKNOWN;
    }

    if ( (packet[4] * 100) != aqdata->swg_ppm ) {
      aqdata->swg_ppm = packet[4] * 100;
      LOG(DJAN_LOG, LOG_DEBUG, "Set SWG PPM to %d from SWG packet\n", aqdata->swg_ppm);
      changedAnything = true;
      aqdata->updated = true;
    }
    // logMessage(LOG_DEBUG, "Read SWG PPM %d from ID 0x%02hhx\n", aqdata.swg_ppm, SWG_DEV_ID);
  }

  return changedAnything;
}

void processMissingAckPacketFromSWG(unsigned char destination, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  // SWG_STATUS_UNKNOWN means we have never seen anything from SWG, so leave as is. 
  // IAQTOUCH & ONETOUCH give us AQUAPURE=0 but ALLBUTTON doesn't, so only turn off if we are not in extra device mode.
  // NSF Need to check that we actually use 0 from IAQTOUCH & ONETOUCH
  if (_SWG_ID != previous_packet_to) {
    //LOG(DJAN_LOG, LOG_DEBUG, "Ignoring SWG no reply from 0x%02hhx\n", previous_packet_to);
    return;
  }

  if ( aqdata->ar_swg_device_status != SWG_STATUS_UNKNOWN && isIAQT_ENABLED == false && isONET_ENABLED == false )
  { 
    if ( _swg_noreply_cnt < 3 ) {
              //_aqualink_data.ar_swg_device_status = SWG_STATUS_OFF;
              //_aqualink_data.updated = true;
      setSWGoff(aqdata);
      _swg_noreply_cnt++; // Don't put in if, as it'll go past size limit
    }
  }     
}

bool isSWGDeviceErrorState(unsigned char status)
{
  if (status == SWG_STATUS_NO_FLOW ||
      status == SWG_STATUS_CHECK_PCB ||
      status == SWG_STATUS_LOW_TEMP ||
      status == SWG_STATUS_HIGH_CURRENT ||
      status == SWG_STATUS_NO_FLOW)
      // Maybe add CLEAN_CELL and GENFAULT here
    return true;
  else
    return false;
}

void setSWGdeviceStatus(struct aqualinkdata *aqdata, emulation_type requester, unsigned char status) {
  static unsigned char last_status = SWG_STATUS_UNKNOWN;
  /* This is only needed for DEBUG
  static bool haveSeenRSSWG = false;

  if (requester == JANDY_DEVICE) {
    haveSeenRSSWG = true;
  }
  */
   // If we are reading state directly from RS458, then ignore everything else.
  //if ( READ_RSDEV_SWG && requester != JANDY_DEVICE ) {
  //  return;
  //}

  if ((aqdata->ar_swg_device_status == status) || (last_status == status)) {
    //LOG(DJAN_LOG, LOG_DEBUG, "Set SWG device state to '0x%02hhx', request from %d\n", aqdata->ar_swg_device_status, requester);
    return;
  }
  last_status = status;

  // If we get (ALLBUTTON, SWG_STATUS_CHECK_PCB // GENFAULT), it sends this for many status, like clean cell.
  // So if we are in one of those states, don't use it.

  // Need to rethink this.  Use general fault only if we are not reading SWG status direct from device
  //if ( READ_RSDEV_SWG && requester == ALLBUTTON && status == SWG_STATUS_GENFAULT ) {

  // SWG_STATUS_GENFAULT is shown on panels for many reasons, if we are NOT reading the status directly from the SWG
  // then use it, otherwise disguard it as we will have a better status
  if (requester == ALLBUTTON && status == SWG_STATUS_GENFAULT ) {
    if (aqdata->ar_swg_device_status > SWG_STATUS_ON && 
        aqdata->ar_swg_device_status < SWG_STATUS_TURNING_OFF) {
          LOG(DJAN_LOG, LOG_DEBUG, "Ignoring set SWG device state to '0x%02hhx', request from %d\n", aqdata->ar_swg_device_status, requester);
          return;
        }
  }

  // Check validity of status and set as appropiate
  switch (status) {

  case SWG_STATUS_ON:
  case SWG_STATUS_NO_FLOW:
  case SWG_STATUS_LOW_SALT:
  case SWG_STATUS_HI_SALT:
  case SWG_STATUS_HIGH_CURRENT:
  case SWG_STATUS_CLEAN_CELL:
  case SWG_STATUS_LOW_VOLTS:
  case SWG_STATUS_LOW_TEMP:
  case SWG_STATUS_CHECK_PCB:
  case SWG_STATUS_GENFAULT:
    aqdata->ar_swg_device_status = status;
    aqdata->swg_led_state = isSWGDeviceErrorState(status)?ENABLE:ON;
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
  case SWG_STATUS_TURNING_OFF:
    aqdata->ar_swg_device_status = status;
    aqdata->swg_led_state = OFF;
    break;
  default:
    LOG(DJAN_LOG, LOG_WARNING, "Ignoring set SWG device to state '0x%02hhx', state is unknown\n", status);
    return;
    break;
  }

  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG device state to '0x%02hhx', request from %d, LED state = %d\n", aqdata->ar_swg_device_status, requester, aqdata->swg_led_state);
}


/*
bool updateSWG(struct aqualinkdata *aqdata, emulation_type requester, aqledstate state, int percent)
{
  switch (requester) {
    case ALLBUTTON: // no insight into 0% (just blank)
    break;
    case ONETOUCH:
    break;
    case IAQTOUCH:
    break;
    case AQUAPDA:
    break;
    case JANDY_DEVICE:
    break;
  }
}
*/

bool setSWGboost(struct aqualinkdata *aqdata, bool on) {
  if (!on) {
    aqdata->boost = false;
    aqdata->boost_msg[0] = '\0';
    aqdata->swg_percent = 0;
  } else {
    aqdata->boost = true;
    aqdata->swg_percent = 101;
    aqdata->swg_led_state = ON;
  }

  return true;
}

// Only change SWG percent if we are not in SWG programming
bool changeSWGpercent(struct aqualinkdata *aqdata, int percent) {
  
  if (in_swg_programming_mode(aqdata)) {
    LOG(DJAN_LOG, LOG_DEBUG, "Ignoring set SWG %% to %d due to programming SWG\n", aqdata->swg_percent);
    return false;
  }

  setSWGpercent(aqdata, percent);
  return true;
}

void setSWGoff(struct aqualinkdata *aqdata) {
  if (aqdata->ar_swg_device_status != SWG_STATUS_OFF || aqdata->swg_led_state != OFF)
    aqdata->updated = true;

  aqdata->ar_swg_device_status = SWG_STATUS_OFF;
  aqdata->swg_led_state = OFF;

  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG to off\n");
}

void setSWGenabled(struct aqualinkdata *aqdata) {
  if (aqdata->swg_led_state != ENABLE) {
    aqdata->updated = true;
    aqdata->swg_led_state = ENABLE;
    LOG(DJAN_LOG, LOG_DEBUG, "Set SWG to Enable\n");
  }
}

// force a Change SWG percent.
void setSWGpercent(struct aqualinkdata *aqdata, int percent) {
 
  aqdata->swg_percent = percent;
  aqdata->updated = true;

  if (aqdata->swg_percent > 0) {
    //LOG(DJAN_LOG, LOG_DEBUG, "swg_led_state=%d, swg_led_state=%d, isSWGDeviceErrorState=%d, ar_swg_device_status=%d\n",aqdata->swg_led_state, aqdata->swg_led_state, isSWGDeviceErrorState(aqdata->ar_swg_device_status),aqdata->ar_swg_device_status);
    if (aqdata->swg_led_state == OFF || (aqdata->swg_led_state == ENABLE && ! isSWGDeviceErrorState(aqdata->ar_swg_device_status)) ) // Don't change ENABLE / FLASH
      aqdata->swg_led_state = ON;
    
    if (aqdata->ar_swg_device_status == SWG_STATUS_UNKNOWN)
      aqdata->ar_swg_device_status = SWG_STATUS_ON; 
  
  } if ( aqdata->swg_percent == 0 ) {
    if (aqdata->swg_led_state == ON)
      aqdata->swg_led_state = ENABLE; // Don't change OFF 
    
    if (aqdata->ar_swg_device_status == SWG_STATUS_UNKNOWN)
      aqdata->ar_swg_device_status = SWG_STATUS_ON; // Maybe this should be off
  }

  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG %% to %d, LED=%d, FullStatus=0x%02hhx\n", aqdata->swg_percent, aqdata->swg_led_state, aqdata->ar_swg_device_status);
}

aqledstate get_swg_led_state(struct aqualinkdata *aqdata)
{
  switch (aqdata->ar_swg_device_status) {
  
  case SWG_STATUS_ON:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_NO_FLOW:
    return ENABLE;
    break;
  case SWG_STATUS_LOW_SALT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_HI_SALT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_HIGH_CURRENT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_TURNING_OFF:
    return OFF;
    break;
  case SWG_STATUS_CLEAN_CELL:
    return (aqdata->swg_percent > 0?ON:ENABLE);
      return ENABLE;
    break;
  case SWG_STATUS_LOW_VOLTS:
    return ENABLE;
    break;
  case SWG_STATUS_LOW_TEMP:
    return ENABLE;
    break;
  case SWG_STATUS_CHECK_PCB:
    return ENABLE;
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
    return OFF;
    break;
  case SWG_STATUS_GENFAULT:
    return ENABLE;
  break;
  default:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  }
}

void get_swg_status_msg(struct aqualinkdata *aqdata, char *message)
{
  int tmp1;
  int tmp2;

  return get_swg_status_mqtt(aqdata, message, &tmp1, &tmp2);
}

void get_swg_status_mqtt(struct aqualinkdata *aqdata, char *message, int *status, int *dzalert) 
{
  switch (aqdata->ar_swg_device_status) {
  // Level = (0=gray, 1=green, 2=yellow, 3=orange, 4=red)
  case SWG_STATUS_ON:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE GENERATING CHLORINE");
    *dzalert = 1;
    break;
  case SWG_STATUS_NO_FLOW:
    *status = SWG_OFF;
    sprintf(message, "AQUAPURE NO FLOW");
    *dzalert = 2;
    break;
  case SWG_STATUS_LOW_SALT:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE LOW SALT");
    *dzalert = 2;
    break;
  case SWG_STATUS_HI_SALT:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE HIGH SALT");
    *dzalert = 3;
    break;
  case SWG_STATUS_HIGH_CURRENT:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE HIGH CURRENT");
    *dzalert = 4;
    break;
  case SWG_STATUS_TURNING_OFF:
    *status = SWG_OFF;
    sprintf(message, "AQUAPURE TURNING OFF");
    *dzalert = 0;
    break;
  case SWG_STATUS_CLEAN_CELL:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE CLEAN CELL");
    *dzalert = 2;
    break;
  case SWG_STATUS_LOW_VOLTS:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE LOW VOLTAGE");
    *dzalert = 3;
    break;
  case SWG_STATUS_LOW_TEMP:
    *status = SWG_OFF;
    sprintf(message, "AQUAPURE WATER TEMP LOW");
    *dzalert = 2;
    break;
  case SWG_STATUS_CHECK_PCB:
    *status = SWG_OFF;
    sprintf(message, "AQUAPURE CHECK PCB");
    *dzalert = 4;
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
    *status = SWG_OFF;
    sprintf(message, "AQUAPURE OFF");
    *dzalert = 0;
    break;
  case SWG_STATUS_GENFAULT:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE GENERAL FAULT");
    *dzalert = 2;
    break;
  default:
    *status = (aqdata->swg_percent > 0?SWG_ON:SWG_OFF);
    sprintf(message, "AQUAPURE UNKNOWN STATUS");
    *dzalert = 4;
    break;
  }
}


#define EP_HI_B_WAT 8
#define EP_LO_B_WAT 7
#define EP_HI_B_RPM 7
#define EP_LO_B_RPM 6

bool processPacketToJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  /*
  Set & Sataus Watts.  Looks like send to ePump type 0x45, return type 0xf1|0x45
  JandyDvce: To   ePump:  Read To 0x78 of type   Unknown '0x45' | HEX: 0x10|0x02|0x78|0x45|0x00|0x05|0xd4|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|0x45|0x00|0x05|0x1d|0x05|0x9d|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|  69|   0|   5|  29|   5|0x9d|0x10|0x03| (Decimal)
  Type 0x1F and cmd 0x45 is Watts = 5 * (256) + 29 = 1309  or  Byte 8 * 265 + Byte 7
  */
 /*
  Set & Sataus RPM.  Looks like send to ePump type 0x44, return type 0xf1|0x44
  JandyDvce: To   ePump:  Read To 0x78 of type   Unknown '0x44' | HEX: 0x10|0x02|0x78|0x44|0x00|0x60|0x27|0x55|0x10|0x03|  
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|0x44|0x00|0x60|0x27|0x00|0xfc|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|  68|   0|  96|  39|   0|0xfc|0x10|0x03| (Decimal)
  PDA:       PDA Menu Line 3 = SET TO 2520 RPM 

  Type 0x1F and cmd 0x45 is RPM = 39 * (256) + 96 / 4 = 2520  or  Byte 8 * 265 + Byte 7 / 4
 */
   // Only log if we are jandy debug move and not serial (otherwise it'll print twice)
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char msg[1000];
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "To   ePump: %s\n", msg);
  }

  // If type 0x45 and 0x44 set to interested in next command.
  if (packet_buffer[3] == CMD_EPUMP_RPM) {
    // All we need to do is set we are interested in next packet, but ca   lling function already did this.
    LOG(DJAN_LOG, LOG_DEBUG, "ControlPanel request Pump ID 0x%02hhx set RPM to %d\n",packet_buffer[PKT_DEST], ( (packet_buffer[EP_HI_B_RPM-1] * 256) + packet_buffer[EP_LO_B_RPM-1]) / 4 );
  } else if (packet_buffer[3] == CMD_EPUMP_WATTS) {
    LOG(DJAN_LOG, LOG_DEBUG, "ControlPanel request Pump ID 0x%02hhx get watts\n",packet_buffer[PKT_DEST]);
  }
    
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG) {
  //find pump for message
    for (int i=0; i < aqdata->num_pumps; i++) {
      if (aqdata->pumps[i].pumpID == packet_buffer[PKT_DEST]) {
        LOG(DJAN_LOG, LOG_DEBUG, "Last panel info RPM:%d GPM:%d WATTS:%d\n", aqdata->pumps[i].rpm, aqdata->pumps[i].gpm, aqdata->pumps[i].watts);
        break;
      }
    }
  }

  return false;
}

bool processPacketFromJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  bool found=false;

  // Only log if we are jandy debug move and not serial (otherwise it'll print twice)
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char msg[1000];
    //logMessage(LOG_DEBUG, "Need to log ePump message here for future\n");
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "From ePump: %s\n", msg);
  }

  if (packet_buffer[3] == CMD_EPUMP_STATUS && packet_buffer[4] == CMD_EPUMP_RPM) {
    for (int i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].prclType == JANDY && aqdata->pumps[i].pumpID == previous_packet_to ) {
        LOG(DJAN_LOG, LOG_INFO, "Jandy Pump Status message = RPM %d\n",( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM]) / 4 );
        aqdata->pumps[i].rpm = ( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM] ) / 4;
        found=true;
      }
    }
  } else if (packet_buffer[3] == CMD_EPUMP_STATUS && packet_buffer[4] == CMD_EPUMP_WATTS) {
    for (int i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].prclType == JANDY && aqdata->pumps[i].pumpID == previous_packet_to ) {
        LOG(DJAN_LOG, LOG_INFO, "Jandy Pump Status message = WATTS %d\n", (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT]);
        aqdata->pumps[i].watts = (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT];
        found=true;
      }
    }
  }

  if (!found) {
    if (packet_buffer[4] == CMD_EPUMP_RPM)
      LOG(DJAN_LOG, LOG_NOTICE, "Jandy Pump found at ID 0x%02hhx with RPM %d, but not configured, information ignored!\n",previous_packet_to,( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM]) / 4 );
    else if (packet_buffer[4] == CMD_EPUMP_WATTS)
      LOG(DJAN_LOG, LOG_NOTICE, "Jandy Pump found at ID 0x%02hhx with WATTS %d, but not configured, information ignored!\n",previous_packet_to, (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT]);
  }

  
  return false;
}

void processMissingAckPacketFromJandyPump(unsigned char destination, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  // Do nothing for the moment.
  return;
}






bool processPacketToJandyJXiHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{

  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char msg[1000];
    //logMessage(LOG_DEBUG, "Need to log ePump message here for future\n");
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "To     JXi: %s\n", msg);
  }

  if (packet_buffer[3] != CMD_JXI_PING) {
    // Not sure what this message is, so ignore
    // Maybe print a messsage.
    return false;
  }
  /*
  Below counfing first as bit 0
  4th bit 0x00 no pump on (nothing)
        0x10 seems to be JXi came online. nothing more
        0x11 (pool mode)
        0x12 (spa mode)
        0x19 heat pool
        0x1a heat spa
  5th bit 0x55 = 85 deg. (current pool setpoint)
  6th bit 0x66 = 102 deg. (current spa setpoint)
  7th bit 0x4f = current water temp 79 (0xFF is off / 255)
  */
 
  if (packet_buffer[5] != aqdata->pool_htr_set_point) {
    LOG(DJAN_LOG, LOG_DEBUG, "JXi pool setpoint %d, Pool heater sp %d (changing to LXi)\n", packet_buffer[5], aqdata->pool_htr_set_point);
    aqdata->pool_htr_set_point = packet_buffer[5];
  }

  if (packet_buffer[6] != aqdata->spa_htr_set_point) {
    LOG(DJAN_LOG, LOG_DEBUG, "JXi spa setpoint %d, Spa heater sp %d (changing to LXi)\n", packet_buffer[6], aqdata->spa_htr_set_point);
    aqdata->spa_htr_set_point = packet_buffer[6];
  }

  if (packet_buffer[7] != 0xff && packet_buffer[4] != 0x00) {
    if (packet_buffer[4] == 0x11 || packet_buffer[4] == 0x19) {
      if (aqdata->pool_temp != packet_buffer[7]) {
        LOG(DJAN_LOG, LOG_DEBUG, "JXi pool water temp %d, pool water temp %d (changing to LXi)\n", packet_buffer[7], aqdata->pool_temp);
        aqdata->pool_temp = packet_buffer[7];
      }
    } else if (packet_buffer[4] == 0x12 || packet_buffer[4] == 0x1a) {
      if (aqdata->spa_temp != packet_buffer[7]) {
        LOG(DJAN_LOG, LOG_DEBUG, "JXi spa water temp %d, spa water temp %d (changing to LXi)\n", packet_buffer[7], aqdata->spa_temp);
        aqdata->spa_temp = packet_buffer[7];
      }
    }
  }

  switch (packet_buffer[4]) {
    case 0x11:  // Pool heat off or enabled
    break;
    case 0x12:  // Pool Heat enabled or heating
    break;
    case 0x19:  // Spa heat off or enabled
    break;
    case 0x1a:  // Spa Hear Heat enabled or heating
    break;
  }

  /*
  char msg[1000];
  int length = 0;

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "To   JXi Heater: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);
  
  return false;
  */
   
  return true;
}

void getJandyHeaterError(struct aqualinkdata *aqdata, char *message) 
{
  if (aqdata->heater_err_status == NUL) {
    return;
  }

  int size = sprintf(message, "JXi Heater ");
  getJandyHeaterErrorMQTT(aqdata, message+size);
}

void getJandyHeaterErrorMQTT(struct aqualinkdata *aqdata, char *message) 
{
  switch (aqdata->heater_err_status) {
      case 0x00:
        //sprintf(message,  "");
      break;
      case 0x10:
        sprintf(message,  "FAULT HIGH LIMIT");
      break;
      case 0x02:
        sprintf(message,  "FAULT H20 SENSOR");
      break;
      case 0x08:
        sprintf(message,  "FAULT AUX MONITOR");
      break;
      default:
      //
      /*  Error we haven't decoded yet
       ?x?? check flow
       0x10 Fault high limit
       ?x?? Fault High Flu temp
       ?x?? Fault Check Igntion Control
       0x02 Fault Short H20 sensor (or Fault open water sensor)
       ?x?? Pump fault
       0x08 AUX Monitor
      */
        sprintf(message,  "FAULT 0x%02hhx",aqdata->heater_err_status);
      break;
    } 
}

bool processPacketFromJandyJXiHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char msg[1000];
    //logMessage(LOG_DEBUG, "Need to log ePump message here for future\n");
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "From   JXi: %s\n", msg);
  }

  if (packet_buffer[3] != CMD_JXI_STATUS) {
    // Not sure what this message is, so ignore
    // Maybe print a messsage.
    return false;
  }
  
  // No error is 0x00, so blindly set it.
  aqdata->heater_err_status = packet_buffer[6];
   // Check if error first
  if (packet_buffer[6] != 0x00) {
    
  } else if (packet_buffer[4] == 0x00) {
    // Not heating.
    // Heater off or enabeled
  } else if (packet_buffer[4] == 0x08) {
    // Heating
    // Heater on of enabled
  }
  /*
  char msg[1000];
  int length = 0;   

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "From JXi Heater: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
  */
 return true;
}

bool processPacketToJandyLXHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  
  char msg[1000];
  int length = 0;

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "To      LX: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
  
}

bool processPacketFromJandyLXHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  char msg[1000];
  int length = 0;   

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "From    LX: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
}


bool processPacketToJandyChemFeeder(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  char msg[1000];
  int length = 0;

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "To    Chem: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  length += sprintf(msg+length, ", pH=%f, ORP=%d",aqdata->ph, aqdata->orp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
}

bool processPacketFromJandyChemFeeder(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to){
  char msg[1000];
  int length = 0;

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "From  Chem: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  length += sprintf(msg+length, ", pH=%f, ORP=%d",aqdata->ph, aqdata->orp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
}




/*

// JXi Heater

// Normal ping and return
5th bit 0x00 no pump on (nothing)
        0x10 seems to be JXi came online. nothing more
        0x11 (pool mode)
        0x12 (spa mode)
        0x19 heat pool
        0x1a heat spa
6th bit 0x55 = 85 deg. (current pool setpoint)
7th bit 0x66 = 102 deg. (current spa setpoint)
8th bit 0x4f = current water temp 79 (0xFF is off / 255)

Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x11|0x55|0x66|0x4f|0xa1|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x00|0x1f|0x10|0x03|

Request to turn on 85  
5th bit 0x19 looks like turn on
6th bit 0x55 = 85 deg.
7th bit 0x4f = current temp 79
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x55|0x66|0x4f|0xa9|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x00|0x27|0x10|0x03|

Request to turn on 90
5th bit 0x19 looks like turn on
6th bit 0x5a = 90 deg.
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x5a|0x66|0x4f|0xae|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x00|0x27|0x10|0x03|

Request to turn off (standard ping)  // return had hi limit error in it
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x11|0x55|0x66|0x4f|0xa1|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x10|0x2f|0x10|0x03|

Returns

5th bit is type 0x00 nothing (or enabeled) - 0x08 looks like heat 
Hi limit error return
7th bit 0x10 looks like the error
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x5a|0x66|0x4f|0xae|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x10|0x37|0x10|0x03|

Errors are ->
check flow
Fault high limit -> 0x10
Fault High Flu temp
Fault Check Igntion Control
Fault Short H20 sensor (or Fault open water sensor) -> 0x02
Pump fault
AUX Monitor -> 0x08
*/