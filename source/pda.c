
/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#define _GNU_SOURCE 1 // for strcasestr & strptime
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "aqualink.h"

#include "pda.h"
#include "pda_menu.h"
#include "utils.h"
#include "aq_panel.h"
#include "packetLogger.h"
#include "devices_jandy.h"
#include "rs_msg_utils.h"

// Used in equiptment_update_cycle() for additional items on EQUIPMENT STATUS
// TOTAL_BUTTONS is at most 20 so bits 21-31 should be available
#define PDA_BOOST_INDEX           21
#define PDA_FREEZE_PROTECT_INDEX  22


// static struct aqualinkdata _aqualink_data;
static struct aqualinkdata *_aqualink_data;
static unsigned char _last_packet_type;
static unsigned long _pda_loop_cnt = 0;
static bool _initWithRS = false;

// Each RS message is around 0.25 seconds apart

#define PDA_SLEEP_FOR 120 // 30 seconds
#define PDA_WAKE_FOR 6 // ~1 seconds



void init_pda(struct aqualinkdata *aqdata)
{
  _aqualink_data = aqdata;
  //set_pda_mode(true);
}


bool pda_shouldSleep() {
  //LOG(PDA_LOG,LOG_DEBUG, "PDA loop count %d, will sleep at %d\n",_pda_loop_cnt,PDA_LOOP_COUNT);
  if (_pda_loop_cnt++ < PDA_WAKE_FOR) {
    return false;
  } else if (_pda_loop_cnt > PDA_WAKE_FOR + PDA_SLEEP_FOR) {
    _pda_loop_cnt = 0;
    return false;
  }

  // NSF NEED TO CHECK ACTIVE THREADS.
  if (_aqualink_data->active_thread.thread_id != 0) {
    LOG(PDA_LOG,LOG_DEBUG, "PDA can't sleep as thread %d,%p is active\n",
               _aqualink_data->active_thread.ptype,
               _aqualink_data->active_thread.thread_id);

    _pda_loop_cnt = 0;
    return false;
  }

  // Last see if there are any open websockets. (don't sleep if the web UI is open)
  if ( _aqualink_data->open_websockets > 0 ) {
    LOG(PDA_LOG,LOG_DEBUG, "PDA can't sleep as websocket is active\n");
    return false;
  }
  
  return true;
}

/*
bool pda_shouldSleep() {
  //LOG(PDA_LOG,LOG_DEBUG, "PDA loop count %d, will sleep at %d\n",_pda_loop_cnt,PDA_LOOP_COUNT);
  if (_pda_loop_cnt++ < PDA_LOOP_COUNT) {
    return false;
  } else if (_pda_loop_cnt > PDA_LOOP_COUNT*2) {
    _pda_loop_cnt = 0;
    return false;
  }

  return true;
}
*/

void pda_wake() {
  pda_reset_sleep();
  // Add and specic code to run when wake is called. 
}

void pda_reset_sleep() {
  _pda_loop_cnt = 0;
}

unsigned char get_last_pda_packet_type()
{
  return _last_packet_type;
}

void set_pda_led(struct aqualinkled *led, char state)
{
  aqledstate old_state = led->state;
  if (state == 'N')
  {
    led->state = ON;
  }
  else if (state == 'A')
  {
    led->state = ENABLE;
  }
  else if (state == '*')
  {
    led->state = FLASH;
  }
  else
  {
    led->state = OFF;
  }
  if (old_state != led->state)
  {
    LOG(PDA_LOG,LOG_DEBUG, "set_pda_led from %d to %d\n", old_state, led->state);
  }
}

