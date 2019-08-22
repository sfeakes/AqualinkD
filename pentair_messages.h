
#ifndef PEN_MESSAGES_H_
#define PEN_MESSAGES_H_

#include <stdbool.h>

bool processPentairPacket(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);

#endif // PEN_MESSAGES_H_