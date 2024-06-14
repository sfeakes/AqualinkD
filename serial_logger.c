
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
#include <time.h>

#include "serial_logger.h"
#include "aq_serial.h"
#include "utils.h"
#include "packetLogger.h"
#include "rs_msg_utils.h"

#ifdef SERIAL_LOGGER
  // Make us look lie config.c when we load config.h
  #define CONFIG_C
#endif
#include "config.h"

#define SLOG_MAX 80
#define PACKET_MAX 600

#define VERSION "serial_logger V2.3"

/*
typedef enum used {
  yes,
  no,
  unknown
} used;
*/

// Bogus config to keep aq_serial.c happy
/*
struct aqconfig
{
  bool readahead_b4_write;
  bool log_protocol_packets; // Read & Write as packets write to file
  bool log_raw_bytes; // bytes read and write to file
};
struct aqconfig _aqconfig_;
*/

char _panelType[AQ_MSGLEN];
char _panelRev[AQ_MSGLEN];
bool _panelPDA = false;

typedef struct serial_id_log {
  unsigned char ID;
  bool inuse;
} serial_id_log;

bool _keepRunning = true;

unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
unsigned char _goodPDAID[] = {0x60, 0x61, 0x62, 0x63}; // PDA Panel only supports one PDA.
unsigned char _goodONETID[] = {0x40, 0x41, 0x42, 0x43};
unsigned char _goodIAQTID[] = {0x30, 0x31, 0x32, 0x33};
//unsigned char _goodRSSAID[] = {0x48, 0x49, 0x4a, 0x4b};
unsigned char _goodRSSAID[] = {0x48, 0x49};  // Know there are only 2 good RS SA id's, guess 0x49 is the second.
unsigned char _filter[10];
int _filters=0;
unsigned char _pfilter[10];
int _pfilters=0;
bool _rawlog=false;
bool _playback_file = false;


int sl_timespec_subtract (struct timespec *result, const struct timespec *x, const struct timespec *y);

int _serial_logger(int rs_fd, char *port_name, int logPackets, int logLevel, bool panleProbe, bool rsSerialSpeedTest, bool errorMonitor, bool printAllIDs, bool timePackets);


#ifdef SERIAL_LOGGER
void broadcast_log(char *msg){
  // Do nothing, just for utils.c to work.
}
void intHandler(int dummy) {
  _keepRunning = false;
  LOG(RSSD_LOG, LOG_NOTICE, "Stopping!\n");
  if (_playback_file)  // If we are reading file, loop is irevelent
    exit(0);
}
#else
int serial_logger (int rs_fd, char *port_name, int logLevel) {
  return _serial_logger(rs_fd,  port_name, PACKET_MAX, (logLevel>=LOG_NOTICE?logLevel:LOG_NOTICE), true, false, false, false, false);
}
#endif

#define MASTER " <-- Master control panel"
#define SWG " <-- Salt Water Generator (Aquarite mode)"
#define KEYPAD " <-- RS Keypad"
#define SPA_R " <-- Spa remote"
#define AQUA " <-- Aqualink (iAqualink / Touch)"
#define LX_HEATER " <-- LX Heater"
#define ONE_T " <-- Onetouch device"
#define RS_SERL " <-- RS Serial Adapter"
#define PC_DOCK " <-- PC Interface (RS485 to RS232)"
#define PDA " <-- PDA Remote"
#define EPUMP " <-- Jandy VSP ePump"
#define CHEM " <-- Chemlink"
#define JXI_HEATER " <-- LXi / LRZ Heater"

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
    return LX_HEATER;
  if (ID >= 0x40 && ID <= 0x43)
    return ONE_T;
  if (ID >= 0x48 && ID <= 0x4B)
    return RS_SERL;
  if (ID >= 0x58 && ID <= 0x5B)
    return PC_DOCK;
  if (ID >= 0x60 && ID <= 0x63)
    return PDA;
  if (ID >= 0x68 && ID <= 0x6B)
    return JXI_HEATER;
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

bool canUseAllB(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodID[i])
      return true;
  }
  return false;
}
bool canUsePDA(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodPDAID[i])
      return true;
  }
  return false;
}
bool canUseONET(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodONETID[i])
      return true;
  }
  return false;
}
bool canUseIQAT(unsigned char ID) {
  int i;
  for (i = 0; i < 4; i++) {
    if (ID == _goodIAQTID[i])
      return true;
  }
  return false;
}
bool canUseRSSA(unsigned char ID) {
  int i;
  for (i = 0; i < 2; i++) {
    if (ID == _goodRSSAID[i])
      return true;
  }
  return false;
}

