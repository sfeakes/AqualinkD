#define _GNU_SOURCE // for strcasestr
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pda_menu.h"
#include "aq_serial.h"
#include "utils.h"



#define SLOG_MAX 80
#define PACKET_MAX 600

/*
typedef enum used {
  yes,
  no,
  unknown
} used;
*/
#define PDA_ID       0x60
//#define PDA_ID       0x58
#define PDA_LINES    10 // There is only 9 lines, but add buffer to make shifting easier
/*
typedef struct serial_id_log {
  unsigned char ID;
  bool inuse;
} serial_id_log;
*/

bool _keepRunning = true;
//int _lineindex = -1;
//char _menu[PDA_LINES][AQ_MSGLEN+1]; 
//bool _readMenu = false;
//unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
unsigned char _filter = PDA_ID;

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
}

/*
bool canUse(unsigned char ID) {
  int i;
  for (i = 0; i < strlen((char *)_goodID); i++) {
    if (ID == _goodID[i])
      return true;
  }
  return false;
}
*/

void printHex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
}


void printPacket(unsigned char ID, unsigned char *packet_buffer, int packet_length)
{
  //if (_filter != 0x00 && ID != _filter && packet_buffer[PKT_DEST] != _filter )
  //  return;

  if (_filter != 0x00) {
    if ( packet_buffer[PKT_DEST]==0x00 && ID != _filter )
      return;
    if ( packet_buffer[PKT_DEST]!=0x00 && packet_buffer[PKT_DEST] != _filter )
      return;
  }

  if (packet_buffer[PKT_DEST] != 0x00)
    printf("\n");

  printf("%4.4s 0x%02hhx of type %8.8s", (packet_buffer[PKT_DEST]==0x00?"From":"To"), (packet_buffer[PKT_DEST]==0x00?ID:packet_buffer[PKT_DEST]), get_packet_type(packet_buffer, packet_length));
  printf(" | HEX: ");
  printHex((char *)packet_buffer, packet_length);
  
  if (packet_buffer[PKT_CMD] == CMD_MSG || packet_buffer[PKT_CMD] == CMD_MSG_LONG) {
    printf("  Message : ");
    //fwrite(packet_buffer + 4, 1, AQ_MSGLEN+1, stdout);
    fwrite(packet_buffer + 4, 1, packet_length-7, stdout);
  }
  
  //if (packet_buffer[PKT_DEST]==0x00)
  //  printf("\n\n");
  //else
    printf("\n");
}
/*
int addLine(unsigned char *packet_buffer)
{
  //strcpy (_menu[20], ;

  if (packet_buffer[PKT_DATA] < 10) {
    memset(_menu[packet_buffer[PKT_DATA]], 0, AQ_MSGLEN);
    strncpy(_menu[packet_buffer[PKT_DATA]], (char*)packet_buffer+PKT_DATA+1, AQ_MSGLEN);
    _menu[packet_buffer[PKT_DATA]][AQ_MSGLEN+1] = '\0';
    //printf ("Added line\n");
  } else if (packet_buffer[PKT_DATA] == 0x82) {
    printf("AIR TEMP = %s\n",(char*)packet_buffer+PKT_DATA+1);
  } else if (packet_buffer[PKT_DATA] == 0x40) {
    printf("TIME = %s\n",(char*)packet_buffer+PKT_DATA+1);
  }

  {
    int i;
    for (i=0; i < PDA_LINES; i++)
      printf("Line %d = %s\n",i,_menu[i]);
    printf("Highlight = %d = %s\n",_lineindex,_menu[_lineindex]);
  }
  return 0;
}
*/
/*
Line 2 =   AquaPure 60%  
Line 3 =  SALT 3200 PPM  
Line 4 =   FILTER PUMP   
Line 5 =   SPA HEAT ENA  

Line 4 = POOL MODE     ON
Line 5 = POOL HEATER  OFF
Line 6 = SPA MODE     OFF
Line 7 = SPA HEATER   OFF

Line 1 = FILTER PUMP   ON
Line 2 = SPA          OFF
Line 3 = POOL HEAT    OFF
Line 4 = SPA HEAT     OFF
Line 5 = CLEANER       ON
Line 6 = AUX2         OFF
Line 7 = AUX3         OFF
Line 8 = AUX4         OFF
*/
bool process_pda_packet(unsigned char* packet, int length)
{
  bool rtn = true;

  process_pda_menu_packet(packet, length);

  switch (packet[PKT_CMD]) {

    case CMD_MSG_LONG: {
      char *msg = (char*)packet+PKT_DATA+1;

      if (packet[PKT_DATA] == 0x82) { // Air & Water temp is always this ID
        // message = "73`     66`"
        //_aqualink_data.temp_units = FAHRENHEIT; // Force FAHRENHEIT
        //_aqualink_data.air_temp = atoi(msg);
        //_aqualink_data.pool_temp = atoi(msg+7);
        printf("Air Temp = %d | Water Temp = %d\n",atoi(msg),atoi(msg+7));
      } else if (packet[PKT_DATA] == 0x40) {
        // message "   SAT 8:46AM"

        //NSF strcpy(_aqualink_data.time, msg);
        if (msg[8] == ' ')
          printf("TIME = %.*s\n",6,msg+9);
        else
          printf("TIME = %.*s\n",7,msg+8);
           
        // NSF  strcpy(_aqualink_data.date, msg);
        if (msg[4] == ' ')
          printf("DAY = %.*s\n",3,msg+5);
        else
          printf("DAY = %.*s\n",3,msg+4);
        // NSF Come back and change the above to correctly check date and time in future
      } else if (pda_m_hlightindex() == -1) { // There is a chance this is a message we are interested in.
        char *index;  
        if( (index = strcasestr(msg, MSG_SWG_PCT)) != NULL) {
          int aq = atoi(index + strlen(MSG_SWG_PCT));
          printf("Aquapure PERCENT message %d\n", aq);
        }
        if( (index = strcasestr(msg, MSG_SWG_PPM)) != NULL) {
          int aq = atoi(index + strlen(MSG_SWG_PPM));
          printf("Aquapure SALT message %d\n", aq);
        }
      } else if (strcmp(pda_m_line(0), "   EQUIPMENT    ") != 0) {
        // Check message for status of device
      }
    }
    break;
  }

  return rtn;
}


