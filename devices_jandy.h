#ifndef AQUAPURE_H_
#define AQUAPURE_H_

#include <stdbool.h>

#include "aqualink.h"

bool processJandyPacket(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);

bool processPacketToSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata, int swg_zero_ignore);
bool processPacketFromSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata);
bool processPacketToJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);
bool processPacketFromJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);
void processMissingAckPacketFromSWG(unsigned char destination, struct aqualinkdata *aqdata);
void processMissingAckPacketFromJandyPump(unsigned char destination, struct aqualinkdata *aqdata);

void get_swg_status_mqtt(struct aqualinkdata *aqdata, char *message, int *status, int *dzalert);
aqledstate get_swg_led_state(struct aqualinkdata *aqdata);

bool changeSWGpercent(struct aqualinkdata *aqdata, int percent);
void setSWGpercent(struct aqualinkdata *aqdata, int percent);
void setSWGoff(struct aqualinkdata *aqdata);
void setSWGenabled(struct aqualinkdata *aqdata);
bool setSWGboost(struct aqualinkdata *aqdata, bool on);
void setSWGdeviceStatus(struct aqualinkdata *aqdata, emulation_type requester, unsigned char status);

#endif // AQUAPURE_H_