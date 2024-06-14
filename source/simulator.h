#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <stdbool.h>

bool processSimulatorPacket(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata);
//unsigned char pop_simulator_cmd(struct aqualinkdata *aq_data);
unsigned char pop_simulator_cmd(unsigned char receive_type);
int simulator_cmd_length();
//bool push_simulator_cmd(unsigned char cmd);
void simulator_send_cmd(unsigned char cmd);

bool start_simulator(struct aqualinkdata *aqdata, emulation_type type);
bool stop_simulator(struct aqualinkdata *aqdata);

bool is_simulator_packet(struct aqualinkdata *aqdata, unsigned char *packet, int packet_length);

#endif // SIMULATOR_H_