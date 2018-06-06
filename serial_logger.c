
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
#define PACKET_MAX 10000

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

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
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
  //char buffer[256];
  bool idMode = true;

  if (getuid() != 0) {
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  //if (idMode)
    setLoggingPrms(LOG_DEBUG, false, false);
  //else
  //  setLoggingPrms(LOG_DEBUG_SERIAL, false, false);

  if (argc < 2) {
    logMessage(LOG_DEBUG, "ERROR, first param must be serial port, ie:-\n    %s /dev/ttyUSB0\n\n", argv[0]);
    return 1;
  }

  rs_fd = init_serial_port(argv[1]);

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  while (_keepRunning == true) {
    if (rs_fd < 0) {
      logMessage(LOG_DEBUG, "ERROR, serial port disconnect\n");
    }

    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_DEBUG, "ERROR, on serial port\n");
      _keepRunning = false;
    } else if (packet_length == 0) {
      // Nothing read
    } else if (packet_length > 0) {

        //logMessage(LOG_DEBUG_SERIAL, "Received Packet for ID 0x%02hhx of type %s\n", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
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

    if (received_packets >= PACKET_MAX) {
      _keepRunning = false;
    }
  }

  logMessage(LOG_DEBUG, "\n");
  if (sindex >= SLOG_MAX)
    logMessage(LOG_DEBUG, "Ran out of storage, some ID's were not captured, please increase SLOG_MAX and recompile\n");
  logMessage(LOG_DEBUG, "ID's found\n");
  for (i = 0; i <= sindex; i++) {
    logMessage(LOG_DEBUG, "ID 0x%02hhx is %s %s\n", slog[i].ID, slog[i].inuse == true ? "in use" : "not used",
               slog[i].inuse == false && canUse(slog[i].ID) == true ? " <-- can use for Aqualinkd" : "");
  }

  return 0;
}
