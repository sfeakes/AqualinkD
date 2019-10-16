
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

#include <fcntl.h>


#include "aq_serial.h"
#include "utils.h"

#define SLOG_MAX 80
#define PACKET_MAX 600

#define VERSION "serial_logger V1.1"

/*
typedef enum used {
  yes,
  no,
  unknown
} used;
*/

typedef struct serial_id_log {
  unsigned char ID;
  bool inuse;
} serial_id_log;

bool _keepRunning = true;

unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
unsigned char _goodPDAID[] = {0x60, 0x61, 0x62, 0x63};
unsigned char _filter[10];
int _filters=0;
bool _rawlog=false;
bool _playback_file = false;

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
  if (_playback_file)  // If we are reading file, loop is irevelent
    exit(0);
}

#define MASTER " <-- Master control panel"
#define SWG " <-- Salt Water Generator (Aquarite mode)"
#define KEYPAD " <-- RS Keypad"
#define SPA_R " <-- Spa remote"
#define AQUA " <-- Aqualink (iAqualink?)"
#define HEATER " <-- LX Heater"
#define ONE_T " <-- Onetouch device"
#define PC_DOCK " <-- PC Interface (RS485 to RS232)"
#define PDA " <-- PDA Remote"
#define EPUMP " <-- Jandy VSP ePump"
#define CHEM " <-- Chemlink"

#define UNKNOWN " <-- Unknown Device"

#define P_VSP " <-- Pentair VSP"
#define P_MASTER " <-- Pentair Master (Probably Jandy RS Control Panel)"
#define P_SWG " <-- Salt Water Generator (Jandy mode)"
#define P_BCAST " <-- Broadcast address"
#define P_RCTL " <-- Remote wired controller"
#define P_RWCTL " <-- Remote wireless controller (Screen Logic)"
#define P_CTL " <-- Pool controller (EasyTouch)"

const char *getDevice(unsigned char ID) {
  if (ID >= 0x00 && ID <= 0x03)
    return MASTER;
  if (ID >= 0x08 && ID <= 0x0B)
    return KEYPAD;
  if (ID >= 0x50 && ID <= 0x53)
    return SWG;
  if (ID >= 0x20 && ID <= 0x23)
    return SPA_R;
  if (ID >= 0x30 && ID <= 0x33)
    return AQUA;
  if (ID >= 0x38 && ID <= 0x3B)
    return HEATER;
  if (ID >= 0x40 && ID <= 0x43)
    return ONE_T;
  if (ID >= 0x58 && ID <= 0x5B)
    return PC_DOCK;
  if (ID >= 0x60 && ID <= 0x63)
    return PDA;
  //if (ID >= 0x70 && ID <= 0x73)
  if (ID >= 0x78 && ID <= 0x7B)
    return EPUMP;
  if (ID >= 0x80 && ID <= 0x83)
    return CHEM;
  //if (ID == 0x08)
  //  return KEYPAD;

  return UNKNOWN;
}

const char *getPentairDevice(unsigned char ID) {
  if (ID >= 0x60 && ID <= 0x6F)
    return P_VSP;
  if (ID == 0x02)
    return P_SWG;
  if (ID == 0x10)  
    return P_MASTER;
  if (ID == 0x0F)
    return P_BCAST;
  if (ID == 0x10)
    return P_CTL;
  if (ID == 0x20)
    return P_RCTL;
  if (ID == 0x22)
    return P_RWCTL;

  return UNKNOWN;
}

void advance_cursor() {
  static int pos=0;
  char cursor[4]={'/','-','\\','|'};
  printf("%c\b", cursor[pos]);
  fflush(stdout);
  pos = (pos+1) % 4;
}

