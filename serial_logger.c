
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

typedef struct serial_id_log {
  unsigned char ID;
  bool inuse;
} serial_id_log;

bool _keepRunning = true;

unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
unsigned char _filter = 0x00;

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
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
  for (i = 0; i < strlen((char *)_goodID); i++) {
    if (ID == _goodID[i])
      return true;
  }
  return false;
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

int main(int argc, char *argv[]) {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  unsigned char lastID;
  int i = 0;
  bool found;
  serial_id_log slog[SLOG_MAX];
  int sindex = 0;
  int received_packets = 0;
  int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
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

        if (packet_buffer[PKT_DEST] == DEV_MASTER && packet_buffer[PKT_CMD] == CMD_ACK) {
          //logMessage(LOG_DEBUG_SERIAL, "ID is in use 0x%02hhx %x\n", lastID, lastID);
          for (i = 0; i <= sindex; i++) {
            if (slog[i].ID == lastID) {
              slog[i].inuse = true;
              break;
            }
          }
        }
      

      lastID = packet_buffer[PKT_DEST];
      received_packets++;
    }

    if (logPackets != 0 && received_packets >= logPackets) {
      _keepRunning = false;
    }
    if (logLevel < LOG_DEBUG)
      advance_cursor();

    //sleep(1);
  }

  logMessage(LOG_DEBUG, "\n");
  if (sindex >= SLOG_MAX)
    logMessage(LOG_ERR, "Ran out of storage, some ID's were not captured, please increase SLOG_MAX and recompile\n");
  logMessage(LOG_NOTICE, "ID's found\n");
  for (i = 0; i <= sindex; i++) {
    logMessage(LOG_NOTICE, "ID 0x%02hhx is %s %s\n", slog[i].ID, (slog[i].inuse == true) ? "in use" : "not used",
               (slog[i].inuse == false && canUse(slog[i].ID) == true)? " <-- can use for Aqualinkd" : "");
  }

  return 0;
}