bool canUse(unsigned char ID) {
  if (canUseAllB(ID))
    return true;
  else if (canUsePDA(ID))
    return true;
  else if (canUseONET(ID))
    return true;
  else if (canUseIQAT(ID))
    return true;
  else if (canUseRSSA(ID))
    return true;

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
  for (i = 0; i < 4; i++) {
    if (ID == _goodONETID[i] && _panelPDA == false)
      return " <-- can use for Aqualinkd (Extended Device ID)";
  }
  for (i = 0; i < 4; i++) {
    if (ID == _goodIAQTID[i] && _panelPDA == false)
      return " <-- can use for Aqualinkd (Prefered Extended Device ID)";
    else if (ID == _goodIAQTID[i] && _panelPDA == true)
      return " <-- can use for Aqualinkd (PDA mode Expermental only)";
  }
  for (i = 0; i < 2; i++) {
    if (ID == _goodRSSAID[i] && _panelPDA == false)
      return " <-- can use for Aqualinkd (RSSA ID)";
  }
  return "";
}

void getPanelInfo(int rs_fd, unsigned char *packet_buffer, int packet_length)
{
  static unsigned char getPanelRev[] = {0x00,0x14,0x01};
  static unsigned char getPanelType[] = {0x00,0x14,0x02};
  static int msgcnt=0;

  if (packet_buffer[PKT_CMD] == CMD_PROBE) {
    if (msgcnt == 0)
      send_ack(rs_fd, 0x00);
    else if (msgcnt == 1)
      send_jandy_command(rs_fd, getPanelRev, 3);
    else if (msgcnt == 2)
      send_jandy_command(rs_fd, getPanelType, 3);
    msgcnt++;
  } else if (packet_buffer[PKT_CMD] == CMD_MSG) {
    send_ack(rs_fd, 0x00);
    if (msgcnt == 2)
      rsm_strncpy(_panelRev, packet_buffer+4, AQ_MSGLEN, packet_length-5);
    else if (msgcnt == 3)
      rsm_strncpy(_panelType, packet_buffer+4, AQ_MSGLEN, packet_length-5);
  }

  if (_panelType[1] == 'P' && _panelType[2] == 'D') { // PDA Panel
    _panelPDA = true;
  }
}

#ifdef SERIAL_LOGGER

void printHex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
}


void printPacket(unsigned char ID, unsigned char *packet_buffer, int packet_length, char *message)
{
  int i;

  if (_rawlog) {
    printHex((char *)packet_buffer, packet_length);
    printf("\n");
    return;
  }

  // if filter is set and not match, then return without printing.
  if (_filters != 0 || _pfilters != 0)
  {
    bool dest_match = false;
    bool src_match = false;
    bool pent_match = false;

    if (_filters != 0) {
      for (i=0; i < _filters; i++) {
        if ( packet_buffer[PKT_DEST] == _filter[i])
          dest_match = true;
        if ( ID == _filter[i] && packet_buffer[PKT_DEST] == 0x00 )
          src_match = true;
      }
    }
    if (_pfilters != 0) {
      for (i=0; i < _pfilters; i++) {
        if ( packet_buffer[PEN_PKT_FROM] == _pfilter[i] ||
             packet_buffer[PEN_PKT_DEST] == _pfilter[i] )
          pent_match = true;
      }
    }

    if(dest_match == false && src_match == false && pent_match == false)
      return;
  }

  if (message != NULL)
    printf("%s",message);

  protocolType ptype = getProtocolType(packet_buffer);

  printf("%s packet To 0x%02hhx of type %16.16s",
                      (ptype==PENTAIR?"Pentair":"Jandy  "),
                      packet_buffer[(ptype==JANDY?PKT_DEST:PEN_PKT_DEST)], 
                      get_packet_type(packet_buffer, packet_length));
/*
  if (getProtocolType(packet_buffer)==JANDY) {
    if (packet_buffer[PKT_DEST] != 0x00)
      printf("\n");
    printf("Jandy   %4.4s 0x%02hhx of type %16.16s", (packet_buffer[PKT_DEST]==0x00?"From":"To"), (packet_buffer[PKT_DEST]==0x00?ID:packet_buffer[PKT_DEST]), get_packet_type(packet_buffer, packet_length));
  } else {
    if (packet_buffer[PEN_PKT_FROM] == 0x10)
      printf("\n");
    printf("Pentair From 0x%02hhx To 0x%02hhx                 ",packet_buffer[PEN_PKT_FROM],packet_buffer[PEN_PKT_DEST]  );
  }
*/
  printf(" | HEX: ");
  printHex((char *)packet_buffer, packet_length);
  
  if (packet_buffer[PKT_CMD] == CMD_MSG || packet_buffer[PKT_CMD] == CMD_MSG_LONG) {
    printf("  Message : ");
    for(i=4; i < packet_length-3; i++) {
      if (packet_buffer[i] >= 32 && packet_buffer[i] <= 126)
        printf("%c",packet_buffer[i]);
    }
  }
  
  printf("\n");
}