bool canUse(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodID[i])
      return true;
  }
  for (i = 0; i < 4; i++) {
    if (ID == _goodPDAID[i])
      return true;
  }
  return false;
}
char* canUseExtended(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodID[i])
      return " <-- can use for Aqualinkd";
  }
  for (i = 0; i < 4; i++) {
    if (ID == _goodPDAID[i])
      return " <-- can use for Aqualinkd (PDA mode only)";
  }
  return "";
}


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
  if (_rawlog) {
    printHex((char *)packet_buffer, packet_length);
    printf("\n");
    return;
  }

  if (_filters != 0)
  {
    int i;
    bool dest_match = false;
    bool src_match = false;

    for (i=0; i < _filters; i++) {
      if ( packet_buffer[PKT_DEST] == _filter[i])
        dest_match = true;
      if ( ID == _filter[i] && packet_buffer[PKT_DEST] == 0x00 )
        src_match = true;
    }

    if(dest_match == false && src_match == false)
      return;
  }
  
/*
  if (_filter != 0x00) {
    if ( packet_buffer[PKT_DEST]==0x00 && ID != _filter )
      return;
    if ( packet_buffer[PKT_DEST]!=0x00 && packet_buffer[PKT_DEST] != _filter )
      return;
  }
*/
  if (getProtocolType(packet_buffer)==JANDY) {
    if (packet_buffer[PKT_DEST] != 0x00)
      printf("\n");
    printf("Jandy   %4.4s 0x%02hhx of type %8.8s", (packet_buffer[PKT_DEST]==0x00?"From":"To"), (packet_buffer[PKT_DEST]==0x00?ID:packet_buffer[PKT_DEST]), get_packet_type(packet_buffer, packet_length));
  } else {
    printf("Pentair From 0x%02hhx To 0x%02hhx       ",packet_buffer[PEN_PKT_FROM],packet_buffer[PEN_PKT_DEST]  );
  }
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

