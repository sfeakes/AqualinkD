
/*

Removed as iAqualink has a sleep mode, Keeping code to use as stub for other devices.


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aqualink.h"
#include "iAqualink_messages.h"
#include "utils.h"

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