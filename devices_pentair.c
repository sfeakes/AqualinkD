
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

#include "aqualink.h"
#include "aq_serial.h"
#include "devices_pentair.h"
#include "utils.h"
#include "packetLogger.h"

bool processPentairPacket(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata) 
{
  bool changedAnything = false;
  int i;

  // Only log if we are pentair debug move and not serial (otherwise it'll print twice)
  if (getLogLevel(DPEN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    char buff[1024];
    beautifyPacket(buff, packet, packet_length, true);
    LOG(DPEN_LOG,LOG_DEBUG, "%s", buff);
  }

  //ID's 96 to 111 = Pentair (or 0x60 to 0x6F)

  // Need to find a better way to support pump index

  //static int pumpIndex = 1;
  
  // Status from pump
  if ( packet[PEN_PKT_CMD] == PEN_CMD_STATUS && packet[PEN_PKT_FROM] >= PENTAIR_DEC_PUMP_MIN &&  packet[PEN_PKT_FROM] <= PENTAIR_DEC_PUMP_MAX ){
    // We have Pentair Pump packet, let's see if it's configured.
    //printf("PUMP\n");

    for (i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].prclType == PENTAIR && aqdata->pumps[i].pumpID == packet[PEN_PKT_FROM] ) {
        // We found the pump.
        LOG(DPEN_LOG, LOG_INFO, "Pentair Pump 0x%02hhx Status message = RPM %d | WATTS %d | GPM %d | Mode %d | DriveState %d | Status %d | PresureCurve %d\n",
          packet[PEN_PKT_FROM],
          (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM],
          (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT],
          packet[PEN_FLOW],
          packet[PEN_MODE],
          packet[PEN_DRIVE_STATE],
          (packet[PEN_HI_B_STATUS] * 256) + packet[PEN_LO_B_STATUS],
          packet[PEN_PPC]);

        aqdata->pumps[i].rpm = (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM];
        aqdata->pumps[i].watts = (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT];
        aqdata->pumps[i].gpm = packet[PEN_FLOW];
        aqdata->pumps[i].mode = packet[PEN_MODE];
        //aqdata->pumps[i].driveState = packet[PEN_DRIVE_STATE];
        aqdata->pumps[i].status = (packet[PEN_HI_B_STATUS] * 256) + packet[PEN_LO_B_STATUS];
        aqdata->pumps[i].pressureCurve = packet[PEN_PPC];

        changedAnything = true;
        break;
      }
      if (changedAnything != true) {
        LOG(DPEN_LOG, LOG_NOTICE, "Pentair Pump found at ID 0x%02hhx values RPM %d | WATTS %d | PGM %d | Mode %d | DriveState %d | Status %d | PresureCurve %d\n",
                               packet[PEN_PKT_FROM],
                               (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM],
                               (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT],
                               packet[PEN_FLOW],
                               packet[PEN_MODE],
                               packet[PEN_DRIVE_STATE],
                              (packet[PEN_HI_B_STATUS] * 256) + packet[PEN_LO_B_STATUS],
                               packet[PEN_PPC]);
      }
    }
    // 
  }
  // Set RPM/GPM to pump 
  else if (packet[PEN_PKT_CMD] == PEN_CMD_SPEED && packet[PEN_PKT_DEST] >= PENTAIR_DEC_PUMP_MIN &&  packet[PEN_PKT_DEST] <= PENTAIR_DEC_PUMP_MAX) {

    //(msg.extractPayloadByte(2) & 32) >> 5 === 0 ? 'RPM' : 'GPM';

    bool isRPM = (packet[11] & 32) >> 5 == 0?true:false;

    if (isRPM) {
      LOG(DPEN_LOG, LOG_INFO,"Pentair Pump 0x%02hhx request to set RPM to %d\n",packet[PEN_PKT_DEST], (packet[11] * 256) + packet[12]);
    } else {
      LOG(DPEN_LOG, LOG_INFO,"Pentair Pump 0x%02hhx request to set GPM to %d\n",packet[PEN_PKT_DEST],packet[11]);
    }
  }
  // Set power to pump 
  else if (packet[PEN_PKT_CMD] == PEN_CMD_POWER && packet[PEN_PKT_DEST] >= PENTAIR_DEC_PUMP_MIN &&  packet[PEN_PKT_DEST] <= PENTAIR_DEC_PUMP_MAX) {
    if (packet[9] == 0x0A) {
      LOG(DPEN_LOG, LOG_INFO,"Pentair Pump 0x%02hhx request set power ON\n");
    } else {
      LOG(DPEN_LOG, LOG_INFO,"Pentair Pump 0x%02hhx request set power OFF\n");
    }
  }
  
  return changedAnything;
}
/*
  VSP Pump Status.

  Mode 0=local control, 1=remote control
  DriveState = no idea
  Pressure Curve = see manual
  Status = below
  (packet[PEN_HI_B_STATUS] * 256) + packet[PEN_LO_B_STATUS];

  // Below was pulled from another project.  0 doesn;t seem to be accurate.
  [0, { name: 'off', desc: 'Off' }], // When the pump is disconnected or has no power then we simply report off as the status.  This is not the recommended wiring
        // for a VS/VF pump as is should be powered at all times.  When it is, the status will always report a value > 0.
  [1, { name: 'ok', desc: 'Ok' }], // Status is always reported when the pump is not wired to a relay regardless of whether it is on or not
        // as is should be if this is a VS / VF pump.  However if it is wired to a relay most often filter, the pump will report status
        // 0 if it is not running.  Essentially this is no error but it is not a status either.
  [2, { name: 'filter', desc: 'Filter warning' }],
  [3, { name: 'overcurrent', desc: 'Overcurrent condition' }],
  [4, { name: 'priming', desc: 'Priming' }],
  [5, { name: 'blocked', desc: 'System blocked' }],
  [6, { name: 'general', desc: 'General alarm' }],
  [7, { name: 'overtemp', desc: 'Overtemp condition' }],
  [8, { name: 'power', dec: 'Power outage' }],
  [9, { name: 'overcurrent2', desc: 'Overcurrent condition 2' }],
  [10, { name: 'overvoltage', desc: 'Overvoltage condition' }],
  [11, { name: 'error11', desc: 'Unspecified Error 11' }],
  [12, { name: 'error12', desc: 'Unspecified Error 12' }],
  [13, { name: 'error13', desc: 'Unspecified Error 13' }],
  [14, { name: 'error14', desc: 'Unspecified Error 14' }],
  [15, { name: 'error15', desc: 'Unspecified Error 15' }],
  [16, { name: 'commfailure', desc: 'Communication failure' }]
*/