// :TODO: Test what happens if there are more devices on than can fit on the status page
// :TODO: If web page is up PDA will not sleep therefore there is no wake and seeing
// the equipment page.  Need to add support for determining filter pump on/off based on home
void equiptment_update_cycle(int eqID) {
  // If you have a -1, it's a reset to clear / update information.
  // TOTAL_BUTTONS is 20 so bits 21-31 available for BOOST, FREEZE PROTECT, etc.
  int i;
  static uint32_t update_equiptment_bitmask = 0;

  if (eqID == -1) {
    LOG(PDA_LOG,LOG_DEBUG, "Start new equipment cycle bitmask 0x%04x\n",
        update_equiptment_bitmask);
   
    for (i=0; i < _aqualink_data->total_buttons - 2 ; i++) { // total_buttons - 2 because we don't get heaters in this cycle
      if ((update_equiptment_bitmask & (1 << (i))) != (1 << (i))) {
        if (_aqualink_data->aqbuttons[i].led->state != OFF) {
          _aqualink_data->aqbuttons[i].led->state = OFF;
          _aqualink_data->updated = true;
          LOG(PDA_LOG,LOG_DEBUG, "Turn off equipment id %d %s not seen in last cycle\n", i, _aqualink_data->aqbuttons[i].name);
        }
      }
    }

    if ((_aqualink_data->frz_protect_state == ON) &&
        (! (update_equiptment_bitmask & (1 << PDA_FREEZE_PROTECT_INDEX)))) {
        LOG(PDA_LOG,LOG_DEBUG, "Turn off freeze protect not seen in last cycle\n");
       _aqualink_data->frz_protect_state = ENABLE;
    }

    if ((_aqualink_data->boost) &&
        (! (update_equiptment_bitmask & (1 << PDA_BOOST_INDEX)))) {
        LOG(PDA_LOG,LOG_DEBUG, "Turn off BOOST not seen in last cycle\n");
      setSWGboost(_aqualink_data, false);
    }
    update_equiptment_bitmask = 0;
  } else if ((eqID >= 0) && (eqID < 32)) {
    update_equiptment_bitmask |= (1 << (eqID));
    char *eqName = NULL;
    if (eqID < TOTAL_BUTTONS) {
        eqName = _aqualink_data->aqbuttons[eqID].name;
    } else if (eqID == PDA_FREEZE_PROTECT_INDEX) {
        eqName = "FREEZE PROTECT";
    } else if (eqID == PDA_BOOST_INDEX) {
        eqName = "BOOST";
    } else {
        eqName = "UNKNOWN";
    }
    LOG(PDA_LOG,LOG_DEBUG, "Added equipment id %d %s to updated cycle bitmask 0x%04x\n",
        eqID, eqName, update_equiptment_bitmask);
  } else {
      LOG(PDA_LOG,LOG_ERR, "equiptment_update_cycle(%d) - Invalid eqID\n", eqID);
  }
}



