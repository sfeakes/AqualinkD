#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aq_serial.h"
#include "utils.h"


void logiAqualinkMsg(unsigned char *packet_buffer, int packet_length) {
  static int pumpIndex = 0;

  /* 
  Pump type are like // Not sure how to read this accuratly.
  "Jandy ePUMP   1"
  "Intelliflo VS 1"

  RPM message always comes after the above, so maybe saving last string
  then when see RPM go back to get pump number.

  '    RPM: 2950'
  '  Watts: 1028'
  */

  if (packet_buffer[9] == 'R' && packet_buffer[10] == 'P' && packet_buffer[11] == 'M' && packet_buffer[12] == ':')
    printf("RPM = %d\n", atoi((char *) &packet_buffer[13]));
  else if (packet_buffer[7] == 'W' && packet_buffer[8] == 'a' && packet_buffer[9] == 't' && packet_buffer[10] == 't' && packet_buffer[11] == 's' && packet_buffer[12] == ':')
    printf("Watts = %d\n", atoi((char *) &packet_buffer[13]));
}


int main(int argc, char *argv[]) {

  //unsigned char packet_buffer[10];
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  int packet_length = 0;
  char hex[5];
  int i;
  //int num;
  //int pi;

  if (argc < 1 || access( argv[1], F_OK ) == -1 ) {
    fprintf(stderr, "ERROR, first param must be valid log file\n");
    return 1;
  }

  FILE *file = fopen ( argv[1], "r" );
  if ( file == NULL )
  {
    perror ( argv[1] ); /* why didn't the file open? */
    return 1;
  }
  
  char line [ 128 ]; /* or other suitable maximum line size */
  while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
  {
    packet_length=0;
    for (i=0; i < strlen(line); i=i+5)
    {
      strncpy(hex, &line[i], 4);
      hex[5] = '\0';
      packet_buffer[packet_length] = (int)strtol(hex, NULL, 16);
      //printf("%s = 0x%02hhx = %c\n", hex, packet_buffer[packet_length], packet_buffer[packet_length]);
      packet_length++;
    }
    packet_length--;

    if (packet_buffer[PKT_DEST] == 0x33 && packet_buffer[PKT_CMD] == 0x25)
        logiAqualinkMsg(packet_buffer, packet_length);
    
    //if (packet_buffer[PKT_CMD] == 0x24 || packet_buffer[PKT_CMD] == 0x25) {
      
      printf("To 0x%02hhx, type %15.15s, length %2.2d ", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length),packet_length);
      fputs ( line, stdout ); 
      //printf("Message : '");
      //fwrite(packet_buffer + 4, 1, packet_length-7, stdout);
      //printf("'\n");
    //}
  }
  fclose ( file );
  

  return 0;
}

/*
*** NOTES ***

iAqualink is ID 0x33
Two TEXT Message Types 0x24 & 0x25
0x24 seems to be menu options
0x25 seems to be status messages

*/