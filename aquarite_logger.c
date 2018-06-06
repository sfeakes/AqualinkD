
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
#define PACKET_MAX 10000000000

bool _keepRunning = true;

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
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
  
  printf("\n");
}

int main(int argc, char *argv[]) {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  unsigned char lastID;
  int received_packets = 0;


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
    received_packets++;

    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_DEBUG, "ERROR, on serial port\n");
      _keepRunning = false;
    } else if (packet_length == 0) {
      // Nothing read

    } else if (packet_length > 0) {
        if ((packet_buffer[PKT_DEST] == DEV_MASTER && (lastID == 0x50 || lastID == 0x58)) 
            || packet_buffer[PKT_DEST] == 0x50 || packet_buffer[PKT_DEST] == 0x58)
          printPacket(lastID, packet_buffer, packet_length);
        else if (packet_buffer[PKT_CMD] == CMD_MSG || packet_buffer[PKT_CMD] == CMD_MSG_LONG) {
          printf("  To 0x%02hhx of Type  Message | ",packet_buffer[PKT_DEST]);
          fwrite(packet_buffer + 4, 1, packet_length-7, stdout);
          printf("\n");
        } else {
          printPacket(lastID, packet_buffer, packet_length);
        }

        //send_messaged(0, 0x00, "AquaPure");
       
       /*
        if (received_packets == 10 || received_packets == 20) {
          send_test_cmd(rs_fd, 0x50, 0x11, 0x32, 0x7d);
          //send_test_cmd(rs_fd, 0x80, CMD_PROBE, NUL, NUL);
          //send_test_cmd(rs_fd, 0x50, 0x11, 0x0a, NUL); // Set salt to 10%
          //send_test_cmd(rs_fd, 0x50, 0x14, 0x01, 0x77); // Send initial string AquaPure (0x14 is the comand, others should be NUL)
           //0x10|0x02|0x50|0x14|0x01|0x77|0x10|0x03|
           //0x10|0x02|0x50|0x14|0x01|0x77|0x10|0x03|
          // 10 02 50 11 0a 7d 10 03

           // 0x11 is set %

          printf(" *** SENT TEST *** \n");
        }
       */
       lastID = packet_buffer[PKT_DEST];
       //received_packets++;

       //delay(100);
       sleep(1);
      }
    

    if (received_packets >= PACKET_MAX) {
      _keepRunning = false;
    }
  }

  return 0;
}
