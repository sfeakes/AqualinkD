
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

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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


void read_bin_file(char *filename)
{
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  int packet_length;
  int i=0;
  
  setLoggingPrms(LOG_DEBUG_SERIAL , false, NULL, NULL);
  startPacketLogger(false,true);

  int fd = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
  if (fd < 0)  {
    logMessage(LOG_ERR, "Unable to open port: %s\n", filename);
    return;
  }

  while ( (packet_length = get_packet_new(fd, packet_buffer)) > -1){
    printf("----------------\n");
    //logPacket(packet_buffer, packet_length);
  }

}

void create_bin_file(FILE *in, FILE *out) {
  char hex[6];
  unsigned char byte;

  while ( fgets ( hex, 6, in ) != NULL ) /* read a line */
  {
    byte = (int)strtol(hex, NULL, 16);
    //printf("read 0x%02hhx, %s\n",byte,hex);
    fwrite(&byte, 1,1, out);
    //return;
  }
}

int main(int argc, char *argv[]) {

  //unsigned char packet_buffer[10];
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  int packet_length = 0;
  char hex[5];
  int i;
  FILE *outfile = NULL;
  bool writeb = false;
  //int num;
  //int pi;

  if (strcmp(argv[1], "-rb") == 0) {
    read_bin_file(argv[2]);
    return 0;
  }
  else if (argc < 1 || access( argv[1], F_OK ) == -1 ) {
    fprintf(stderr, "ERROR, first param must be valid log file\n");
    return 1;
  }
  
  else if (strcmp(argv[2], "-b") == 0) {
    outfile = fopen ( argv[3], "w" );
    if ( outfile == NULL )
    {
      fprintf(stderr, "ERROR, Couldn't open out file `%s`\n", argv[3]);
      perror ( argv[3] ); /* why didn't the file open? */
      return 1;
    }
    writeb = true;
  }

  FILE *file = fopen ( argv[1], "r" );
  if ( file == NULL )
  {
    perror ( argv[1] ); /* why didn't the file open? */
    return 1;
  }

  if (writeb) {
    create_bin_file(file, outfile);
    fclose ( file );
    fclose ( outfile );
    return 0;
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
      
    printf("To 0x%02hhx, type %15.15s, length %2.2d\n", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length),packet_length);
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