int main(int argc, char *argv[]) {
  int rs_fd;
  int i = 0;
  int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
  bool panleProbe = true;
  bool rsSerialSpeedTest = false;
  bool serialBlocking = true;
  bool errorMonitor = false;
  bool printAllIDs = false;
  bool timePackets = false;

  // aq_serial.c uses the following
  _aqconfig_.log_protocol_packets = false;
  _aqconfig_.log_raw_bytes = false;
  _aqconfig_.ftdi_low_latency  = true;
  _aqconfig_.frame_delay = 10;

  printf("AqualinkD %s\n",VERSION);

  if (getuid() != 0) {
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc < 2 || access( argv[1], F_OK ) == -1 ) {
    fprintf(stderr, "ERROR, first param must be valid serial port, ie:-\n\t%s /dev/ttyUSB0\n\n", argv[0]);
    //fprintf(stderr, "Optional parameters are -d (debug) & -p <number> (log # packets) & -i <ID> & -r (raw) ie:=\n\t%s /dev/ttyUSB0 -d -p 1000 -i 0x08\n\n", argv[0]);
    fprintf(stderr, "Optional parameters are :-\n");
    fprintf(stderr, "\t-n (Do not probe panel for type/rev info)\n");
    fprintf(stderr, "\t-d (debug / print messages)\n");
    fprintf(stderr, "\t-p <number> (# packets to log, default=%d)\n",PACKET_MAX);
    fprintf(stderr, "\t-i <ID> (just log specific ID, can use multiple -i. will also force -d switc)\n");
    fprintf(stderr, "\t-pi <ID> (just log specific Pantair ID, can use multiple -pi. will also force -d switch)\n");
    fprintf(stderr, "\t-r (raw)\n");
    fprintf(stderr, "\t-s (Serial Speed Test / OS caching issues)\n");
    fprintf(stderr, "\t-lpack (log RS packets to %s)\n",RS485LOGFILE);
    fprintf(stderr, "\t-lrawb (log raw RS bytes to %s)\n",RS485BYTELOGFILE);
    fprintf(stderr, "\t-e (monitor errors)\n");
    fprintf(stderr, "\t-a (Print all ID's the panel queried)\n");
    fprintf(stderr, "\t-t (time each packet, will also force -s switch)\n");
    fprintf(stderr, "\nie:\t%s /dev/ttyUSB0 -d -p 1000 -i 0x08 -i 0x0a\n\n", argv[0]);
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
      printf("Add Jandy filter %i 0x%02hhx\n",_filters, _filter[_filters-1]);
      logLevel = LOG_DEBUG; // no point in filtering on ID if we're not going to print it.
    } else if (strcmp(argv[i], "-pi") == 0 && i+1 < argc) {
      unsigned int n;
      sscanf(argv[i+1], "0x%2x", &n);
      _pfilter[_pfilters] = n;
      _pfilters++;
      printf("Add Pentair filter %i 0x%02hhx\n",_pfilters, _pfilter[_pfilters-1]);
      logLevel = LOG_DEBUG; // no point in filtering on ID if we're not going to print it.
    } else if (strcmp(argv[i], "-r") == 0) {
      _rawlog = true;
      logLevel = LOG_DEBUG;
    } else if (strcmp(argv[i], "-f") == 0) {
      _playback_file = true;
    } else if (strcmp(argv[i], "-lpack") == 0) {
      _aqconfig_.log_protocol_packets = true;
    } else if (strcmp(argv[i], "-lrawb") == 0) {
      _aqconfig_.log_raw_bytes = true;
    } else if (strcmp(argv[i], "-n") == 0) {
      panleProbe = false;
    } else if (strcmp(argv[i], "-s") == 0) {
      rsSerialSpeedTest = true;
      serialBlocking = false;
    } else if (strcmp(argv[i], "-e") == 0) {
      errorMonitor = true;
    } else if (strcmp(argv[i], "-a") == 0) {
      printAllIDs = true;
    } else if (strcmp(argv[i], "-t") == 0) {
      timePackets = true;
      logLevel = LOG_DEBUG;
    }
  }