int main(int argc, char *argv[]) {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  unsigned char lastID;
  int i = 0;
  //bool found;
  //serial_id_log slog[SLOG_MAX];
  //int sindex = 0;
  int received_packets = 0;
  int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
  unsigned char NextMsg = NUL;
  //int logLevel; 
  //char buffer[256];
  //bool idMode = true;

  if (getuid() != 0) {
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc < 2 || access( argv[1], F_OK ) == -1 ) {
    fprintf(stderr, "ERROR, first param must be valid serial port, ie:-\n\t%s /dev/ttyUSB0\n\n", argv[0]);
    fprintf(stderr, "Optional parameters are -d (debug) & -p <number> (log # packets) & -i <ID> ie:=\n\t%s /dev/ttyUSB0 -d -p 1000 -i 0x08\n\n", argv[0]);
    return 1;
  }

  
  for (i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      logLevel = LOG_DEBUG;
    } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
      logPackets = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
      unsigned int n;
      sscanf(argv[i+1], "0x%2x", &n);
      _filter = n;
      logLevel = LOG_DEBUG; // no point in filtering on ID if we're not going to print it.
    }
  }

  setLoggingPrms(logLevel, false, false);

  rs_fd = init_serial_port(argv[1]);

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  logMessage(LOG_NOTICE, "Logging serial information!\n");
  if (logLevel < LOG_DEBUG)
    printf("Please wait.");

  while (_keepRunning == true) {
    if (rs_fd < 0) {
      logMessage(LOG_ERR, "ERROR, serial port disconnect\n");
    }

    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_ERR, "ERROR, on serial port\n");
      _keepRunning = false;
    } else if (packet_length == 0) {
      // Nothing read
    } else if (packet_length > 0) {

        //logMessage(LOG_DEBUG_SERIAL, "Received Packet for ID 0x%02hhx of type %s\n", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
      if (logLevel > LOG_NOTICE)
        printPacket(lastID, packet_buffer, packet_length);


      if (packet_buffer[PKT_DEST] == PDA_ID) {
        
        if (packet_buffer[PKT_CMD] == CMD_STATUS) {
          send_extended_ack(rs_fd, 0x40, NextMsg);
          NextMsg = NUL;
        } else {
          //printf("****** Send ACK *******\n");
          send_extended_ack(rs_fd, ACK_PDA, NUL);
        }

        process_pda_packet(packet_buffer, packet_length);

        switch (packet_buffer[PKT_CMD]) {
          case CMD_MSG_LONG:
            if (strcasestr(pda_m_line(0), "EQUIPMENT STATUS") != NULL) {
              //NextMsg = KEY_PDA_BACK;
              //printf("****** queue BACK *******\n");
            }
            //if (_readMenu)
            //  addLine(packet_buffer);
          break;
        }
      }

      char *selection = pda_m_hlight();
      if (selection != NULL && strcmp(selection, "EQUIPMENT ON/OFF") != 0) {
        NextMsg = KEY_PDA_DOWN;
      } else if (selection != NULL && strcmp(selection, "EQUIPMENT ON/OFF") == 0) {
        NextMsg = KEY_PDA_SELECT;
      }

      lastID = packet_buffer[PKT_DEST];
      received_packets++;
    }

    if (logPackets != 0 && received_packets >= logPackets) {
      _keepRunning = false;
    }

    //sleep(1);
  }

  return 0;
}
