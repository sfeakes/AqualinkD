
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
static unsigned char _lastReadFrom = NUL;

void _logPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force, bool is_read);
int _beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool error, bool is_read);

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

void debuglogPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool is_read) {
  if ( getLogLevel(from) >= LOG_DEBUG )
    _logPacket(from, packet_buffer, packet_length, false, true, is_read);
}

void _logPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool error, bool force, bool is_read)
{
  // No point in continuing if loglevel is < debug_serial and not writing to file
  if ( force == false && 
       error == false && 
       getLogLevel(from) < LOG_DEBUG_SERIAL && 
       /*_logfile_raw == false &&*/ 
       _logfile_packets == false ) {
    return;
  }
  
  if ( _aqconfig_.RSSD_LOG_filter != NUL ) {
    if (is_read) {
      _lastReadFrom = packet_buffer[PKT_DEST];
      if ( is_read && _aqconfig_.RSSD_LOG_filter != packet_buffer[PKT_DEST]) {
        return;
      }
    } else if (!is_read && _lastReadFrom != _aqconfig_.RSSD_LOG_filter) { // Must be write
      return;
    }
/*
    if ( is_read && _aqconfig_.RSSD_LOG_filter != packet_buffer[PKT_DEST]) {
      return;
    }
*/
  }

  char buff[1000];

  _beautifyPacket(buff, packet_buffer, packet_length, error, is_read);

  if (_logfile_packets)
    writePacketLog(buff);

  if (error == true)
    LOG(from,LOG_WARNING, "%s", buff);
  else {
    if (force)
      LOG(from,LOG_DEBUG, "%s", buff);
    //else if (is_read &&  _aqconfig_.serial_debug_filter != NUL && _aqconfig_.serial_debug_filter == packet_buffer[PKT_DEST])
    //  LOG(from,LOG_NOTICE, "%s", buff);
    else
      LOG(from,LOG_DEBUG_SERIAL, "%s", buff);
  }
}

int beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool is_read)
{
  return _beautifyPacket(buff, packet_buffer, packet_length, false, is_read);
}
int _beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool error, bool is_read)
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
  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  cnt += sprintf(buff + cnt, "\n");

  return cnt;
}

