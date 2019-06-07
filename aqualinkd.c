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
#define __USE_XOPEN 1
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <termios.h>
#include <signal.h>

#include <time.h> // Need GNU_SOURCE & XOPEN defined for strptime

#include "mongoose.h"
#include "aqualink.h"
#include "utils.h"
#include "config.h"
#include "aq_serial.h"
#include "init_buttons.h"
#include "aq_programmer.h"
#include "net_services.h"
#include "pda_menu.h"
#include "pda.h"
#include "pentair_messages.h"
#include "version.h"

//#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;
static struct aqconfig _config_parameters;
static struct aqualinkdata _aqualink_data;

void main_loop();

void intHandler(int dummy)
{
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
  if (dummy){}// stop compile warnings
}

void processLEDstate()
{

  int i = 0;
  int byte;
  int bit;

  for (byte = 0; byte < 5; byte++)
  {
    for (bit = 0; bit < 8; bit += 2)
    {
      if (((_aqualink_data.raw_status[byte] >> (bit + 1)) & 1) == 1)
        _aqualink_data.aqualinkleds[i].state = FLASH;
      else if (((_aqualink_data.raw_status[byte] >> bit) & 1) == 1)
        _aqualink_data.aqualinkleds[i].state = ON;
      else
        _aqualink_data.aqualinkleds[i].state = OFF;

      //logMessage(LOG_DEBUG,"Led %d state %d",i+1,_aqualink_data.aqualinkleds[i].state);
      i++;
    }
  }
  // Reset enabled state for heaters, as they take 2 led states
  if (_aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX - 1].state == OFF && _aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX].state == ON)
    _aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX - 1].state = ENABLE;

  if (_aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX - 1].state == OFF && _aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX].state == ON)
    _aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX - 1].state = ENABLE;

  if (_aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX - 1].state == OFF && _aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX].state == ON)
    _aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX - 1].state = ENABLE;
  /*
  for (i=0; i < TOTAL_BUTTONS; i++) {
    logMessage(LOG_NOTICE, "%s = %d", _aqualink_data.aqbuttons[i].name,  _aqualink_data.aqualinkleds[i].state);
  }
*/
}

bool checkAqualinkTime()
{
  static time_t last_checked;
  time_t now = time(0); // get time now
  int time_difference;
  struct tm aq_tm;
  time_t aqualink_time;

  time_difference = (int)difftime(now, last_checked);
  if (time_difference < TIME_CHECK_INTERVAL)
  {
    logMessage(LOG_DEBUG, "time not checked, will check in %d seconds", TIME_CHECK_INTERVAL - time_difference);
    return true;
  }
  else
  {
    last_checked = now;
    //return false;
  }

  char datestr[DATE_STRING_LEN];
  strcpy(&datestr[0], _aqualink_data.date);
  strcpy(&datestr[12], " ");
  strcpy(&datestr[13], _aqualink_data.time);

  if (strptime(datestr, "%m/%d/%y %a %I:%M %p", &aq_tm) == NULL)
  {
    logMessage(LOG_ERR, "Could not convert RS time string '%s'", datestr);
    last_checked = (time_t)NULL;
    return true;
  }

  aq_tm.tm_isdst = -1; // Force mktime to use local timezone
  aqualink_time = mktime(&aq_tm);
  time_difference = (int)difftime(now, aqualink_time);

  logMessage(LOG_INFO, "Aqualink time is off by %d seconds...\n", time_difference);

  if (abs(time_difference) <= ACCEPTABLE_TIME_DIFF)
  {
    // Time difference is less than or equal to 90 seconds (1 1/2 minutes).
    // Set the return value to true.
    return true;
  }

  /*
  char datestring[256];
  //time_t t = time(0);   // get time now
  struct tm * tm = localtime( & now );
  strftime (datestring, sizeof(datestring), "%m/%d/%y %a %I:%M %p %z", tm);
  printf("Computer '%s'\n",datestring);
  strftime (datestring, sizeof(datestring), "%m/%d/%y %a %I:%M %p %z", &aq_tm);
  printf("Aqualink '%s'\n",datestring);
  printf("test '%s'\n",datestring);
  */
  return false;
}

/*
void aqualink_strcpy(char *dest, char *src)
{
  int i;
  
  for (i=0; i<strlen(src); i++) {
    if (src[i] > 125 || src[i] < 32 || src[i] == 34 || src[i] == 42 || src[i] == 96 || src[i] == 39 || src[i] == 92)
      dest[i] = 32;
    else
      dest[i] = src[i];
  }
  
  dest[i] = '\0';
  //dest[10] = '\0';
} 
*/
void queueGetProgramData()
{
  //aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, &_aqualink_data);
  // Init string good time to get setpoints
  //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, &_aqualink_data);
  //aq_programmer(AQ_SEND_CMD, (char *)*NUL, &_aqualink_data);
  aq_send_cmd(NUL);
  aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, &_aqualink_data);
  aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, &_aqualink_data);
  //aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, &_aqualink_data);
  if (_config_parameters.use_panel_aux_labels == true)
  {
    aq_programmer(AQ_GET_AUX_LABELS, NULL, &_aqualink_data);
  }
  //aq_programmer(AQ_GET_PROGRAMS, NULL, &_aqualink_data); // only displays to log at present, also seems to confuse getting set_points
}

void setUnits(char *msg)
{
  logMessage(LOG_DEBUG, "Getting temp units from message %s, looking at %c", msg, msg[strlen(msg) - 1]);

  if (msg[strlen(msg) - 1] == 'F')
    _aqualink_data.temp_units = FAHRENHEIT;
  else if (msg[strlen(msg) - 1] == 'C')
    _aqualink_data.temp_units = CELSIUS;
  else
    _aqualink_data.temp_units = UNKNOWN;

  logMessage(LOG_INFO, "Temp Units set to %d (F=0, C=1, Unknown=3)", _aqualink_data.temp_units);
}

