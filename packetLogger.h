#ifndef PACKETLOGGER_H_
#define PACKETLOGGER_H_

#include <stdbool.h>
#include <stdint.h>

#define RS485LOGFILE "/tmp/RS485.log"
#define RS485BYTELOGFILE "/tmp/RS485raw.log"


void startPacketLogger(); // use what ever config has
void startPacketLogging(bool debug_protocol_packets, bool debug_raw_bytes); // Set custom options

void stopPacketLogger();
//void logPacket(unsigned char *packet_buffer, int packet_length, bool checksumerror);
//void logPacket(unsigned char *packet_buffer, int packet_length);
void logPacketRead(unsigned char *packet_buffer, int packet_length);
void logPacketWrite(unsigned char *packet_buffer, int packet_length);
void logPacketError(unsigned char *packet_buffer, int packet_length);
void logPacketByte(unsigned char *byte);

// Only use for manual debugging
//void debuglogPacket(unsigned char *packet_buffer, int packet_length);
void debuglogPacket(int16_t from, unsigned char *packet_buffer, int packet_length, bool is_read);
int beautifyPacket(char *buff, unsigned char *packet_buffer, int packet_length, bool is_read);

#endif //PACKETLOGGER_H_