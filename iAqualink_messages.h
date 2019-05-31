
#ifndef IAQ_MESSAGES_H_
#define IAQ_MESSAGES_H_

#include <stdbool.h>

bool processiAqualinkMsg(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);

#endif // IAQ_MESSAGES_H_