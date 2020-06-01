
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
#include "pentair_messages.h"
#include "utils.h"

bool processPentairPacket(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata) 
{
  bool changedAnything = false;
  int i;
  //ID's 96 to 111 = Pentair (or 0x60 to 0x6F)

  // Need to find a better way to support pump index

  //static int pumpIndex = 1;
  
  if ( packet[PEN_PKT_CMD] == PEN_CMD_STATUS && packet[PEN_PKT_FROM] >= 96 &&  packet[PEN_PKT_FROM] <= 111 ){
    // We have Pentair Pump packet, let's see if it's configured.
    //printf("PUMP\n");

    for (i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].ptype == PENTAIR && aqdata->pumps[i].pumpID == packet[PEN_PKT_FROM] ) {
        // We found the pump.
        logMessage(LOG_INFO, "Pentair Pump Status message = RPM %d | WATTS %d\n",
          (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM],
          (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT]);

        aqdata->pumps[i].rpm = (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM];
        aqdata->pumps[i].watts = (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT];

        changedAnything = true;
        break;
      }
      if (changedAnything != true)
        logMessage(LOG_NOTICE, "Pentair Pump found at ID 0x%02hhx with RPM %d | WATTS %d, but not configured, information ignored!\n",
                               packet[PEN_PKT_FROM],
                               (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM],
                               (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT]);
    }
    // 
  }
  
  return changedAnything;
}




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