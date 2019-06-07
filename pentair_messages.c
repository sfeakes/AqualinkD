

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
  
  // Need to find a better way to support pump index

  static int pumpIndex = 1;
  
  if ( packet[PEN_PKT_FROM] == 0x60 && packet[PEN_PKT_CMD] == PEN_CMD_STATUS ){
    //printf("PUMP\n");
    logMessage(LOG_INFO, "Pentair Pump Status message = RPM %d | WATTS %d\n",
          (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM],
          (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT]);

    aqdata->pumps[pumpIndex-1].rpm = (packet[PEN_HI_B_RPM] * 256) + packet[PEN_LO_B_RPM];
    aqdata->pumps[pumpIndex-1].watts = (packet[PEN_HI_B_WAT] * 256) + packet[PEN_LO_B_WAT];

    changedAnything = true;
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

    if ( pumpIndex < MAX_PUMPS && pumpIndex < 0) {
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