
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "packetLogger.h"
#include "aq_serial.h"
#include "utils.h"


static FILE *_packetLogFile = NULL;
static FILE *_byteLogFile = NULL;
static bool _log2file = false;
static bool _includePentair = false;

void _logPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force);
int _beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool error);

void startPacketLogger(bool debug_RSProtocol_packets, bool read_pentair_packets) {
  _log2file = debug_RSProtocol_packets;
  _includePentair = read_pentair_packets;
}

void stopPacketLogger() {
  if (_packetLogFile != NULL)
    fclose(_packetLogFile);

  if (_byteLogFile != NULL)
    fclose(_byteLogFile);
}


void writePacketLog(char *buffer) {
  if (_packetLogFile == NULL)
    _packetLogFile = fopen(RS485LOGFILE, "w");

  if (_packetLogFile != NULL) {
    fputs(buffer, _packetLogFile);
  } 
}

void logPacket(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, false, false);
}

void logPacketError(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, true, false);
}

void debuglogPacket(int16_t from, unsigned char *packet_buffer, int packet_length) {
  if ( getLogLevel(from) >= LOG_DEBUG )
    _logPacket(from, packet_buffer, packet_length, false, true);
}

void _logPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force)
{
  // No point in continuing if loglevel is < debug_serial and not writing to file
  if ( force == false && error == false && getLogLevel(from) < LOG_DEBUG_SERIAL && _log2file == false)
    return;

  char buff[1000];
  //int i = 0;
  //int cnt = 0;

  _beautifyPacket(buff, packet_buffer, packet_length, error);
/*
  if (_includePentair) {
    cnt = sprintf(buff, "%s%8.8s Packet | HEX: ",(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==JANDY?"Jandy":"Pentair");
  } else {
    cnt = sprintf(buff, "%sTo 0x%02hhx of type %8.8s | HEX: ",(error?"BAD PACKET ":""), packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
  }

  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  cnt += sprintf(buff + cnt, "\n");
*/
  if (_log2file)
    writePacketLog(buff);
  
  if (error == true)
    LOG(from,LOG_WARNING, "%s", buff);
  else {
    if (force)
      LOG(from,LOG_DEBUG, "%s", buff);
    else
      LOG(from,LOG_DEBUG_SERIAL, "%s", buff);
  }
}

int beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length)
{
  return _beautifyPacket(buff, packet_buffer, packet_length, false);
}
int _beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool error)
{
  int i = 0;
  int cnt = 0;

  if (_includePentair) {
    cnt = sprintf(buff, "%s%8.8s Packet | HEX: ",(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==PENTAIR?"Pentair":"Jandy");
  } else {
    cnt = sprintf(buff, "%sTo 0x%02hhx of type %8.8s | HEX: ",(error?"BAD PACKET ":""), packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
  }

  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  cnt += sprintf(buff + cnt, "\n");

  return cnt;
}

//#define RAW_BUFFER_SIZE 100
// Log Raw Bytes
void logPacketByte(unsigned char *byte)
{
  char buff[10];
  //static int _length = 0;
  //static unsigned char _bytes[RAW_BUFFER_SIZE];

  //_bytes[_length++] = byte;

  //if (_length >= RAW_BUFFER_SIZE) {
    if (_byteLogFile == NULL)
      _byteLogFile = fopen(RS485BYTELOGFILE, "w");

    if (_byteLogFile != NULL) {
      sprintf(buff, "0x%02hhx|",*byte);
      fputs( buff, _byteLogFile);
    } 
  //}
}