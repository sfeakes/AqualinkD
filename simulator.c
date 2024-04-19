#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "aqualink.h"
#include "net_services.h"
#include "packetLogger.h"

#define MAX_STACK 20
int _sim_stack_place = 0;
unsigned char _commands[MAX_STACK];

bool push_simulator_cmd(unsigned char cmd);

int simulator_cmd_length()
{
  return _sim_stack_place;
}

// External command
void simulator_send_cmd(unsigned char cmd)
{
  push_simulator_cmd(cmd);
}

bool push_simulator_cmd(unsigned char cmd)
{
  if (_sim_stack_place < MAX_STACK) {
    _commands[_sim_stack_place] = cmd;
    _sim_stack_place++;
  } else {
    LOG(SIM_LOG, LOG_ERR, "Command queue overflow, too many unsent commands to RS control panel\n");
    return false;
  }

  return true;
}

unsigned char pop_simulator_cmd(unsigned char receive_type)
{
  unsigned char cmd = NUL;

  if (_sim_stack_place > 0 && receive_type == CMD_STATUS ) {
    cmd = _commands[0];
    _sim_stack_place--;
    memmove(&_commands[0], &_commands[1], sizeof(unsigned char) * _sim_stack_place ) ;
  }

  LOG(SIM_LOG,LOG_DEBUG, "Sending '0x%02hhx' to controller\n", cmd);
  return cmd;
}

bool processSimulatorPacket(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata) 
{
  // copy packed into buffer to be sent to web
  //memset(aqdata->simulator_packet, 0, sizeof aqdata->simulator_packet);
  memcpy(aqdata->simulator_packet, packet, packet_length);
  aqdata->simulator_packet_length = packet_length;

  if ( getLogLevel(SIM_LOG) >= LOG_DEBUG ) {
    char buff[1000];
    //sprintf("Sending control command:")
    beautifyPacket(buff, packet, packet_length, false);
    LOG(SIM_LOG,LOG_DEBUG, "Received message : %s", buff);
  }

  broadcast_simulator_message();

  return true;
}