/*

Removed as iAqualink has a sleep mode, Keeping code to use as stub for other devices.

*/
#ifdef DO_NOT_COMPILE

bool processiAqualinkMsg(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata) 
{
  bool changedAnything = false;
  static char lastmessage[AQ_MSGLONGLEN];

  //static char message[AQ_MSGLONGLEN + 1];
  static int pumpIndex = 1;

  /*

   Jandy ePumpTM DC, 
   Jandy ePumpTM AC,
   IntelliFlo 1 VF, 
   IntelliFlo VS

  Pump type are like // Not sure how to read this accuratly.
  "Jandy ePUMP   1"
  "Intelliflo VS 1"

  RPM message always comes after the above, so maybe saving last string
  then when see RPM go back to get pump number.

  '    RPM: 2950'
  '  Watts: 1028'
  '    GPM: 1028'
  */

  if (packet_buffer[9] == 'R' && packet_buffer[10] == 'P' && packet_buffer[11] == 'M' && packet_buffer[12] == ':') {
    
    pumpIndex = atoi((char *) &lastmessage[14]);

    if ( pumpIndex < aqdata->num_pumps && pumpIndex < 0) {
      pumpIndex = 1; 
      logMessage(LOG_ERR, "Can't find pump index for messsage '%.*s' in string '%.*s' using %d\n",AQ_MSGLEN, packet_buffer+4, AQ_MSGLEN, lastmessage, pumpIndex);
    }
      
    aqdata->pumps[pumpIndex-1].rpm = atoi((char *) &packet_buffer[13]);

    logMessage(LOG_DEBUG, "Read message '%.*s' from iAqualink device\n",AQ_MSGLEN, packet_buffer+4);
    changedAnything = true;
  }
  else if (packet_buffer[9] == 'G' && packet_buffer[10] == 'P' && packet_buffer[11] == 'H' && packet_buffer[12] == ':') {
    aqdata->pumps[pumpIndex-1].gph = atoi((char *) &packet_buffer[13]);
    logMessage(LOG_DEBUG, "Read message '%.*s' from iAqualink device\n",AQ_MSGLEN, packet_buffer+4);
    changedAnything = true;
  }
  else if (packet_buffer[7] == 'W' && packet_buffer[8] == 'a' && packet_buffer[9] == 't' && packet_buffer[10] == 't' && packet_buffer[11] == 's' && packet_buffer[12] == ':') {
    //printf("Punp %d, Watts = %d\n", pumpIndex, atoi((char *) &packet_buffer[13]));
    aqdata->pumps[pumpIndex-1].watts = atoi((char *) &packet_buffer[13]);
    logMessage(LOG_DEBUG, "Read message '%.*s' from iAqualink device\n",AQ_MSGLEN, packet_buffer+4);
    changedAnything = true;
  }

  //printf("Message : '");
  //fwrite(packet_buffer + 4, 1, packet_length-7, stdout);
  //printf("'\n");

  strncpy(lastmessage, (char *)&packet_buffer[4], packet_length-7);

  return changedAnything;
}

#endif