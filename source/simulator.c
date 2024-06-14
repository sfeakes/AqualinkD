#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "aqualink.h"
#include "net_services.h"
#include "packetLogger.h"

#define MAX_STACK 20
int _sim_stack_place = 0;
unsigned char _sim_commands[MAX_STACK];

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
    _sim_commands[_sim_stack_place] = cmd;
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
    cmd = _sim_commands[0];
    _sim_stack_place--;
    memmove(&_sim_commands[0], &_sim_commands[1], sizeof(unsigned char) * _sim_stack_place ) ;
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

bool start_simulator(struct aqualinkdata *aqdata, emulation_type type) {

  // If we are a PDA panel and PDA sim, we are connected, so just set that
  // since PDA only panel can only handle one remote ID.
  if (isPDA_PANEL && type == AQUAPDA) {
    aqdata->simulator_active = type;
    aqdata->simulator_id = _aqconfig_.device_id;
  }

  // if type is same AND id is valid, sim is already started, their is nothing to do.
  if (aqdata->simulator_active == type) {
    if (aqdata->simulator_id >= 0x40 && aqdata->simulator_id <= 0x43) {
      LOG(SIM_LOG,LOG_NOTICE, "OneTouch Simulator already active!\n");
      return true;
    } else if (aqdata->simulator_id >= 0x08 && aqdata->simulator_id <= 0x0a) {
      LOG(SIM_LOG,LOG_NOTICE, "AllButton Simulator already active!\n");
      return true;
    } else if (aqdata->simulator_id >= 0x30 && aqdata->simulator_id <= 0x33) {
      LOG(SIM_LOG,LOG_NOTICE, "iAqualinkTouch Simulator already active!\n");
      return true;
    } else if (aqdata->simulator_id >= 0x60 && aqdata->simulator_id <= 0x63) {
      LOG(SIM_LOG,LOG_NOTICE, "PDA Simulator already active!\n");
      return true;
    }
  }

  // Check it's a valid request
  if (type == ALLBUTTON) {
    LOG(SIM_LOG,LOG_NOTICE, "Starting AllButton Simulator!\n");
  } else if (type == ONETOUCH) {
    LOG(SIM_LOG,LOG_NOTICE, "Starting OneTouch Simulator!\n");
  } else if (type == AQUAPDA ) {
    LOG(SIM_LOG,LOG_NOTICE, "Starting PDA Simulator!\n");
  } else if (type == IAQTOUCH) {
    LOG(SIM_LOG,LOG_NOTICE, "Starting iAqualinkTouch Simulator!\n");
  } else {
    LOG(SIM_LOG,LOG_ERR, "Request to start simulator of unknown type : %d", type);
    return false;
  }

  // start the simulator
  aqdata->simulator_active = type;
  aqdata->simulator_id = NUL;

  return true;
}

bool stop_simulator(struct aqualinkdata *aqdata) {
  aqdata->simulator_active = SIM_NONE;
  aqdata->simulator_id = NUL;

  LOG(SIM_LOG,LOG_DEBUG, "Stoped Simulator Mode\n");

  return true;
}

bool is_simulator_packet(struct aqualinkdata *aqdata, unsigned char *packet, int packet_length) {

    if ( (aqdata->simulator_active == ONETOUCH && packet[PKT_DEST] >= 0x40 && packet[PKT_DEST] <= 0x43) ||
         (aqdata->simulator_active == ALLBUTTON && packet[PKT_DEST] >= 0x08 && packet[PKT_DEST] <= 0x0a) ||
         (aqdata->simulator_active == IAQTOUCH && packet[PKT_DEST] >= 0x30 && packet[PKT_DEST] <= 0x33) ||
         (aqdata->simulator_active == AQUAPDA && packet[PKT_DEST] >= 0x60 && packet[PKT_DEST] <= 0x63) ) {
      return true;
    } else {
      return false;
    }
}