#ifdef AQ_MANAGER
  setLoggingPrms(logLevel, false, NULL);
#else
  setLoggingPrms(logLevel, false, false, NULL);
#endif

  if (_playback_file) {
    rs_fd = open(argv[1], O_RDONLY | O_NOCTTY | O_NONBLOCK | O_NDELAY);
    if (rs_fd < 0)  {
      LOG(RSSD_LOG, LOG_ERR, "Unable to open file: %s\n", argv[1]);
      displayLastSystemError(argv[1]);
      return -1;
    }
  } else {
    if (!serialBlocking)
      rs_fd = init_serial_port(argv[1]);
    else
      rs_fd = init_blocking_serial_port(argv[1]);

    if (rs_fd < 0)  {
      LOG(RSSD_LOG, LOG_ERR, "Unable to open port: %s\n", argv[1]);
      displayLastSystemError(argv[1]);
      return -1;
    }
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  if (!errorMonitor) {
    LOG(RSSD_LOG, LOG_NOTICE, "Logging serial information!\n");
  } else {
    LOG(RSSD_LOG, LOG_NOTICE, "Logging serial errors!\n");
  }
  if (_aqconfig_.log_protocol_packets)
     LOG(RSSD_LOG, LOG_NOTICE, "Logging packets to %s!\n",RS485LOGFILE);
  if (_aqconfig_.log_raw_bytes)
     LOG(RSSD_LOG, LOG_NOTICE, "Logging raw bytes to %s!\n",RS485BYTELOGFILE);

  if (logLevel < LOG_DEBUG && errorMonitor==false )
    printf("Please wait.");

  startPacketLogger();

  _serial_logger(rs_fd, argv[1], logPackets, logLevel, panleProbe, rsSerialSpeedTest, errorMonitor, printAllIDs, timePackets);

  stopPacketLogger();

  close_serial_port(rs_fd);
}

#endif // SERIAL_LOGGER