void process_pda_packet_msg_long_temp(const char *msg)
{
  //           'AIR         POOL'
  //           ' 86`     86`    '
  //           'AIR   SPA       '
  //           ' 86` 86`        '
  //           'AIR             '
  //           ' 86`            '
  //           'AIR        WATER'  // In case of single device.
  _aqualink_data->temp_units = FAHRENHEIT; // Force FAHRENHEIT
  if (stristr(pda_m_line(1), "AIR") != NULL)
    _aqualink_data->air_temp = atoi(msg);

  if (stristr(pda_m_line(1), "SPA") != NULL)
  {
    _aqualink_data->spa_temp = atoi(msg + 4);
    _aqualink_data->pool_temp = TEMP_UNKNOWN;
  }
  else if (stristr(pda_m_line(1), "POOL") != NULL)
  {
    _aqualink_data->pool_temp = atoi(msg + 7);
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  else if (stristr(pda_m_line(1), "WATER") != NULL)
  {
    _aqualink_data->pool_temp = atoi(msg + 7);
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  else
  {
    _aqualink_data->pool_temp = TEMP_UNKNOWN;
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  // printf("Air Temp = %d | Water Temp = %d\n",atoi(msg),atoi(msg+7));
}

void process_pda_packet_msg_long_time(const char *msg)
{
  // message "     SAT 8:46AM "
  //         "     SAT 10:29AM"
  //         "     SAT 4:23PM "
  //         "     SUN  2:36PM"
  // printf("TIME = '%.*s'\n",AQ_MSGLEN,msg );
  // printf("TIME = '%c'\n",msg[AQ_MSGLEN-1] );

  if (msg[AQ_MSGLEN - 1] == ' ')
  {
    snprintf(_aqualink_data->time, sizeof(_aqualink_data->time), "%.6s", msg + 9);
  }
  else
  {
    snprintf(_aqualink_data->time, sizeof(_aqualink_data->time), "%.7s", msg + 9);
  }
  snprintf(_aqualink_data->date, sizeof(_aqualink_data->date), "%.3s", msg + 5);

  if (checkAqualinkTime() != true)
  {
    LOG(PDA_LOG,LOG_NOTICE, "RS time is NOT accurate '%s %s', re-setting on controller!\n", _aqualink_data->time, _aqualink_data->date);
    aq_programmer(AQ_SET_TIME, NULL, _aqualink_data);
  } 
}

void process_pda_packet_msg_long_equipment_control(const char *msg)
{
  // These are listed as "FILTER PUMP  OFF"
  int i;
  char labelBuff[AQ_MSGLEN + 1];
  strncpy(labelBuff, msg, AQ_MSGLEN - 4);
  labelBuff[AQ_MSGLEN - 4] = 0;

  LOG(PDA_LOG,LOG_DEBUG, "*** Checking Equiptment '%s'\n", labelBuff);

  for (i = 0; i < _aqualink_data->total_buttons; i++)
  {
    if (strcasecmp(stripwhitespace(labelBuff), _aqualink_data->aqbuttons[i].label) == 0)
    {
      LOG(PDA_LOG,LOG_DEBUG, "*** Found EQ CTL Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg);
      set_pda_led(_aqualink_data->aqbuttons[i].led, msg[AQ_MSGLEN - 1]);
      // Force SWG off if pump is off.
      if ((i==0) && (_aqualink_data->aqbuttons[0].led->state == OFF )) {
        setSWGoff(_aqualink_data);
      }
    }
  }

  // NSF I think we need to check TEMP1 and TEMP2 and set Pool HEater and Spa heater directly, to support single device.
  if (isSINGLE_DEV_PANEL){
    if (strcasecmp(stripwhitespace(labelBuff), "TEMP1") == 0)
      set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led, msg[AQ_MSGLEN - 1]);
    if (strcasecmp(stripwhitespace(labelBuff), "TEMP2") == 0)
      set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
}

void process_pda_packet_msg_long_home(const char *msg)
{
  if (stristr(msg, "POOL MODE") != NULL)
  {
    // If pool mode is on the filter pump is on but if it is off the filter pump might be on if spa mode is on.
    if (msg[AQ_MSGLEN - 1] == 'N')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = ON;
    }
    else if (msg[AQ_MSGLEN - 1] == '*')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = FLASH;
    }
  }
  else if (stristr(msg, "POOL HEATER") != NULL)
  {
    set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
  else if (stristr(msg, "SPA MODE") != NULL)
  {
    // when SPA mode is on the filter may be on or pending
    if (msg[AQ_MSGLEN - 1] == 'N')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = ON;
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = ON;
    }
    else if (msg[AQ_MSGLEN - 1] == '*')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = FLASH;
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = ON;
    }
    else
    {
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = OFF;
    }
  }
  else if (stristr(msg, "SPA HEATER") != NULL)
  {
    set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
}

void setSingleDeviceMode()
{
  if (isSINGLE_DEV_PANEL != true)
  {
    changePanelToMode_Only();
    LOG(PDA_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
  }
}

void process_pda_packet_msg_long_set_time(const char *msg)
{
/*
  // NOT Working at moment, also wrong format
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_packet_msg_long_set_temp\n");
  if (msg[4] == '/' && msg[7] == '/'){
    //DATE
    //rsm_strncpycut(_aqualink_data->date, msg, AQ_MSGLEN-1, AQ_MSGLEN-1);
    strncpy(_aqualink_data->date, msg + 11, 3);
  } else if (msg[6] == ':' && msg[11] == 'M') {
    // TIME
    //rsm_strncpycut(_aqualink_data->time, msg, AQ_MSGLEN-1, AQ_MSGLEN-1);
    if (msg[4] == ' ')
      strncpy(_aqualink_data->time, msg + 5, 6);
    else
  }
 */ 
}

void process_pda_packet_msg_long_set_temp(const char *msg)
{
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_packet_msg_long_set_temp\n");

  if (stristr(msg, "POOL HEAT") != NULL)
  {
    _aqualink_data->pool_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
  else if (stristr(msg, "SPA HEAT") != NULL)
  {
    _aqualink_data->spa_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }
  else if (stristr(msg, "TEMP1") != NULL)
  {
    setSingleDeviceMode();
    _aqualink_data->pool_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
  else if (stristr(msg, "TEMP2") != NULL)
  {
    setSingleDeviceMode();
    _aqualink_data->spa_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }

 
}

void process_pda_packet_msg_long_spa_heat(const char *msg)
{
  if (strncasecmp(msg, "    ENABLED     ", 16) == 0)
  {
    _aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led->state = ENABLE;
  }
  else if (strncasecmp(msg, "  SET TO", 8) == 0)
  {
    _aqualink_data->spa_htr_set_point = atoi(msg + 8);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }
}

void process_pda_packet_msg_long_pool_heat(const char *msg)
{
  if (strncasecmp(msg, "    ENABLED     ", 16) == 0)
  {
    _aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led->state = ENABLE;
  }
  else if (strncasecmp(msg, "  SET TO", 8) == 0)
  {
    _aqualink_data->pool_htr_set_point = atoi(msg + 8);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
}

void process_pda_packet_msg_long_freeze_protect(const char *msg)
{
  if (strncasecmp(msg, "TEMP      ", 10) == 0)
  {
    _aqualink_data->frz_protect_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "frz_protect_set_point = %d\n", _aqualink_data->frz_protect_set_point);
  }
}

void process_pda_packet_msg_long_SWG(int index, const char *msg)
{
  char *ptr = NULL;
  // Single Setpoint
  // PDA Line 0 =   SET AquaPure
  // PDA Line 1 =
  // PDA Line 2 =
  // PDA Line 3 =    SET TO 100%

  // PDA Line 0 =   SET AquaPure
  // PDA Line 1 =
  // PDA Line 2 =
  // PDA Line 3 =      SET TO: 20%

  // Dual Setpoint
  // PDA Line 0 =   SET AquaPure
  // PDA Line 1 =
  // PDA Line 2 =
  // PDA Line 3 = SET POOL TO: 45%
  // PDA Line 4 =  SET SPA TO:  0%

  // Note: use pda_m_line(index) instead of msg because it is NULL terminated
  if ((ptr = strcasestr(pda_m_line(index), "SET TO")) != NULL) {
    setSWGpercent(_aqualink_data, atoi(ptr+7));
    LOG(PDA_LOG,LOG_DEBUG, "swg_percent = %d\n", _aqualink_data->swg_percent);
  } else if ((ptr = strcasestr(pda_m_line(index), "SET SPA TO")) != NULL) {
    if (_aqualink_data->aqbuttons[SPA_INDEX].led->state != OFF) {
      setSWGpercent(_aqualink_data, atoi(ptr+11));
      LOG(PDA_LOG,LOG_DEBUG, "SPA swg_percent = %d\n", _aqualink_data->swg_percent);
    }
  } else if ((ptr = strcasestr(pda_m_line(index), "SET POOL TO")) != NULL) {
    if (_aqualink_data->aqbuttons[SPA_INDEX].led->state == OFF) {
      setSWGpercent(_aqualink_data, atoi(ptr + 12));
      LOG(PDA_LOG,LOG_DEBUG, "POOL swg_percent = %d\n", _aqualink_data->swg_percent);
    }
  } else if (index == 3) {
    LOG(PDA_LOG,LOG_ERR, "process msg SWG POOL idx %d unmatched %s\n", index, pda_m_line(index));
  }
}

void process_pda_packet_msg_long_unknown(const char *msg)
{
  int i;
  // Lets make a guess here and just see if there is an ON/OFF/ENA/*** at the end of the line
  // When you turn on/off a piece of equiptment, a clear screen followed by single message is sent.
  // So we are not in any PDA menu, try to catch that message here so we catch new device state ASAP.
  if (msg[AQ_MSGLEN - 1] == 'N' || msg[AQ_MSGLEN - 1] == 'F' || msg[AQ_MSGLEN - 1] == 'A' || msg[AQ_MSGLEN - 1] == '*')
  {
    for (i = 0; i < _aqualink_data->total_buttons; i++)
    {
      if (stristr(msg, _aqualink_data->aqbuttons[i].label) != NULL)
      {
        //LOG(PDA_LOG,LOG_ERR," UNKNOWN Found Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg);
        // This seems to keep everything off.
        set_pda_led(_aqualink_data->aqbuttons[i].led, msg[AQ_MSGLEN-1]);
      }
    }
  }
}

void pda_pump_update(struct aqualinkdata *aq_data, int updated) {
  const int bitmask[MAX_PUMPS] = {1,2,4,8};
  static unsigned char updates = '\0';
  int i;

  if (updated == -1) {
    for(i=0; i < MAX_PUMPS; i++) {
      if ((updates & bitmask[i]) != bitmask[i]) {
        aq_data->pumps[i].rpm = PUMP_OFF_RPM;
        aq_data->pumps[i].gpm = PUMP_OFF_GPM;
        aq_data->pumps[i].watts = PUMP_OFF_WAT;
      }
    }
    updates = '\0';
  } else if (updated >=0 && updated < MAX_PUMPS) {
     updates |= bitmask[updated];
  }
}

/*
// Messages from different PDA versions.
PDA Menu Line 0 = Equipment Status
PDA Menu Line 1 = 
PDA Menu Line 2 = Intelliflo VS 1 
PDA Menu Line 3 =      RPM: 1700  
PDA Menu Line 4 =     Watts: 367  
------------
PDA Menu Line 2 = JANDY ePUMP   1 
PDA Menu Line 3 =     RPM: 2520
PDA Menu Line 4 =   WATTS: 856
------------
PDA Menu Line 0 = EQUIPMENT STATUS
PDA Menu Line 1 = 
PDA Menu Line 2 = 
PDA Menu Line 3 =  *** PRIMING ***
PDA Menu Line 4 =   WATTS: 1303
------------
PDA Menu Line 4 =   WATTS: 1298
------------
PDA Menu Line 0 = Equipment Status
PDA Menu Line 1 =
PDA Menu Line 2 = Intelliflo VS 1
PDA Menu Line 3 =    (Offline)
----------
PDA Menu Line 0 = EQUIPMENT STATUS
PDA Menu Line 1 = 
PDA Menu Line 2 = JANDY ePUMP   1 
PDA Menu Line 3 =     RPM: 2520
PDA Menu Line 4 =   WATTS: 809
----------
*/
// NSF This is now VERY similar to onetouch function get_pumpinfo_from_menu(), should thinmk about combining in future
void get_pda_pumpinfo_from_menu(int menuLineIdx, int pump_number)
{
  int rpm = 0;
  int watts = 0;
  int gpm = 0;
  char *cidx = NULL;

  // valid controlpanel pump numbers are 1,2,3,4
  if (pump_number < 1 || pump_number > MAX_PUMPS) {
    LOG(PDA_LOG, LOG_WARNING, "Pump number %d for pump '%s' is invalid, ignoring!\n",pump_number,pda_m_line(menuLineIdx));
    return;
  }
  if ( (cidx = rsm_charafterstr(pda_m_line(menuLineIdx+1), "RPM", AQ_MSGLEN)) != NULL ){
    rpm = rsm_atoi(cidx);
    // Assuming Watts is always next line and GPM (if available) line after
    if ( (cidx = rsm_charafterstr(pda_m_line(menuLineIdx+2), "Watts", AQ_MSGLEN)) != NULL ){
      watts = rsm_atoi(cidx);
    }
    if ( (cidx = rsm_charafterstr(pda_m_line(menuLineIdx+3), "GPM", AQ_MSGLEN)) != NULL ){
      gpm = rsm_atoi(cidx);
    }
  }
  else if (rsm_strcmp(pda_m_line(menuLineIdx+1), "*** Priming ***") == 0){
    rpm = PUMP_PRIMING;
  }
  else if (rsm_strcmp(pda_m_line(menuLineIdx+1), "(Offline)") == 0){
    rpm = PUMP_OFFLINE;
  }
  else if (rsm_strcmp(pda_m_line(menuLineIdx+1), "(Priming Error)") == 0){
    rpm = PUMP_ERROR;
  }

  if (rpm==0 && watts==0 && rpm==0) {
    // Didn't get any info, so return.
    return;
  }
  LOG(PDA_LOG, LOG_DEBUG, "Found Pump information '%s', RPM %d, Watts %d, GPM %d\n", pda_m_line(menuLineIdx), rpm, watts, gpm);

  for (int i=0; i < _aqualink_data->num_pumps; i++) {
    if (_aqualink_data->pumps[i].pumpIndex == pump_number) {
      LOG(PDA_LOG,LOG_DEBUG, "Pump label: %s Index: %d, Number: %d, RPM: %d, Watts: %d, GPM: %d\n",_aqualink_data->pumps[i].button->name, i ,pump_number,rpm,watts,gpm);
      pda_pump_update(_aqualink_data, i);
      _aqualink_data->pumps[i].rpm = rpm;
      _aqualink_data->pumps[i].watts = watts;
      _aqualink_data->pumps[i].gpm = gpm;
      if (_aqualink_data->pumps[i].pumpType == PT_UNKNOWN){
        if (rsm_strcmp(pda_m_line(menuLineIdx),"Intelliflo VS") == 0)
          _aqualink_data->pumps[i].pumpType = VSPUMP;
        else if (rsm_strcmp(pda_m_line(menuLineIdx),"Intelliflo VF") == 0)
          _aqualink_data->pumps[i].pumpType = VFPUMP;
        else if (rsm_strcmp(pda_m_line(menuLineIdx),"Jandy ePUMP") == 0 ||
                   rsm_strcmp(pda_m_line(menuLineIdx),"ePump AC") == 0)
          _aqualink_data->pumps[i].pumpType = EPUMP;

        LOG(PDA_LOG, LOG_DEBUG, "Pump index %d set PumpType to %d\n", i, _aqualink_data->pumps[i].pumpType);
      }
      return;
    }
  }
  LOG(PDA_LOG,LOG_WARNING, "PDA Could not find config for Pump %s, Number %d, RPM %d, Watts %d, GPM %d\n",pda_m_line(menuLineIdx),pump_number,rpm,watts,gpm);
}

void log_pump_information() {
  int i;
  char *cidx = NULL;

  for (i = 0; i < PDA_LINES; i++)
  {
    if ( (cidx = rsm_charafterstr(pda_m_line(i), "Intelliflo VS", AQ_MSGLEN)) != NULL ||
         (cidx = rsm_charafterstr(pda_m_line(i), "Intelliflo VF", AQ_MSGLEN)) != NULL ||
         (cidx = rsm_charafterstr(pda_m_line(i), "Jandy ePUMP", AQ_MSGLEN)) != NULL ||
         (cidx = rsm_charafterstr(pda_m_line(i), "ePump AC", AQ_MSGLEN)) != NULL)
    {
      get_pda_pumpinfo_from_menu(i, rsm_atoi(cidx));
    }
    /* // NSF This need to be used in the future and not process_pda_packet_msg_long_equiptment_status()
    else if (rsm_strcmp(pda_m_line(i),"AQUAPURE") == 0) {
      rtn = get_aquapureinfo_from_menu(aq_data, i);
    } else if (rsm_strcmp(pda_m_line(i),"Chemlink") == 0) {
      rtn = get_chemlinkinfo_from_menu(aq_data, i);
    */
  }
}


void process_pda_packet_msg_long_equiptment_status(const char *msg_line, int lineindex, bool reset)
{
  LOG(PDA_LOG,LOG_DEBUG, "*** Pass Equiptment msg '%.16s'\n", msg_line);

  if (msg_line == NULL) {
    LOG(PDA_LOG,LOG_DEBUG, "*** Pass Equiptment msg is NULL do nothing\n");
    return;
  }

  static char *index;
  int i;
  char *msg = (char *)msg_line;
  while(isspace(*msg)) msg++;
  
    //strncpy(labelBuff, msg, AQ_MSGLEN);

  // EQUIPMENT STATUS
  //
  //  AquaPure 100%
  //  SALT 25500 PPM
  //   FILTER PUMP
  //    POOL HEAT
  //   SPA HEAT ENA

  // EQUIPMENT STATUS
  //
  //  FREEZE PROTECT
  //  AquaPure 100%
  //  SALT 25500 PPM
  //  CHECK AquaPure
  //  GENERAL FAULT
  //   FILTER PUMP
  //     CLEANER
  //
  // EQUIPMENT STATUS
  //
  //      BOOST
  //   23:59 REMAIN
  //  SALT 25500 PPM
  //   FILTER PUMP

  // VSP Pumps are not read here, since they are over multiple lines.

  // Check message for status of device
  // Loop through all buttons and match the PDA text.
  // Should probably use strncasestr
  if ((index = rsm_strncasestr(msg, "CHECK AquaPure", AQ_MSGLEN)) != NULL)
  {
    LOG(PDA_LOG,LOG_DEBUG, "CHECK AquaPure\n");
  }
  else if ((index = rsm_strncasestr(msg, "FREEZE PROTECT", AQ_MSGLEN)) != NULL)
  {
    _aqualink_data->frz_protect_state = ON;
    equiptment_update_cycle(PDA_FREEZE_PROTECT_INDEX);
    LOG(PDA_LOG,LOG_DEBUG, "Freeze Protect is on\n");
  }
  else if ((index = rsm_strncasestr(msg, "BOOST", AQ_MSGLEN)) != NULL)
  {
    setSWGboost(_aqualink_data, true);
    equiptment_update_cycle(PDA_BOOST_INDEX);
  }
  else if ((_aqualink_data->boost) && ((index = rsm_strncasestr(msg, "REMAIN", AQ_MSGLEN)) != NULL))
  {
    //snprintf(_aqualink_data->boost_msg, sizeof(_aqualink_data->boost_msg), "%s", msg+2);
    //Message is '  23:21 Remain', we only want time part 
    snprintf(_aqualink_data->boost_msg, 6, "%s", msg);
    _aqualink_data->boost_duration = rsm_HHMM2min(_aqualink_data->boost_msg);
  }
  else if ((index = rsm_strncasestr(msg, MSG_SWG_PCT, AQ_MSGLEN)) != NULL)
  {
    changeSWGpercent(_aqualink_data, atoi(index + strlen(MSG_SWG_PCT)));
    //_aqualink_data->swg_percent = atoi(index + strlen(MSG_SWG_PCT));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "AquaPure = %d\n", _aqualink_data->swg_percent);
  }
  else if ((index = rsm_strncasestr(msg, MSG_SWG_PPM, AQ_MSGLEN)) != NULL)
  {
    _aqualink_data->swg_ppm = atoi(index + strlen(MSG_SWG_PPM));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "SALT = %d\n", _aqualink_data->swg_ppm);
  }  
  else if (rsm_strncmp(msg_line, "POOL HEAT ENA",AQ_MSGLEN) == 0)
  {
      _aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led->state = ENABLE;
      LOG(PDA_LOG,LOG_DEBUG, "Pool Hearter is enabled\n");
      //equiptment_update_cycle(_aqualink_data->pool_heater_index);
  }
  else if (rsm_strncmp(msg_line, "SPA HEAT ENA",AQ_MSGLEN) == 0)
  {
      _aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led->state = ENABLE;
      LOG(PDA_LOG,LOG_DEBUG, "Spa Hearter is enabled\n");
      //equiptment_update_cycle(_aqualink_data->spa_heater_index);
  }
  else
  {
      for (i = 0; i < _aqualink_data->total_buttons; i++)
      {
        //LOG(PDA_LOG,LOG_DEBUG, "*** check msg '%s' against '%s'\n",labelBuff,_aqualink_data->aqbuttons[i].label);
        //LOG(PDA_LOG,LOG_DEBUG, "*** check msg '%.*s' against '%s'\n",AQ_MSGLEN,msg_line,_aqualink_data->aqbuttons[i].label);
        if (rsm_strncmp(msg_line, _aqualink_data->aqbuttons[i].label, AQ_MSGLEN-1) == 0)
        //if (rsm_strcmp(_aqualink_data->aqbuttons[i].label, labelBuff) == 0)
        {
          equiptment_update_cycle(i);
          LOG(PDA_LOG,LOG_DEBUG, "Found Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg_line);
          // It's on (or delayed) if it's listed here.
          if (_aqualink_data->aqbuttons[i].led->state != FLASH)
          {
            _aqualink_data->aqbuttons[i].led->state = ON;
          }
          break;
        }
      }
  }
  


}

void process_pda_packet_msg_long_level_aux_device(const char *msg)
{
#ifdef BETA_PDA_AUTOLABEL
  int li=-1;
  char *str, *label;

  if (! _aqconfig_->use_panel_aux_labels)
    return;
  // NSF  Need to check config for use_panel_aux_labels value and ignore if not set

  // Only care once we have the full menu, so check line 
  //  PDA Line 0 =    LABEL AUX1   
  //  PDA Line 1 =     
  //  PDA Line 2 =   CURRENT LABEL 
  //  PDA Line 3 =       AUX1          


  if ( (strlen(pda_m_line(3)) > 0 ) &&  
     (strncasecmp(pda_m_line(2),"  CURRENT LABEL ", 16) == 0) && 
     (strncasecmp(pda_m_line(0),"   LABEL AUX", 12) == 0)                           ) {
    str = pda_m_line(0);
    li = atoi(&str[12] );  // 12 is a guess, need to check on real system
    if (li > 0) {
      str = cleanwhitespace(pda_m_line(3));
      label = (char*)malloc(strlen(str)+1);
      strcpy ( label, str );
      _aqualink_data->aqbuttons[li-1].label = label;
    } else {
      LOG(PDA_LOG,LOG_ERR, "PDA couldn't get AUX? number\n", pda_m_line(0));
    }
  }
#endif
}

void process_pda_freeze_protect_devices()
{
  //  PDA Line 0 =  FREEZE PROTECT
  //  PDA Line 1 =     DEVICES
  //  PDA Line 2 =
  //  PDA Line 3 = FILTER PUMP    X
  //  PDA Line 4 = SPA
  //  PDA Line 5 = CLEANER        X
  //  PDA Line 6 = POOL LIGHT
  //  PDA Line 7 = SPA LIGHT
  //  PDA Line 8 = EXTRA AUX
  //  PDA Line 9 =
  int i;
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_freeze_protect_devices\n");
  for (i = 1; i < PDA_LINES; i++)
  {
    if (pda_m_line(i)[AQ_MSGLEN - 1] == 'X')
    {
      LOG(PDA_LOG,LOG_DEBUG, "PDA freeze protect enabled by %s\n", pda_m_line(i));
      if (_aqualink_data->frz_protect_state == OFF)
      {
        _aqualink_data->frz_protect_state = ENABLE;
        break;
      }
    }
  }
}

bool process_pda_packet(unsigned char *packet, int length)
{
  bool rtn = true;
  //int i;
  char *msg;
  int index = -1;
  static bool equiptment_update_loop = false;
  static bool read_equiptment_menu = false;

  _aqualink_data->last_packet_type = packet[PKT_CMD];

  process_pda_menu_packet(packet, length, in_programming_mode(_aqualink_data));


  switch (packet[PKT_CMD])
  {
    case CMD_ACK:
      LOG(PDA_LOG,LOG_DEBUG, "RS Received ACK length %d.\n", length);
    break;

    case CMD_PDA_CLEAR:
      read_equiptment_menu = false; // Reset the have read menu flag, since this is new menu.
    break;

    case CMD_STATUS:
      _aqualink_data->last_display_message[0] = '\0';
      if (equiptment_update_loop == false && pda_m_type() == PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA Start new Equiptment loop ****\n");
        equiptment_update_loop = true;
        // Need to process the equiptment full MENU here
      }
      else if (read_equiptment_menu == false && equiptment_update_loop == true && pda_m_type() == PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA read Equiptment page ****\n");
        log_pump_information();
        read_equiptment_menu = true;
      }
      else if (equiptment_update_loop == true && pda_m_type() != PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA End Equiptment loop ****\n");
        // Moved onto different MENU.  Probably need to update any pump changes
        equiptment_update_loop = false;

        // End of equiptment status chain of menus, reset any pump or equiptment that wasn't listed in menus as long as we are not in programming mode
        if (!in_programming_mode(_aqualink_data) ) {
          pda_pump_update(_aqualink_data, -1);
          equiptment_update_cycle(-1);
        }
      } 
      else if (_initWithRS == false && pda_m_type() == PM_FW_VERSION) 
      {
        _initWithRS = true;
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA INIT ****\n");
        //printf("**** PDA INIT PUT BACK IN ****\n");
        queueGetProgramData(AQUAPDA, _aqualink_data);
      }
    break;

    case CMD_MSG_LONG:
      msg = (char *)packet + PKT_DATA + 1;
      index = packet[PKT_DATA] & 0xF;
      if (packet[PKT_DATA] == 0x82)
      { // Air & Water temp is always this ID
        process_pda_packet_msg_long_temp(msg);
      }
      else if (packet[PKT_DATA] == 0x40)
      { // Time is always on this ID
        process_pda_packet_msg_long_time(msg);
      }
      else 
      {
        switch (pda_m_type()) 
        {
          case PM_EQUIPTMENT_CONTROL:
            process_pda_packet_msg_long_equipment_control(msg);
          break;
          case PM_HOME:
          case PM_BUILDING_HOME:
            process_pda_packet_msg_long_home(msg);
          break;
          case PM_EQUIPTMENT_STATUS:
            // NSF In the future need to run over this like log_pump_information()
            // So move into that function.  This way you can get status of devices 
            // that span over multiple lines (like AQUAPURE).
            // Plus tell if a device is not seen in loop, so can turn it off.
            process_pda_packet_msg_long_equiptment_status(msg, index, false);
          break;
          case PM_SET_TEMP:
            process_pda_packet_msg_long_set_temp(msg);
          break;
          case PM_SPA_HEAT:
            process_pda_packet_msg_long_spa_heat(msg);
          break;
          case PM_POOL_HEAT:
            process_pda_packet_msg_long_pool_heat(msg);
          break;
          case PM_FREEZE_PROTECT:
            process_pda_packet_msg_long_freeze_protect(msg);
          break;
          case PM_AQUAPURE:
            process_pda_packet_msg_long_SWG(index, msg);
          break;
          case PM_AUX_LABEL_DEVICE:
            process_pda_packet_msg_long_level_aux_device(msg);
          break;
          case PM_SET_TIME:
            process_pda_packet_msg_long_set_time(msg);
          break;
          //case PM_FW_VERSION:
          //  process_pda_packet_msg_long_FW_version(msg);
          //break;
          case PM_UNKNOWN:
          default:
            process_pda_packet_msg_long_unknown(msg);
          break;
        }
      }
    break;

    case CMD_PDA_0x1B:
      LOG(PDA_LOG,LOG_DEBUG, "**** CMD_PDA_0x1B ****\n");
    // We get two of these on startup, one with 0x00 another with 0x01 at index 4.  Just act on one.
    // Think this is PDA finishd showing startup screen
      if (packet[4] == 0x00) { 
        if (_initWithRS == false)
        {
          _initWithRS = true;
          LOG(PDA_LOG,LOG_DEBUG, "**** PDA INIT ****\n");
        //aq_programmer(AQ_PDA_INIT, NULL, _aqualink_data);
          queueGetProgramData(AQUAPDA, _aqualink_data);
          delay(50);  // Make sure this one runs first.
#ifdef BETA_PDA_AUTOLABEL
          if (_aqconfig_->use_panel_aux_labels)
             aq_programmer(AQ_GET_AUX_LABELS, NULL, _aqualink_data);
#endif
          aq_programmer(AQ_PDA_WAKE_INIT, NULL, _aqualink_data);
        } else {
          LOG(PDA_LOG,LOG_DEBUG, "**** PDA WAKE INIT ****\n");
          aq_programmer(AQ_PDA_WAKE_INIT, NULL, _aqualink_data);
        }
      }
    break;
  }

  if (packet[PKT_CMD] == CMD_MSG_LONG || packet[PKT_CMD] == CMD_PDA_HIGHLIGHT || 
      packet[PKT_CMD] == CMD_PDA_SHIFTLINES || packet[PKT_CMD] == CMD_PDA_CLEAR ||
      packet[PKT_CMD] == CMD_PDA_HIGHLIGHTCHARS)
  {
    // We processed the next message, kick any threads waiting on the message.
    kick_aq_program_thread(_aqualink_data, AQUAPDA);
  }
  
  // HERE AS A TEST.  NEED TO FULLY TEST THIS IS GOES TO PROD.
  else if (packet[PKT_CMD] == CMD_STATUS)
  {
    kick_aq_program_thread(_aqualink_data, AQUAPDA);
  }
  

  return rtn;
}