int main(int argc, char *argv[]) {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  unsigned char lastID = 0x00;
  int i = 0;
  bool found;
  serial_id_log slog[SLOG_MAX];
  serial_id_log pent_slog[SLOG_MAX];
  int sindex = 0;
  int pent_sindex = 0;
  int received_packets = 0;
  int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
  //bool playback_file = false;

  //int logLevel; 
  //char buffer[256];
  //bool idMode = true;

  printf("AqualinkD %s\n",VERSION);

  if (getuid() != 0) {
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc < 2 || access( argv[1], F_OK ) == -1 ) {
    fprintf(stderr, "ERROR, first param must be valid serial port, ie:-\n\t%s /dev/ttyUSB0\n\n", argv[0]);
    fprintf(stderr, "Optional parameters are -d (debug) & -p <number> (log # packets) & -i <ID> & -r (raw) ie:=\n\t%s /dev/ttyUSB0 -d -p 1000 -i 0x08\n\n", argv[0]);
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
      _filter[_filters] = n;
      _filters++;
      printf("Add filter %i 0x%02hhx\n",_filters, _filter[_filters-1]);
      logLevel = LOG_DEBUG; // no point in filtering on ID if we're not going to print it.
    } else if (strcmp(argv[i], "-r") == 0) {
      _rawlog = true;
      logLevel = LOG_DEBUG;
    } else if (strcmp(argv[i], "-f") == 0) {
      _playback_file = true;
    }
  }

  setLoggingPrms(logLevel, false, false, NULL);

  if (_playback_file) {
    rs_fd = open(argv[1], O_RDONLY | O_NOCTTY | O_NONBLOCK | O_NDELAY);
    if (rs_fd < 0)  {
      logMessage(LOG_ERR, "Unable to open file: %s\n", argv[1]);
      displayLastSystemError(argv[1]);
      return -1;
    }
  } else {
    rs_fd = init_serial_port(argv[1]);
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  logMessage(LOG_NOTICE, "Logging serial information!\n");
  if (logLevel < LOG_DEBUG)
    printf("Please wait.");

  while (_keepRunning == true) {
    if (rs_fd < 0) {
      logMessage(LOG_ERR, "ERROR, serial port disconnect\n");
    }

    //packet_length = get_packet(rs_fd, packet_buffer);
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

        if (getProtocolType(packet_buffer) == PENTAIR) {
          found = false;
          for (i = 0; i <= pent_sindex; i++) {
            if (pent_slog[i].ID == packet_buffer[PEN_PKT_FROM]) {
              found = true;
              break;
            }
          }
          if (found == false) {
            pent_slog[pent_sindex].ID = packet_buffer[PEN_PKT_FROM];
            pent_slog[pent_sindex].inuse = true;
            pent_sindex++;
          }
        } else {
         if (packet_buffer[PKT_DEST] != DEV_MASTER) {
          found = false;
          for (i = 0; i <= sindex; i++) {
            if (slog[i].ID == packet_buffer[PKT_DEST]) {
              found = true;
              break;
            }
           }
           if (found != true && sindex < SLOG_MAX) {
            slog[sindex].ID = packet_buffer[PKT_DEST];
            slog[sindex].inuse = false;
            sindex++;
          }
         }

         if (packet_buffer[PKT_DEST] == DEV_MASTER /*&& packet_buffer[PKT_CMD] == CMD_ACK*/) {
          //logMessage(LOG_NOTICE, "ID is in use 0x%02hhx %x\n", lastID, lastID);
          for (i = 0; i <= sindex; i++) {
            if (slog[i].ID == lastID) {
              slog[i].inuse = true;
              break;
            }
          }
         }
  
        lastID = packet_buffer[PKT_DEST];
      }
      received_packets++;

      // NSF TESTING
      /*
        if (packet_buffer[PKT_DEST] == 0x40) {
          static int hex = 0;
          //printf("Sent ack\n");
          //printf("Sent ack hex 0x%02hhx\n",(unsigned char)hex);
          //send_extended_ack (rs_fd, 0x8b, (unsigned char)hex);
          send_extended_ack (rs_fd, 0x8b, 0x00);
          hex++;
         
        }*/
// NSF
    }

    if (logPackets != 0 && received_packets >= logPackets) {
      _keepRunning = false;
    }
    if (logLevel < LOG_DEBUG)
      advance_cursor();

    //sleep(1);
  }

  logMessage(LOG_DEBUG, "\n\n");
  if (logLevel < LOG_DEBUG)
    printf("\n\n");

  if (sindex >= SLOG_MAX)
    logMessage(LOG_ERR, "Ran out of storage, some ID's were not captured, please increase SLOG_MAX and recompile\n");
  logMessage(LOG_NOTICE, "Jandy ID's found\n");
  for (i = 0; i < sindex; i++) {
    //logMessage(LOG_NOTICE, "ID 0x%02hhx is %s %s\n", slog[i].ID, (slog[i].inuse == true) ? "in use" : "not used",
    //           (slog[i].inuse == false && canUse(slog[i].ID) == true)? " <-- can use for Aqualinkd" : "");
    if (logLevel >= LOG_DEBUG || slog[i].inuse == true || canUse(slog[i].ID) == true) {
      logMessage(LOG_NOTICE, "ID 0x%02hhx is %s %s\n", slog[i].ID, (slog[i].inuse == true) ? "in use" : "not used",
               (slog[i].inuse == false)?canUseExtended(slog[i].ID):getDevice(slog[i].ID));
    }
  }

  if (pent_sindex > 0) {
    logMessage(LOG_NOTICE, "\n\n");
    logMessage(LOG_NOTICE, "Pentair ID's found\n");
  }
  for (i=0; i < pent_sindex; i++) {
    logMessage(LOG_NOTICE, "ID 0x%02hhx is %s %s\n", pent_slog[i].ID, (pent_slog[i].inuse == true) ? "in use" : "not used",
               (pent_slog[i].inuse == false)?canUseExtended(pent_slog[i].ID):getPentairDevice(pent_slog[i].ID));
  }

  logMessage(LOG_NOTICE, "\n\n");

  return 0;
}