int _serial_logger(int rs_fd, char *port_name, int logPackets, int logLevel, bool panleProbe, bool rsSerialSpeedTest, bool errorMonitor, bool printAllIDs, bool timePackets) {
  int packet_length;
  int last_packet_length = 0;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  unsigned char last_packet_buffer[AQ_MAXPKTLEN];
  unsigned char lastID = 0x00;
  int i = 0;
  bool found;
  serial_id_log slog[SLOG_MAX];
  serial_id_log pent_slog[SLOG_MAX];
  int sindex = 0;
  int pent_sindex = 0;
  int received_packets = 0;
  struct timespec start_time;
  struct timespec end_time;
  struct timespec elapsed;
  struct timespec packet_start_time;
  struct timespec packet_end_time;
  struct timespec packet_elapsed;
  char extra_message[64];
  int blankReads = 0;
  bool returnError = false;

  clock_gettime(CLOCK_REALTIME, &start_time);
  if (timePackets) {
    clock_gettime(CLOCK_REALTIME, &packet_start_time);
  }

  while (_keepRunning == true) {
    if (rs_fd < 0) {
      LOG(RSSD_LOG, LOG_ERR, "ERROR, serial port disconnect\n");
    }

    packet_length = get_packet(rs_fd, packet_buffer);

    if (timePackets) {
      clock_gettime(CLOCK_REALTIME, &packet_end_time);
      sl_timespec_subtract(&packet_elapsed, &packet_end_time, &packet_start_time);
      clock_gettime(CLOCK_REALTIME, &packet_start_time);
      sprintf(extra_message,"Time between packets (%.3f sec)\n", roundf3(timespec2float(&packet_elapsed)) );
    }

    if (packet_length == AQSERR_READ) {
      // Unrecoverable read error. Force an attempt to reconnect.
      LOG(RSSD_LOG, LOG_ERR, "ERROR, on serial port! Please check %s\n",port_name);
      _keepRunning = false;
      returnError = true;
    } else if (packet_length == AQSERR_TIMEOUT) {
      // Unrecoverable read error. Force an attempt to reconnect.
      LOG(RSSD_LOG, LOG_ERR, "ERROR, Timeout on serial port, nothing read! Please check %s\n",port_name);
      _keepRunning = false;
      returnError = true;
    } else if (packet_length < 0) {
      // Error condition
      if (errorMonitor && last_packet_length > 0) { // Error packet wwould have already been printed.
        char buff[900];
        beautifyPacket(buff, last_packet_buffer, last_packet_length, true);
        LOG(RSSD_LOG, LOG_NOTICE, "Previous packet (before error)\n");
        LOG(RSSD_LOG, LOG_NOTICE, "%s------------------------------\n",buff);
        //LOG(RSSD_LOG, LOG_NOTICE, "\n");
      }
    } else if (packet_length == 0) {
      // Nothing read
      if (++blankReads > (rsSerialSpeedTest?100000000:1000) ) {
        LOG(RSSD_LOG, LOG_ERR, "ERROR, too many blank reads! Please check %s\n",port_name);
        _keepRunning = false;
        returnError = true;
      }
      //if (!rsSerialSpeedTest)
        delay(1);
    } else if (packet_length > 0) {
        blankReads = 0;
        //LOG(RSSD_LOG, LOG_DEBUG_SERIAL, "Received Packet for ID 0x%02hhx of type %s\n", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
#ifdef SERIAL_LOGGER
        if (logLevel > LOG_NOTICE)
          printPacket(lastID, packet_buffer, packet_length, timePackets?extra_message:NULL);
#endif
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
            //printf("Added id 0x%02hhx\n",packet_buffer[PKT_DEST]);
            slog[sindex].ID = packet_buffer[PKT_DEST];
            slog[sindex].inuse = false;
            sindex++;
          }
         }
         
         if (packet_buffer[PKT_DEST] == DEV_MASTER /*&& packet_buffer[PKT_CMD] == CMD_ACK*/) {
          //LOG(RSSD_LOG, LOG_NOTICE, "ID is in use 0x%02hhx %x\n", lastID, lastID);
          for (i = 0; i <= sindex; i++) {
            if (slog[i].ID == lastID) {
              slog[i].inuse = true;
              break;
            }
          }
         }

         if (panleProbe && packet_buffer[PKT_DEST] == 0x58 ) {
           getPanelInfo(rs_fd, packet_buffer, packet_length);
         }
  
        lastID = packet_buffer[PKT_DEST];
      }
      received_packets++;

      // Test Serial speed & caching
      if (rsSerialSpeedTest) {
          packet_length = get_packet(rs_fd, packet_buffer);

        if (packet_length > 0 && packet_buffer[PKT_DEST] != 0x00)  {
          // Only test for packets from panel, when you test to panel you are timing reply.
          LOG(RSSD_LOG, LOG_ERR, "SERIOUS RS485 ERROR, Slow serial port read detected, (check RS485 adapteer / os performance / USB serial speed\n");
        }
      }

    }

#ifndef SERIAL_LOGGER
    if(received_packets%100==0) {
      LOG(RSSD_LOG, LOG_NOTICE, "Read %d of %d packets\n", received_packets, logPackets);
    }