void processMessage(char *message)
{
  char *msg;
  static bool _initWithRS = false;
  static bool _gotREV = false;
  static int freeze_msg_count = 0;
  static int service_msg_count = 0;
  // NSF replace message with msg
  msg = stripwhitespace(message);
  strcpy(_aqualink_data.last_message, msg);
  //_aqualink_data.last_message = _aqualink_data.message;
  //_aqualink_data.display_message = NULL;

  //aqualink_strcpy(_aqualink_data.message, msg);

  logMessage(LOG_INFO, "RS Message :- '%s'\n", msg);
  //logMessage(LOG_NOTICE, "RS Message :- '%s'\n",msg);

  // Check long messages in this if/elseif block first, as some messages are similar.
  // ie "POOL TEMP" and "POOL TEMP IS SET TO"  so want correct match first.
  //

  if (stristr(msg, "JANDY AquaLinkRS") != NULL)
  {
    //_aqualink_data.display_message = NULL;
    _aqualink_data.last_display_message[0] = '\0';
  }
  // If we have more than 10 messages without "Service Mode is active" assume it's off.
  if (_aqualink_data.service_mode_state == ON && service_msg_count++ > 10)
  {
    _aqualink_data.service_mode_state = OFF;
    service_msg_count = 0;
  }
  /*
  // If we have more than 10 messages without "Service Mode is active" assume it's off.
  if (_aqualink_data.service_mode_state == ON && service_msg_count++ > 10) {
    _aqualink_data.service_mode_state = OFF;
    service_msg_count = 0;
  }
*/
  // If we have more than 10 messages without "FREE PROTECT ACTIVATED" assume it's off.
  if (_aqualink_data.frz_protect_state == ON && freeze_msg_count++ > 10)
  {
    _aqualink_data.frz_protect_state = ENABLE;
    freeze_msg_count = 0;
  }

  //else
  //_aqualink_data.display_last_message = false;

  if (stristr(msg, LNG_MSG_BATTERY_LOW) != NULL)
  {
    _aqualink_data.battery = LOW;
    strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI
  }
  else if (stristr(msg, LNG_MSG_POOL_TEMP_SET) != NULL)
  {
    //logMessage(LOG_DEBUG, "pool htr long message: %s", &message[20]);
    _aqualink_data.pool_htr_set_point = atoi(message + 20);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (stristr(msg, LNG_MSG_SPA_TEMP_SET) != NULL)
  {
    //logMessage(LOG_DEBUG, "spa htr long message: %s", &message[19]);
    _aqualink_data.spa_htr_set_point = atoi(message + 19);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_SET) != NULL)
  {
    //logMessage(LOG_DEBUG, "frz protect long message: %s", &message[28]);
    _aqualink_data.frz_protect_set_point = atoi(message + 28);
    _aqualink_data.frz_protect_state = ENABLE;

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (strncasecmp(msg, MSG_AIR_TEMP, MSG_AIR_TEMP_LEN) == 0)
  {
    _aqualink_data.air_temp = atoi(msg + MSG_AIR_TEMP_LEN);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (strncasecmp(msg, MSG_POOL_TEMP, MSG_POOL_TEMP_LEN) == 0)
  {
    _aqualink_data.pool_temp = atoi(msg + MSG_POOL_TEMP_LEN);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (strncasecmp(msg, MSG_SPA_TEMP, MSG_SPA_TEMP_LEN) == 0)
  {
    _aqualink_data.spa_temp = atoi(msg + MSG_SPA_TEMP_LEN);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  // NSF If get water temp rather than pool or spa in some cases, then we are in Pool OR Spa ONLY mode
  else if (strncasecmp(msg, MSG_WATER_TEMP, MSG_WATER_TEMP_LEN) == 0)
  {
    _aqualink_data.pool_temp = atoi(msg + MSG_WATER_TEMP_LEN);
    _aqualink_data.spa_temp = atoi(msg + MSG_WATER_TEMP_LEN);
    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);

    if (_aqualink_data.single_device != true)
    {
      _aqualink_data.single_device = true;
      logMessage(LOG_NOTICE, "AqualinkD set to 'Pool OR Spa Only' mode\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP1_SET) != NULL)
  {
    _aqualink_data.pool_htr_set_point = atoi(message + 28);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);

    if (_aqualink_data.single_device != true)
    {
      _aqualink_data.single_device = true;
      logMessage(LOG_NOTICE, "AqualinkD set to 'Pool OR Spa Only' mode\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP2_SET) != NULL)
  {
    _aqualink_data.spa_htr_set_point = atoi(message + 27);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);

    if (_aqualink_data.single_device != true)
    {
      _aqualink_data.single_device = true;
      logMessage(LOG_NOTICE, "AqualinkD set to 'Pool OR Spa Only' mode\n");
    }
  }
  else if (stristr(msg, LNG_MSG_SERVICE_ACTIVE) != NULL)
  {
    if (_aqualink_data.service_mode_state == OFF)
      logMessage(LOG_NOTICE, "AqualinkD set to Service Mode\n");
    _aqualink_data.service_mode_state = ON;
    service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_ACTIVATED) != NULL)
  {
    _aqualink_data.frz_protect_state = ON;
    freeze_msg_count = 0;
    strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI
  }
  else if (msg[2] == '/' && msg[5] == '/' && msg[8] == ' ')
  { // date in format '08/29/16 MON'
    strcpy(_aqualink_data.date, msg);
  }
  else if (strncasecmp(msg, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0)
  {
    _aqualink_data.swg_percent = atoi(msg + MSG_SWG_PCT_LEN);
    //logMessage(LOG_DEBUG, "Stored SWG Percent as %d\n", _aqualink_data.swg_percent);
  }
  else if (strncasecmp(msg, MSG_SWG_PPM, MSG_SWG_PPM_LEN) == 0)
  {
    _aqualink_data.swg_ppm = atoi(msg + MSG_SWG_PPM_LEN);
    //logMessage(LOG_DEBUG, "Stored SWG PPM as %d\n", _aqualink_data.swg_ppm);
  }
  else if ((msg[1] == ':' || msg[2] == ':') && msg[strlen(msg) - 1] == 'M')
  { // time in format '9:45 AM'
    strcpy(_aqualink_data.time, msg);
    // Setting time takes a long time, so don't try until we have all other programmed data.
    if ((_initWithRS == true) && strlen(_aqualink_data.date) > 1 && checkAqualinkTime() != true)
    {
      logMessage(LOG_NOTICE, "RS time is NOT accurate '%s %s', re-setting on controller!\n", _aqualink_data.time, _aqualink_data.date);
      aq_programmer(AQ_SET_TIME, NULL, &_aqualink_data);
    }
    else
    {
      logMessage(LOG_DEBUG, "RS time is accurate '%s %s'\n", _aqualink_data.time, _aqualink_data.date);
    }
    // If we get a time message before REV, the controller didn't see us as we started too quickly.
    if (_gotREV == false)
    {
      logMessage(LOG_NOTICE, "Getting control panel information\n", msg);
      aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, &_aqualink_data);
      _gotREV = true; // Force it to true just incase we don't understand the model#
    }
  }
  else if (strstr(msg, " REV ") != NULL)
  { // '8157 REV MMM'
    // A master firmware revision message.
    strcpy(_aqualink_data.version, msg);
    _gotREV = true;
    logMessage(LOG_NOTICE, "Control Panel %s\n", msg);
    if (_initWithRS == false)
    {
      queueGetProgramData();
      _initWithRS = true;
    }
  }
  else if (stristr(msg, " TURNS ON") != NULL)
  {
    logMessage(LOG_NOTICE, "Program data '%s'\n", msg);
  }
  else if (_config_parameters.override_freeze_protect == TRUE && strncasecmp(msg, "Press Enter* to override Freeze Protection with", 47) == 0)
  {
    //send_cmd(KEY_ENTER, aq_data);
    //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, &_aqualink_data);
    aq_send_cmd(KEY_ENTER);
  }
  else if ((msg[4] == ':') && (strncasecmp(msg, "AUX", 3) == 0))
  { // AUX label "AUX1:"
    int labelid = atoi(msg + 3);
    if (labelid > 0 && _config_parameters.use_panel_aux_labels == true)
    {
      // Aux1: on panel = Button 3 in aqualinkd  (button 2 in array)
      logMessage(LOG_NOTICE, "AUX LABEL %d '%s'\n", labelid + 1, msg);
      _aqualink_data.aqbuttons[labelid+1].label = prittyString(cleanalloc(msg+5));
      //_aqualink_data.aqbuttons[labelid + 1].label = cleanalloc(msg + 5);
    }
  }
  else
  {
    logMessage(LOG_DEBUG_SERIAL, "Ignoring '%s'\n", msg);
    //_aqualink_data.display_message = msg;
    if (_aqualink_data.active_thread.thread_id == 0 &&
        stristr(msg, "JANDY AquaLinkRS") == NULL &&
        stristr(msg, "PUMP O") == NULL /*&&  // Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        stristr(msg, "CLEANER O") == NULL &&
        stristr(msg, "SPA O") == NULL &&
        stristr(msg, "AUX") == NULL*/
    )
    { // Catch all AUX1 AUX5 messages
      //_aqualink_data.display_last_message = true;
      strcpy(_aqualink_data.last_display_message, msg);
    }
  }

  // Send every message if we are in simulate panel mode
  if (_aqualink_data.simulate_panel)
    ascii(_aqualink_data.last_display_message, msg);

  // We processed the next message, kick any threads waiting on the message.
  kick_aq_program_thread(&_aqualink_data);
}
/* 
bool process_pda_monitor_packet(unsigned char *packet, int length)
{
  bool rtn = false;
  static bool checkedMenu = false;

  process_pda_menu_packet(packet, length);

  //printf("*** PDA Received 0x%02hhx %s ***\n", packet[PKT_CMD], get_packet_type(packet, length));

  if (packet[PKT_CMD] == CMD_STATUS && pda_m_type() == PM_EQUIPTMENT_STATUS && checkedMenu == false) {
    // Can't use any of the build in pda_menu find functions.
    //printf("*** PDA Checking equiptment status ***\n");
    logMessage(LOG_INFO, "Checking PDA equiptment status\n");
    int i;
    char *index;
    int pi = 0;
    for (i = 0; i < PDA_LINES; i++) {
      if (strcasestr(pda_m_line(i), "epump") != NULL || strcasestr(pda_m_line(i), "Intelliflo") != NULL )
      { // Pump labels arew ePUMP & Intelliflo with number at end "Jandy ePUMP   1" "Intelliflo VS 1"
         pi = atoi(pda_m_line(i)+14)-1;
         if (pi < 0 || pi >= MAX_PUMPS-1)
           pi = 0;
      }
      else if ((index = strcasestr(pda_m_line(i), MSG_PMP_RPM)) != NULL)
      { 
        _aqualink_data.pumps[0].rpm = atoi(index + strlen(MSG_PMP_RPM));
        rtn = true;
        logMessage(LOG_DEBUG, "Pump %d RPM = %d\n", pi+1, _aqualink_data.pumps[0].rpm);
      } 
      else if ((index = strcasestr(pda_m_line(i), MSG_PMP_WAT)) != NULL)
      { // Default to pump 0, should check for correct pump
        _aqualink_data.pumps[pi].watts = atoi(index + strlen(MSG_PMP_WAT));
        rtn = true;
        logMessage(LOG_DEBUG, "Pump %d Watts = %d\n", pi+1, _aqualink_data.pumps[pi].watts);
      } 
      else if ((index = strcasestr(pda_m_line(i), MSG_PMP_GPH)) != NULL)
      { // Default to pump 0, should check for correct pump
        _aqualink_data.pumps[pi].gph = atoi(index + strlen(MSG_PMP_GPH));
        rtn = true;
        logMessage(LOG_DEBUG, "Pump %d GPH = %d\n", pi+1, _aqualink_data.pumps[pi].watts);
      }
    }
    checkedMenu = true;    
  } else if (packet[PKT_CMD] != CMD_STATUS) { // a lot of STATUS messages after menu has been created, so ignore them reset on different message
    checkedMenu = false;    
  }

  return rtn;
}
*/
bool process_packet(unsigned char *packet, int length)
{
  bool rtn = false;
  static unsigned char last_packet[AQ_MAXPKTLEN];
  static char message[AQ_MSGLONGLEN + 1];
  static int processing_long_msg = 0;

  // Check packet against last check if different.
  if (memcmp(packet, last_packet, length) == 0)
  {
    logMessage(LOG_DEBUG_SERIAL, "RS Received duplicate, ignoring.\n", length);
    return rtn;
  }
  else
  {
    memcpy(last_packet, packet, length);
    _aqualink_data.last_packet_type = packet[PKT_CMD];
    rtn = true;
  }

  if (_config_parameters.pda_mode == true)
  {
    return process_pda_packet(packet, length);
  }

  if (processing_long_msg > 0 && packet[PKT_CMD] != CMD_MSG_LONG)
  {
    processing_long_msg = 0;
    //logMessage(LOG_ERR, "RS failed to receive complete long message, received '%s'\n",message);
    //logMessage(LOG_DEBUG, "RS didn't finished receiving of MSG_LONG '%s'\n",message);
    processMessage(message);
  }

  switch (packet[PKT_CMD])
  {
  case CMD_ACK:
    logMessage(LOG_DEBUG, "RS Received ACK length %d.\n", length);
    break;
  case CMD_STATUS:
    logMessage(LOG_DEBUG, "RS Received STATUS length %d.\n", length);
    memcpy(_aqualink_data.raw_status, packet + 4, AQ_PSTLEN);
    processLEDstate();
    if (_aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
      //_aqualink_data.spa_temp = _config_parameters.report_zero_spa_temp?-18:TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == OFF && _aqualink_data.single_device != true)
    {
      //_aqualink_data.spa_temp = _config_parameters.report_zero_spa_temp?-18:TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == ON && _aqualink_data.single_device != true)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
    }
    break;
  case CMD_MSG:
    memset(message, 0, AQ_MSGLONGLEN + 1);
    strncpy(message, (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
    //logMessage(LOG_DEBUG_SERIAL, "RS Received message '%s'\n",message);
    if (packet[PKT_DATA] == 1) // Start of long message, get them all before processing
    {
      kick_aq_program_thread(&_aqualink_data);
      break;
    }

    processMessage(message);
    break;
  case CMD_MSG_LONG:
    // First in sequence is normal message.
    processing_long_msg++;
    strncpy(&message[processing_long_msg * AQ_MSGLEN], (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
    //logMessage(LOG_DEBUG_SERIAL, "RS Received long message '%s'\n",message);
    if (processing_long_msg == 3)
    {
      //logMessage(LOG_DEBUG, "RS Finished receiving of MSG_LONG '%s'\n",message);
      processMessage(message);
      processing_long_msg = 0;
    }
    break;
  case CMD_PROBE:
    logMessage(LOG_DEBUG, "RS Received PROBE length %d.\n", length);
    //logMessage(LOG_INFO, "Synch'ing with Aqualink master device...\n");
    rtn = false;
    break;
  default:
    logMessage(LOG_INFO, "RS Received unknown packet, 0x%02hhx\n", packet[PKT_CMD]);
    rtn = false;
    break;
  }

  return rtn;
}

void action_delayed_request()
{
  char sval[10];
  snprintf(sval, 9, "%d", _aqualink_data.unactioned.value);

  // If we don't know the units yet, we can't action, so wait until we do.
  if (_aqualink_data.temp_units == UNKNOWN && _aqualink_data.unactioned.type != SWG_SETPOINT)
    return;

  if (_aqualink_data.unactioned.type == POOL_HTR_SETOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(POOL_HTR_SETOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.pool_htr_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_POOL_HEATER_TEMP, sval, &_aqualink_data);
      logMessage(LOG_NOTICE, "Setting pool heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      logMessage(LOG_NOTICE, "Pool heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SPA_HTR_SETOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SPA_HTR_SETOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.spa_htr_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, sval, &_aqualink_data);
      logMessage(LOG_NOTICE, "Setting spa heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      logMessage(LOG_NOTICE, "Spa heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == FREEZE_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(FREEZE_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.frz_protect_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, sval, &_aqualink_data);
      logMessage(LOG_NOTICE, "Setting freeze protect to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      logMessage(LOG_NOTICE, "Freeze setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SWG_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SWG_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF)
    {
      // SWG is off, can't set %, so delay the set until it's on.
      _aqualink_data.swg_delayed_percent = _aqualink_data.unactioned.value;
    }
    else
    {
      if (_aqualink_data.swg_percent != _aqualink_data.unactioned.value)
      {
        aq_programmer(AQ_SET_SWG_PERCENT, sval, &_aqualink_data);
        logMessage(LOG_NOTICE, "Setting SWG %% to %d\n", _aqualink_data.unactioned.value);
      }
      else
      {
        logMessage(LOG_NOTICE, "SWG % is already %d, not changing\n", _aqualink_data.unactioned.value);
      }
    }
    // Let's just tell everyone we set it, before we actually did.  Makes homekit happy, and it will re-correct on error.
    _aqualink_data.swg_percent = _aqualink_data.unactioned.value;
  }

  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.unactioned.value = -1;
  _aqualink_data.unactioned.requested = 0;
}

int main(int argc, char *argv[])
{
  // main_loop ();

  int i;
  //char *cfgFile = DEFAULT_CONFIG_FILE;
  char defaultCfg[] = "./aqualinkd.conf";
  char *cfgFile;
  int cmdln_loglevel = -1;
  bool cmdln_debugRS485 = false;

  // struct lws_context_creation_info info;
  // Log only NOTICE messages and above. Debug and info messages
  // will not be logged to syslog.
  setlogmask(LOG_UPTO(LOG_NOTICE));

  if (getuid() != 0)
  {
    //logMessage(LOG_ERR, "%s Can only be run as root\n", argv[0]);
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize the daemon's parameters.
  init_parameters(&_config_parameters);
  cfgFile = defaultCfg;
  //sprintf(cfgFile, "%s", DEFAULT_CONFIG_FILE);

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0)
    {
      _config_parameters.deamonize = false;
    }
    else if (strcmp(argv[i], "-c") == 0)
    {
      cfgFile = argv[++i];
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      cmdln_loglevel = LOG_DEBUG;
    }
    else if (strcmp(argv[i], "-rsd") == 0)
    {
      cmdln_debugRS485 = true;
    }
  }

  initButtons(&_aqualink_data);

  readCfg(&_config_parameters, &_aqualink_data, cfgFile);

  if (cmdln_loglevel != -1)
    _config_parameters.log_level = cmdln_loglevel;

  if (cmdln_debugRS485)
    _config_parameters.debug_RSProtocol_packets = true;

  setLoggingPrms(_config_parameters.log_level, _config_parameters.deamonize, _config_parameters.log_file);

  logMessage(LOG_NOTICE, "%s v%s\n", AQUALINKD_NAME, AQUALINKD_VERSION);

  logMessage(LOG_NOTICE, "Config log_level         = %d\n", _config_parameters.log_level);
  logMessage(LOG_NOTICE, "Config socket_port       = %s\n", _config_parameters.socket_port);
  logMessage(LOG_NOTICE, "Config serial_port       = %s\n", _config_parameters.serial_port);
  logMessage(LOG_NOTICE, "Config web_directory     = %s\n", _config_parameters.web_directory);
  logMessage(LOG_NOTICE, "Config device_id         = 0x%02hhx\n", _config_parameters.device_id);
  logMessage(LOG_NOTICE, "Config read_all_devices  = %s\n", bool2text(_config_parameters.read_all_devices));
  logMessage(LOG_NOTICE, "Config use_aux_labels    = %s\n", bool2text(_config_parameters.use_panel_aux_labels));
  logMessage(LOG_NOTICE, "Config override frz prot = %s\n", bool2text(_config_parameters.override_freeze_protect));
#ifndef MG_DISABLE_MQTT
  logMessage(LOG_NOTICE, "Config mqtt_server       = %s\n", _config_parameters.mqtt_server);
  logMessage(LOG_NOTICE, "Config mqtt_dz_sub_topic = %s\n", _config_parameters.mqtt_dz_sub_topic);
  logMessage(LOG_NOTICE, "Config mqtt_dz_pub_topic = %s\n", _config_parameters.mqtt_dz_pub_topic);
  logMessage(LOG_NOTICE, "Config mqtt_aq_topic     = %s\n", _config_parameters.mqtt_aq_topic);
  logMessage(LOG_NOTICE, "Config mqtt_user         = %s\n", _config_parameters.mqtt_user);
  logMessage(LOG_NOTICE, "Config mqtt_passwd       = %s\n", _config_parameters.mqtt_passwd);
  logMessage(LOG_NOTICE, "Config mqtt_ID           = %s\n", _config_parameters.mqtt_ID);
  logMessage(LOG_NOTICE, "Config idx water temp    = %d\n", _config_parameters.dzidx_air_temp);
  logMessage(LOG_NOTICE, "Config idx pool temp     = %d\n", _config_parameters.dzidx_pool_water_temp);
  logMessage(LOG_NOTICE, "Config idx spa temp      = %d\n", _config_parameters.dzidx_spa_water_temp);
  logMessage(LOG_NOTICE, "Config idx SWG Percent   = %d\n", _config_parameters.dzidx_swg_percent);
  logMessage(LOG_NOTICE, "Config idx SWG PPM       = %d\n", _config_parameters.dzidx_swg_ppm);
  logMessage(LOG_NOTICE, "Config PDA Mode          = %s\n", bool2text(_config_parameters.pda_mode));
  logMessage(LOG_NOTICE, "Config PDA Sleep Mode    = %s\n", bool2text(_config_parameters.pda_sleep_mode));
  logMessage(LOG_NOTICE, "Config force SWG         = %s\n", bool2text(_config_parameters.force_swg));
  /* removed until domoticz has a better virtual thermostat
  logMessage(LOG_NOTICE, "Config idx pool thermostat = %d\n", _config_parameters.dzidx_pool_thermostat);
  logMessage(LOG_NOTICE, "Config idx spa thermostat  = %d\n", _config_parameters.dzidx_spa_thermostat);
  */
#endif // MG_DISABLE_MQTT
  logMessage(LOG_NOTICE, "Config deamonize         = %s\n", bool2text(_config_parameters.deamonize));
  logMessage(LOG_NOTICE, "Config log_file          = %s\n", _config_parameters.log_file);
  logMessage(LOG_NOTICE, "Config light_pgm_mode    = %.2f\n", _config_parameters.light_programming_mode);
  logMessage(LOG_NOTICE, "Debug RS485 protocol     = %s\n", bool2text(_config_parameters.debug_RSProtocol_packets));
  //logMessage(LOG_NOTICE, "Use PDA 4 auxiliary info = %s\n", bool2text(_config_parameters.use_PDA_auxiliary));
  logMessage(LOG_NOTICE, "Read Pentair Packets     = %s\n", bool2text(_config_parameters.read_pentair_packets));
  // logMessage (LOG_NOTICE, "Config serial_port = %s\n", config_parameters->serial_port);

  for (i = 0; i < TOTAL_BUTONS; i++)
  {
    logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | PDAlabel %-15s | dzidx %d\n", _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label, _aqualink_data.aqbuttons[i].pda_label, _aqualink_data.aqbuttons[i].dz_idx);
    //logMessage(LOG_NOTICE, "Button %d\n", i+1, _aqualink_data.aqbuttons[i].label , _aqualink_data.aqbuttons[i].dz_idx);
  }

  if (_config_parameters.deamonize == true)
  {
    char pidfile[256];
    // sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    sprintf(pidfile, "%s/%s.pid", "/run", basename(argv[0]));
    daemonise(pidfile, main_loop);
  }
  else
  {
    main_loop();
  }

  exit(EXIT_SUCCESS);
}

/*
void debugPacketPrint(unsigned char ID, unsigned char *packet_buffer, int packet_length)
{
  char buff[1000];
  int i = 0;
  int cnt = 0;

  cnt = sprintf(buff, "%4.4s 0x%02hhx of type %8.8s", (packet_buffer[PKT_DEST] == 0x00 ? "From" : "To"), ID, get_packet_type(packet_buffer, packet_length));
  cnt += sprintf(buff + cnt, " | HEX: ");
  //printHex(packet_buffer, packet_length);
  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  if (packet_buffer[PKT_CMD] == CMD_MSG)
  {
    cnt += sprintf(buff + cnt, "  Message : ");
    //fwrite(packet_buffer + 4, 1, packet_length - 4, stdout);
    strncpy(buff + cnt, (char *)packet_buffer + PKT_DATA + 1, AQ_MSGLEN);
    cnt += AQ_MSGLEN;
  }

  if (packet_buffer[PKT_DEST] == 0x00)
    cnt += sprintf(buff + cnt, "\n\n");
  else
    cnt += sprintf(buff + cnt, "\n");

  //logMessage(LOG_NOTICE, "- AQUA SWG - \n%s", buff);
  if (_config_parameters.debug_RSProtocol_packets)
    writePacketLog(buff);
  else
    logMessage(LOG_NOTICE, "%s", buff);
}

void debugPacket(unsigned char *packet_buffer, int packet_length)
{
  static unsigned char lastID;

  if (packet_buffer[PKT_DEST] == DEV_MASTER && (lastID == 0x50 || lastID == 0x58))
  {
    debugPacketPrint(lastID, packet_buffer, packet_length);
  }
  else if (packet_buffer[PKT_DEST] == 0x50 || packet_buffer[PKT_DEST] == 0x58)
  {
    debugPacketPrint(packet_buffer[PKT_DEST], packet_buffer, packet_length);
  }

  lastID = packet_buffer[PKT_DEST];
}

void logPacket(unsigned char *packet_buffer, int packet_length)
{
  static unsigned char last_packet_buffer[AQ_MAXPKTLEN];
  static int last_packet_length;

  if (packet_buffer[PKT_DEST] != DEV_MASTER)
  {
    memcpy(last_packet_buffer, packet_buffer, packet_length);
    last_packet_length = packet_length;
  }
  else
  {
    debugPacketPrint(last_packet_buffer[PKT_DEST], last_packet_buffer, last_packet_length);
    debugPacketPrint(last_packet_buffer[PKT_DEST], packet_buffer, packet_length);
  }
}
*/

void logPacket_new(unsigned char* packet_buffer, int packet_length)
{
  char buff[1000];
  int i = 0;
  int cnt = 0;

  cnt = sprintf(buff, "%8.8s Packet | HEX: ",getProtocolType(packet_buffer)==JANDY?"Jandy":"Pentair");
  
  for (i=0;i<packet_length;i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  if (_config_parameters.debug_RSProtocol_packets)
    writePacketLog(buff);
  else
    logMessage(LOG_DEBUG_SERIAL, "%s", buff);
}

void logPacket(unsigned char *packet_buffer, int packet_length)
{
  if (_config_parameters.read_pentair_packets) {
    logPacket_new(packet_buffer, packet_length);
    return;
  }

  char buff[1000];
  int i = 0;
  int cnt = 0;

  cnt = sprintf(buff, "To 0x%02hhx of type %8.8s", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
  cnt += sprintf(buff + cnt, " | HEX: ");
  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  if (_config_parameters.debug_RSProtocol_packets)
    writePacketLog(buff);
  else
    logMessage(LOG_DEBUG_SERIAL, "%s", buff);
}



#define MAX_BLOCK_ACK 12
#define MAX_BUSY_ACK  (50 + MAX_BLOCK_ACK)

void caculate_ack_packet(int rs_fd, unsigned char *packet_buffer) {
  static int delayAckCnt = 0;

  if (!_aqualink_data.simulate_panel || _aqualink_data.active_thread.thread_id != 0) {
    // if PDA mode, should we sleep? if not Can only send command to status message on PDA.
    if (_config_parameters.pda_mode == true) {
      if (_config_parameters.pda_sleep_mode && pda_shouldSleep()) {
        logMessage(LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      } else if (packet_buffer[PKT_CMD] != CMD_STATUS)
        send_ack(rs_fd, NUL);
      else
        send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
    }
  } else { // We are in simlator mode, ack get's complicated now.
    // If have a command to send, send a normal ack.
    // If we last message is waiting for an input "SELECT xxxxx", then sent a pause ack
    // pause ack strarts with around 12 ACK_SCREEN_BUSY_DISPLAY acks, then 50  ACK_SCREEN_BUSY acks
    // if we send a command (ie keypress), the whole count needs to end and go back to sending normal ack.
    // In code below, it jumps to sending ACK_SCREEN_BUSY, which still seems to work ok.
    if (strncasecmp(_aqualink_data.last_display_message, "SELECT", 6) != 0) { // Nothing to wait for, send normal ack.
      send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
      delayAckCnt = 0;
    } else if (get_aq_cmd_length() > 0) {
      // Send command and jump directly "busy but can receive message"
      send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
      delayAckCnt = MAX_BUSY_ACK; // need to test jumping to MAX_BUSY_ACK here
    } else {
      logMessage(LOG_NOTICE, "Sending display busy due to Simulator mode \n");
      if (delayAckCnt < MAX_BLOCK_ACK) // block all incomming messages
        send_extended_ack(rs_fd, ACK_SCREEN_BUSY_BLOCK, pop_aq_cmd(&_aqualink_data));
      else if (delayAckCnt < MAX_BUSY_ACK) // say we are pausing
        send_extended_ack(rs_fd, ACK_SCREEN_BUSY, pop_aq_cmd(&_aqualink_data));
      else // We timed out pause, send normal ack (This should also reset the display message on next message received)
        send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));

      delayAckCnt++;
    }
  }
}

void main_loop()
{
  struct mg_mgr mgr;
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  bool interestedInNextAck = false;
  int i;
  //int delayAckCnt = 0;

  // NSF need to find a better place to init this.
  //_aqualink_data.aq_command = 0x00;
  _aqualink_data.simulate_panel = false;
  _aqualink_data.active_thread.thread_id = 0;
  _aqualink_data.air_temp = TEMP_UNKNOWN;
  _aqualink_data.pool_temp = TEMP_UNKNOWN;
  _aqualink_data.spa_temp = TEMP_UNKNOWN;
  _aqualink_data.frz_protect_set_point = TEMP_UNKNOWN;
  _aqualink_data.pool_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.spa_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.swg_percent = TEMP_UNKNOWN;
  _aqualink_data.swg_ppm = TEMP_UNKNOWN;
  _aqualink_data.ar_swg_status = SWG_STATUS_OFF;
  _aqualink_data.swg_delayed_percent = TEMP_UNKNOWN;
  _aqualink_data.temp_units = UNKNOWN;
  _aqualink_data.single_device = false;
  _aqualink_data.service_mode_state = OFF;
  _aqualink_data.frz_protect_state = OFF;
  _aqualink_data.service_mode_state = OFF;
  _aqualink_data.battery = OK;
  _aqualink_data.open_websockets = 0;

  for (i=0; i < MAX_PUMPS; i++) {
    _aqualink_data.pumps[i].rpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].gph = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].watts = TEMP_UNKNOWN;
  }

  if (_config_parameters.force_swg == true)
     _aqualink_data.swg_percent = 0;

  if (!start_net_services(&mgr, &_aqualink_data, &_config_parameters))
  {
    logMessage(LOG_ERR, "Can not start webserver on port %s.\n", _config_parameters.socket_port);
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  int blank_read = 0;
  rs_fd = init_serial_port(_config_parameters.serial_port);
  logMessage(LOG_NOTICE, "Listening to Aqualink RS8 on serial port: %s\n", _config_parameters.serial_port);

  if (_config_parameters.pda_mode == true)
  {
    init_pda(&_aqualink_data);
  }

  while (_keepRunning == true)
  {
    while ((rs_fd < 0 || blank_read >= MAX_ZERO_READ_BEFORE_RECONNECT) && _keepRunning == true)
    {
      if (rs_fd < 0)
      {
        // sleep(1);
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        logMessage(LOG_ERR, "Aqualink daemon attempting to connect to master device...\n");
        broadcast_aqualinkstate_error(mgr.active_connections, CONNECTION_ERROR);
        mg_mgr_poll(&mgr, 1000); // Sevice messages
        mg_mgr_poll(&mgr, 3000); // should donothing for 3 seconds.
        // broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
      }
      else
      {
        logMessage(LOG_ERR, "Aqualink daemon looks like serial error, resetting.\n");
      }
      rs_fd = init_serial_port(_config_parameters.serial_port);
      blank_read = 0;
    }

    if (_config_parameters.read_pentair_packets)
      packet_length = get_packet_new(rs_fd, packet_buffer);
    else
      packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length == -1)
    {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_ERR, "Bad packet length, reconnecting\n");
      blank_read = MAX_ZERO_READ_BEFORE_RECONNECT;
    }
    else if (packet_length == 0)
    {
      //logMessage(LOG_DEBUG_SERIAL, "Nothing read on serial\n");
      blank_read++;
    }
    else if (packet_length > 0)
    {
      blank_read = 0;

      if (_config_parameters.debug_RSProtocol_packets || getLogLevel() >= LOG_DEBUG_SERIAL) 
        logPacket(packet_buffer, packet_length);

      if (packet_length > 0 && packet_buffer[PKT_DEST] == _config_parameters.device_id)
      {

        if (getLogLevel() >= LOG_DEBUG)
          logMessage(LOG_DEBUG, "RS received packet of type %s length %d\n", get_packet_type(packet_buffer, packet_length), packet_length);

        //logMessage(LOG_DEBUG, "RS received packet of type %s length %d\n", get_packet_type(packet_buffer, packet_length), packet_length);
        //debugPacketPrint(0x00, packet_buffer, packet_length);
        //unsigned char ID, unsigned char *packet_buffer, int packet_length)

        // Process the packet. This includes deriving general status, and identifying
        // warnings and errors.  If something changed, notify any listeners
        if (process_packet(packet_buffer, packet_length) != false)
        {
          broadcast_aqualinkstate(mgr.active_connections);
        }

        // If we are not in PDA or Simulator mode, just sent ACK & any CMD, else caculate the ACK.
        if (!_aqualink_data.simulate_panel && !_config_parameters.pda_mode)
          send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
        else
          caculate_ack_packet(rs_fd, packet_buffer);
      }/* 
      else if (_config_parameters.use_PDA_auxiliary && packet_length > 0 && packet_buffer[PKT_DEST] == 0x60 && _aqualink_data.aqbuttons[PUMP_INDEX].led->state != OFF)
      {
        if (process_pda_monitor_packet(packet_buffer, packet_length))
          broadcast_aqualinkstate(mgr.active_connections);

        //send_ack(rs_fd, NUL);
        send_extended_ack(rs_fd, ACK_PDA, NUL);
      }*/
      else if (packet_length > 0 && _config_parameters.read_all_devices == true)
      {

        //logPacket(packet_buffer, packet_length);

        if (packet_buffer[PKT_DEST] == DEV_MASTER && interestedInNextAck == true)
        {
          if (packet_buffer[PKT_CMD] == CMD_PPM)
          {
            _aqualink_data.ar_swg_status = packet_buffer[5];
            if (_aqualink_data.swg_delayed_percent != TEMP_UNKNOWN && _aqualink_data.ar_swg_status == 0x00)
            { // We have a delayed % to set.
              char sval[10];
              snprintf(sval, 9, "%d", _aqualink_data.swg_delayed_percent);
              aq_programmer(AQ_SET_SWG_PERCENT, sval, &_aqualink_data);
              logMessage(LOG_NOTICE, "Setting SWG %% to %d, from delayed message\n", _aqualink_data.swg_delayed_percent);
              _aqualink_data.swg_delayed_percent = TEMP_UNKNOWN;
            }
            _aqualink_data.swg_ppm = packet_buffer[4] * 100;
          }
          interestedInNextAck = false;
        }
        else if (interestedInNextAck == true && packet_buffer[PKT_DEST] != DEV_MASTER && _aqualink_data.ar_swg_status != 0x00)
        {
          _aqualink_data.ar_swg_status = SWG_STATUS_OFF;
          interestedInNextAck = false;
        }
        else if (packet_buffer[PKT_DEST] == SWG_DEV_ID)
        {
          interestedInNextAck = true;
          if (packet_buffer[3] == CMD_PERCENT && _aqualink_data.active_thread.thread_id == 0)
          {
            // Only read SWG Percent if we are not programming, as we might be changing this
            _aqualink_data.swg_percent = (int)packet_buffer[4];
          }
        }
        else
        {
          interestedInNextAck = false;
        }

        if (_config_parameters.read_pentair_packets && getProtocolType(packet_buffer) == PENTAIR) {
          if (processPentairPacket(packet_buffer, packet_length, &_aqualink_data))
             broadcast_aqualinkstate(mgr.active_connections);
        }
        /*  Removed, iAqualink has sleep mode, so no use
        if (packet_buffer[PKT_DEST] == IAQ_DEV_ID && packet_buffer[PKT_CMD] == CMD_IAQ_MSG) {
          if (processiAqualinkMsg(packet_buffer, packet_length, &_aqualink_data) != false)
            broadcast_aqualinkstate(mgr.active_connections);
        }
        */
      //}
      
        //  logMessage(LOG_DEBUG_SERIAL, "Received Packet for ID 0x%02hhx of type %s %s\n",packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length),
        //                               (packet_buffer[PKT_DEST] == _config_parameters.device_id)?" <-- Aqualinkd ID":"");
      }
    }
    mg_mgr_poll(&mgr, 0);

    // Any unactioned commands
    if (_aqualink_data.unactioned.type != NO_ACTION)
    {
      time_t now;
      time(&now);
      if (difftime(now, _aqualink_data.unactioned.requested) > 2)
      {
        logMessage(LOG_DEBUG, "Actioning delayed request\n");
        action_delayed_request();
      }
    }

#ifdef BLOCKING_MODE
#else
    tcdrain(rs_fd); // Make sure buffer has been sent.
    delay(10);
#endif
    //}
  }
  
  if (_config_parameters.debug_RSProtocol_packets) closePacketLog();
  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  mg_mgr_free(&mgr);

  // NSF need to run through config memory and clean up.

  logMessage(LOG_NOTICE, "Exit!\n");
  exit(EXIT_FAILURE);
}
