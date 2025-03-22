/*
*
*  Program to simulate devices to help debug messages. 
*  Not in release code / binary for AqualinkD
*
*/


#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <fcntl.h>
#include <time.h>

// #include "serial_logger.h"
#include "aq_serial.h"
#include "utils.h"
#include "packetLogger.h"
#include "rs_msg_utils.h"

#define CONFIG_C // Make us look like config.c when we load config.h so we get globals.
#include "config.h"


unsigned char DEVICE_ID = 0x70;

bool _keepRunning = true;
int _rs_fd;

void intHandler(int dummy)
{
  _keepRunning = false;
  LOG(SLOG_LOG, LOG_NOTICE, "Stopping!\n");
}

bool isAqualinkDStopping() {
  return !_keepRunning;
}

void process_heatpump_packet(unsigned char *packet_buffer, const int packet_length);

int main(int argc, char *argv[])
{
  int logLevel = LOG_INFO;

  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];

  int blankReads = 0;
  bool returnError = false;

  if (argc < 2 || access(argv[1], F_OK) == -1)
  {
    fprintf(stderr, "ERROR, first param must be valid serial port, ie:-\n\t%s /dev/ttyUSB0\n\n", argv[0]);
    return 1;
  }

  setLoggingPrms(logLevel, false, NULL);

  LOG(SLOG_LOG, LOG_NOTICE, "Starting %s\n", basename(argv[0]));

  // _rs_fd = init_serial_port(argv[1]);
  _rs_fd = init_blocking_serial_port(argv[1]);

  if (_rs_fd < 0)
  {
    LOG(SLOG_LOG, LOG_ERR, "Unable to open port: %s\n", argv[1]);
    displayLastSystemError(argv[1]);
    return -1;
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  // Force all packets to be printed.
  addDebugLogMask(RSSD_LOG);
  _aqconfig_.RSSD_LOG_filter[0] = DEVICE_ID;

  while (_keepRunning)
  {
    if (_rs_fd < 0)
    {
      LOG(SLOG_LOG, LOG_ERR, "ERROR, serial port disconnect\n");
      _keepRunning = false;
    }

    packet_length = get_packet(_rs_fd, packet_buffer);

    if (packet_length == AQSERR_READ)
    {
      // Unrecoverable read error. Force an attempt to reconnect.
      LOG(SLOG_LOG, LOG_ERR, "ERROR, on serial port! Please check %s\n", argv[1]);
      _keepRunning = false;
      returnError = true;
    }
    else if (packet_length == AQSERR_TIMEOUT)
    {
      // Unrecoverable read error. Force an attempt to reconnect.
      LOG(SLOG_LOG, LOG_ERR, "ERROR, Timeout on serial port, nothing read! Please check %s\n", argv[1]);
      _keepRunning = false;
      returnError = true;
    }
    else if (packet_length < 0)
    {
      // Error condition
      if (packet_length == AQSERR_CHKSUM)
      {
        LOG(SLOG_LOG, LOG_WARNING, "Checksum error\n");
      }
      else if (packet_length == AQSERR_2LARGE)
      {
        LOG(SLOG_LOG, LOG_WARNING, "Packet too large error\n");
      }
      else if (packet_length == AQSERR_2SMALL)
      {
        LOG(SLOG_LOG, LOG_WARNING, "Packet too small error\n");
      }
      else
      {
        LOG(SLOG_LOG, LOG_WARNING, "Unknown error reading packet\n");
      }
    }
    else if (packet_length == 0)
    {
      if (++blankReads > 10)
      {
        LOG(SLOG_LOG, LOG_ERR, "ERROR, too many blank reads! Please check %s\n", argv[1]);
        _keepRunning = false;
        returnError = true;
      }
      delay(1);
    }
    else if (packet_length > 0)
    {
      blankReads = 0;
      //debuglogPacket(SLOG_LOG, packet_buffer, packet_length, true, true);

      if (packet_buffer[PKT_DEST] == DEVICE_ID)
      {
        process_heatpump_packet(packet_buffer, packet_length);
      }
    }
  }

  LOG(SLOG_LOG, LOG_NOTICE, "Stopping!\n");

  close_serial_port(_rs_fd);

  if (returnError)
    return 1;

  return 0;
}


/*
* Reply to message 
*/


void process_heatpump_packet(unsigned char *packet_buffer, const int packet_length)
{
  //LOG(SLOG_LOG, LOG_NOTICE, "Replying to packet 0x%02hhx!\n",packet_buffer[PKT_DEST]);

  //  reply to off 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|
  static unsigned char hp_off[] = {0x00, 0x0d, 0x40, 0x00, 0x00};
  static unsigned char hp_heat[] = {0x00, 0x0d, 0x48, 0x00, 0x00};
  static unsigned char hp_cool[] = {0x00, 0x0d, 0x68, 0x00, 0x00};
  

  if (packet_buffer[PKT_CMD] == CMD_PROBE)
  {
    send_ack(_rs_fd, 0x00);
    LOG(SLOG_LOG, LOG_NOTICE, "Replied to Probe packet to 0x%02hhx with ACK\n",packet_buffer[PKT_DEST]);
  }
  else if (packet_buffer[3] == 0x0c)
  {
    if (packet_buffer[4] == 0x00)
    { // Off
      send_jandy_command(_rs_fd, hp_off, 5);
      LOG(SLOG_LOG, LOG_NOTICE, "Replied to OFF 0x%02hhx packet to 0x%02hhx with ACK\n",packet_buffer[4],packet_buffer[PKT_DEST]);
    } 
    else if (packet_buffer[4] == 0x09) // Heat
    { // Enable
      send_jandy_command(_rs_fd, hp_heat, 5);
      LOG(SLOG_LOG, LOG_NOTICE, "Replied to HEAT 0x%02hhx packet to 0x%02hhx with ACK\n",packet_buffer[4],packet_buffer[PKT_DEST]);
    }
    else if (packet_buffer[4] == 0x29) // Cool
    { // Enable
      send_jandy_command(_rs_fd, hp_cool, 5);
      LOG(SLOG_LOG, LOG_NOTICE, "Replied to COOL 0x%02hhx packet to 0x%02hhx with ACK\n",packet_buffer[4],packet_buffer[PKT_DEST]);
    }
    else 
    { // Enable
      LOG(SLOG_LOG, LOG_ERR, "************* Unknown State Request 0x%02hhx *************",packet_buffer[4]);
      LOG(SLOG_LOG, LOG_NOTICE, "NOT Replying to UNKNOWN 0x%02hhx packet to 0x%02hhx with ACK\n",packet_buffer[4],packet_buffer[PKT_DEST]);
      //send_jandy_command(_rs_fd, hp_unknown, 5);
    }
  }
}