#endif

    if (logPackets != 0 && received_packets >= logPackets) {
      _keepRunning = false;
    }
  
    if (errorMonitor) {
      if (packet_length > 0) {
        memcpy(last_packet_buffer, packet_buffer, packet_length);
        last_packet_length = packet_length;
        received_packets = 0;
      }
    } else if (logLevel < LOG_DEBUG) {
#ifdef SERIAL_LOGGER
      advance_cursor();
#endif
    }

    //sleep(1);
  }

  clock_gettime(CLOCK_REALTIME, &end_time);

  // If we were monitoring errors, or filtering messages, or no panel probe, don;t print details
  if (errorMonitor || panleProbe==false || _filters > 0 || _pfilters > 0) {
    return 0;
  } else if (returnError) {
    return 1;
  }

  sl_timespec_subtract(&elapsed, &end_time, &start_time);

  LOG(RSSD_LOG, LOG_DEBUG, "\n\n");
  if (logLevel < LOG_DEBUG)
    printf("\n\n");

  if (sindex >= SLOG_MAX)
    LOG(RSSD_LOG, LOG_ERR, "Ran out of storage, some ID's were not captured, please increase SLOG_MAX and recompile\n");

  if (elapsed.tv_sec > 0) {
    LOG(RSSD_LOG, LOG_NOTICE, "RS485 interface received %d packets in %d seconds (~%.2f Msg/Sec)\n", received_packets, elapsed.tv_sec, (received_packets / (float)elapsed.tv_sec) );
  }

  LOG(RSSD_LOG, LOG_NOTICE, "Jandy Control Panel Model   : %s\n", _panelType);
  LOG(RSSD_LOG, LOG_NOTICE, "Jandy Control Panel Version : %s\n", _panelRev);
  

  LOG(RSSD_LOG, LOG_NOTICE, "Jandy ID's found\n");
  for (i = 0; i < sindex; i++) {
    //LOG(RSSD_LOG, LOG_NOTICE, "ID 0x%02hhx is %s %s\n", slog[i].ID, (slog[i].inuse == true) ? "in use" : "not used",
    //           (slog[i].inuse == false && canUse(slog[i].ID) == true)? " <-- can use for Aqualinkd" : "");
    //if (logLevel >= LOG_DEBUG || slog[i].inuse == true || canUse(slog[i].ID) == true) {
    if (logLevel >= LOG_DEBUG || slog[i].inuse == true || canUse(slog[i].ID) == true || printAllIDs == true) {
      LOG(RSSD_LOG, LOG_NOTICE, "ID 0x%02hhx is %s %s\n", slog[i].ID, (slog[i].inuse == true) ? "in use  " : "not used",
               (slog[i].inuse == false)?canUseExtended(slog[i].ID):getDevice(slog[i].ID));
    }
  }

  if (pent_sindex > 0) {
    LOG(RSSD_LOG, LOG_NOTICE, "\n\n");
    LOG(RSSD_LOG, LOG_NOTICE, "Pentair ID's found\n");
  }
  for (i=0; i < pent_sindex; i++) {
    LOG(RSSD_LOG, LOG_NOTICE, "ID 0x%02hhx is %s %s\n", pent_slog[i].ID, (pent_slog[i].inuse == true) ? "in use  " : "not used",
               (pent_slog[i].inuse == false)?canUseExtended(pent_slog[i].ID):getPentairDevice(pent_slog[i].ID));
  }

  LOG(RSSD_LOG, LOG_NOTICE, "\n\n");


  char mainID = 0x00;
  char rssaID = 0x00;
  char extID = 0x00; 

  for (i = 0; i < sindex; i++) {
    if (slog[i].inuse == true)
      continue;
    
    if (!_panelPDA) {
      if (canUseAllB(slog[i].ID) && (mainID == 0x00 || canUsePDA(mainID))) 
        mainID = slog[i].ID;
      if (canUsePDA(slog[i].ID) && mainID == 0x00) 
        mainID = slog[i].ID;
      else if (canUseRSSA(slog[i].ID) && rssaID == 0x00) 
        rssaID = slog[i].ID;
      else if (canUseONET(slog[i].ID) && extID == 0x00) 
        extID = slog[i].ID;
      else if (canUseIQAT(slog[i].ID) && (extID == 0x00 || canUseONET(extID))) 
        extID = slog[i].ID;
    } else {
      if (canUsePDA(slog[i].ID) && mainID == 0x00) 
        mainID = slog[i].ID;
    }
    
  }
  LOG(RSSD_LOG, LOG_NOTICE, "Suggested aqualinkd.conf values\n");
  LOG(RSSD_LOG, LOG_NOTICE, "-------------------------\n");
  if (strlen (_panelType) > 0)
    LOG(RSSD_LOG, LOG_NOTICE, "panel_type = %s\n",_panelType);

  if (mainID != 0x00)
    LOG(RSSD_LOG, LOG_NOTICE, "device_id = 0x%02hhx\n",mainID);

  if (rssaID != 0x00)
    LOG(RSSD_LOG, LOG_NOTICE, "rssa_device_id = 0x%02hhx\n",rssaID);

  if (extID != 0x00)
    LOG(RSSD_LOG, LOG_NOTICE, "extended_device_id = 0x%02hhx\n",extID);

  LOG(RSSD_LOG, LOG_NOTICE, "-------------------------\n");

  return 0;
}





