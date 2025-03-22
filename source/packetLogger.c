
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "packetLogger.h"
#include "aq_serial.h"
#include "utils.h"
#include "config.h"

static FILE *_packetLogFile  = NULL;
static FILE *_byteLogFile    = NULL;
static bool _logfile_raw     = false;
static bool _logfile_packets = false;
//static bool _includePentair = false;
//static unsigned char _lastReadFrom = NUL;

void _logPacket(logmask_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force, bool is_read);
int _beautifyPacket(char *buff, int buff_size, unsigned char *packet_buffer, int packet_length, bool error, bool is_read);

//void startPacketLogger(bool debug_RSProtocol_packets) {
void startPacketLogger() {
  // Make local copy of variables so we can turn on/off as needed.
  _logfile_raw = _aqconfig_.log_raw_bytes;
  _logfile_packets = _aqconfig_.log_protocol_packets;
}

void startPacketLogging(bool log_protocol_packets, bool log_raw_bytes)
{
  _logfile_raw = log_raw_bytes;
  _logfile_packets = log_protocol_packets;
}

void stopPacketLogger() {
  if (_packetLogFile != NULL)
    fclose(_packetLogFile);

  if (_byteLogFile != NULL)
    fclose(_byteLogFile);

  _logfile_raw = false;
  _logfile_packets = false;
}

// Log passed packets
void writePacketLog(char *buffer) {
  if (!_logfile_packets)
    return;

  if (_packetLogFile == NULL)
    _packetLogFile = fopen(RS485LOGFILE, "w");

  if (_packetLogFile != NULL) {
    fputs(buffer, _packetLogFile);
  } 
}

// Log Raw Bytes
void logPacketByte(unsigned char *byte)
{
  if (!_logfile_raw)
    return;

  char buff[10];
  if (_byteLogFile == NULL)
    _byteLogFile = fopen(RS485BYTELOGFILE, "w");

  if (_byteLogFile != NULL) {
    sprintf(buff, "0x%02hhx|",*byte);
    fputs( buff, _byteLogFile);
  } 
  //}
}

/*
void logPacket(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, false, false);
}
*/
void logPacketRead(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, false, false, true);
}
void logPacketWrite(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, false, false, false);
}

void logPacketError(unsigned char *packet_buffer, int packet_length) {
  _logPacket(RSSD_LOG, packet_buffer, packet_length, true, false, true);
}

/* This should never be used in production */
void debuglogPacket(logmask_t from, unsigned char *packet_buffer, int packet_length, bool is_read, bool forcelog) {
  if ( forcelog == true || getLogLevel(from) >= LOG_DEBUG )
    _logPacket(from, packet_buffer, packet_length, false, forcelog, is_read);
}

void logPacket(logmask_t from, int level, unsigned char *packet_buffer, int packet_length, bool is_read) {
  if ( getLogLevel(from) >= level )
    _logPacket(from, packet_buffer, packet_length, false, false, is_read);
}

bool RSSD_LOG_filter_match(unsigned char ID) {
  for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
    if (_aqconfig_.RSSD_LOG_filter[i] != NUL && _aqconfig_.RSSD_LOG_filter[i] == ID) {
      return true;
    }
  }
  return false;
}

void _logPacket(logmask_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force, bool is_read)
{
  static unsigned char lastPacketTo = NUL;

  // No point in continuing if loglevel is < debug_serial and not writing to file
  if ( force == false && 
       error == false && 
       getLogLevel(from) < LOG_DEBUG_SERIAL && 
       /*_logfile_raw == false &&*/ 
       _logfile_packets == false ) {
    return;
  }
  

  if ( _aqconfig_.RSSD_LOG_filter[0] != NUL ) {
    // NOTE Whole IF statment is reversed
    //if ( ! ( (_aqconfig_.RSSD_LOG_filter_OLD == packet_buffer[PKT_DEST]) ||
    //         ( packet_buffer[PKT_DEST] == 0x00 && lastPacketTo == _aqconfig_.RSSD_LOG_filter_OLD)) ) 
    if ( ! ( (RSSD_LOG_filter_match(packet_buffer[PKT_DEST])) ||
             ( packet_buffer[PKT_DEST] == 0x00 && RSSD_LOG_filter_match(lastPacketTo) )) ) 
    {
      lastPacketTo = packet_buffer[PKT_DEST];
      return;
    }
    lastPacketTo = packet_buffer[PKT_DEST];
  }

#ifndef DUMMY_DEVICE // Not interested if in dummy_device
  if (is_read)
    LOG(from,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",packet_length);
  else
    LOG(from,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",packet_length);
#endif
  //char buff[1000];
  char buff[LARGELOGBUFFER];

  int len = _beautifyPacket(buff, LARGELOGBUFFER, packet_buffer, packet_length, error, is_read);

  if (_logfile_packets)
    writePacketLog(buff);

  if (error == true)
    LOG_LARGEMSG(from,LOG_WARNING, buff, len);
  else {
    if (force) {
      LOG_LARGEMSG(from, getSystemLogLevel()<LOG_DEBUG?getSystemLogLevel():LOG_DEBUG, buff, len);
      //LOG_LARGEMSG(from, LOG_DEBUG, buff, len);
    }
    //else if (is_read &&  _aqconfig_.serial_debug_filter != NUL && _aqconfig_.serial_debug_filter == packet_buffer[PKT_DEST])
    //  LOG(from,LOG_NOTICE, "%s", buff);
    else {
      LOG_LARGEMSG(from,LOG_DEBUG_SERIAL, buff, len);
    }
  }
}

int beautifyPacket(char *buff, int buff_size, unsigned char *packet_buffer, int packet_length, bool is_read)
{
  return _beautifyPacket(buff, buff_size, packet_buffer, packet_length, false, is_read);
}
int _beautifyPacket(char *buff, int buff_size, unsigned char *packet_buffer, int packet_length, bool error, bool is_read)
{
  int i = 0;
  int cnt = 0;
  protocolType ptype = getProtocolType(packet_buffer);

  cnt = sprintf(buff, "%s %s packet %sTo 0x%02hhx of type %16.16s | HEX: ",
                      (is_read?"Read ":"Write"),
                      (ptype==PENTAIR?"Pentair":"Jandy  "),
                      (error?"BAD PACKET ":""), 
                      packet_buffer[(ptype==JANDY?PKT_DEST:PEN_PKT_DEST)], 
                      get_packet_type(packet_buffer, packet_length));
/*
  if (getProtocolType(packet_buffer)==PENTAIR) {
    // Listing Jandy below if redundant.  need to clean this up.
    cnt = sprintf(buff, "%5.5s %s%8.8s Packet | HEX: ",(is_read?"Read":"Write"),(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==PENTAIR?"Pentair":"Jandy");
  } else {
    cnt = sprintf(buff, "%5.5s %sTo 0x%02hhx of type %16.16s | HEX: ",(is_read?"Read":"Write"),(error?"BAD PACKET ":""), packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
  }
*/
  for (i = 0; i < packet_length; i++) {
    // Check we have enough space for next set of chars
    if ( (cnt + 6) > buff_size)
      break;

    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);
  }

  cnt += sprintf(buff + cnt, "\n");

  return cnt;
}