int sl_timespec_subtract (struct timespec *result, const struct timespec *x, const struct timespec *y)
{
  struct timespec tmp;

  memcpy (&tmp, y, sizeof(struct timespec));

  if (x->tv_nsec < tmp.tv_nsec)
    {
      int nsec = (tmp.tv_nsec - x->tv_nsec) / 1000000000 + 1;
      tmp.tv_nsec -= 1000000000 * nsec;
      tmp.tv_sec += nsec;
    }
  if (x->tv_nsec - tmp.tv_nsec > 1000000000)
    {
      int nsec = (x->tv_nsec - tmp.tv_nsec) / 1000000000;
      tmp.tv_nsec += 1000000000 * nsec;
      tmp.tv_sec -= nsec;
    }


  result->tv_sec = x->tv_sec - tmp.tv_sec;
  result->tv_nsec = x->tv_nsec - tmp.tv_nsec;


  return x->tv_sec < tmp.tv_sec;
}




/*********************************************
// Notes on PC Dock 

To start download.
Jandy     To 0x58 of type            Probe | HEX: 0x10|0x02|0x58|0x00|0x6a|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x00|0x00|0x2b|0x10|0x03|

Then the download is a lot of these.
Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x00|0x00|0x00|0x00|0x3c|0x00|0x00|0x64|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x05|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x29|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x01|0x00|0x2c|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x85|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x02|0x00|0x2d|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x02|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x86|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x03|0x00|0x2e|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x03|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x04|0x00|0x22|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xae|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x04|0x00|0x2f|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x04|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0x4f|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x05|0x00|0x30|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x05|0x00|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0xff|0x49|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x00|0x19|0x06|0x00|0x31|0x10|0x03|

End of download is (Not sure how it knows to send ACK and not 0x19 reply)
Jandy     To 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x58|0x1a|0x56|0x00|0x4d|0xc2|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x43|0x36|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xff|0xff|0xff|0x52|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x4f|0x10|0x03|
Jandy   From 0x58 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x57|0x00|0x6a|0x10|0x03|


To start upload
Jandy     To 0x58 of type            Probe | HEX: 0x10|0x02|0x58|0x00|0x6a|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x17' | HEX: 0x10|0x02|0x00|0x17|0x29|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x00|0x00|0x83|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x00|0x00|0x00|0x00|0x3c|0x00|0x00|0x64|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x05|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xd1|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x01|0x00|0x84|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x2d|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x02|0x00|0x85|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x02|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x2e|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x03|0x00|0x86|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x03|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x01|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x04|0x00|0x22|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x56|0x10|0x03|

Not sure on end, kind-a looks like control panel sends probe back.
Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x55|0x00|0xd8|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x55|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x01|0x00|0x01|0x00|0x57|0x50|0x77|0x72|0x20|0x43|0x6e|0x74|0x72|0x20|0x42|0x00|0x00|0x58|0x01|0xff|0x17|0x00|0x00|0x6c|0x17|0x00|0x00|0x6c|0x17|0x00|0x00|0x6c|0x17|0x00|0x00|0x6c|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x20|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xb0|0x10|0x03|

Jandy     To 0x58 of type   Unknown '0x19' | HEX: 0x10|0x02|0x58|0x19|0x56|0x00|0xd9|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x56|0x00|0x4d|0xc2|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x43|0x36|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xff|0xff|0xff|0x52|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xf7|0x10|0x03|

Jandy     To 0x58 of type            Probe | HEX: 0x10|0x02|0x58|0x00|0x6a|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x56|0x00|0x4d|0xc2|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x43|0x36|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xff|0xff|0xff|0x52|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xf7|0x10|0x03|

Jandy     To 0x58 of type            Probe | HEX: 0x10|0x02|0x58|0x00|0x6a|0x10|0x03|
Jandy   From 0x58 of type   Unknown '0x1a' | HEX: 0x10|0x02|0x00|0x1a|0x56|0x00|0x4d|0xc2|0x31|0x42|0x41|0x36|0x32|0x38|0x32|0x35|0x42|0x37|0x43|0x36|0x39|0x41|0x34|0x43|0x43|0x36|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xff|0xff|0xff|0x52|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xf7|0x10|0x03|

********************************************/
