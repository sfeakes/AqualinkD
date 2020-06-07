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

#define AQUALINKD_C
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
#include "devices_pentair.h"
#include "pda_aq_programmer.h"
#include "packetLogger.h"
#include "devices_jandy.h"
#include "onetouch.h"
#include "onetouch_aq_programmer.h"
#include "version.h"


//#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;
//static struct aqconfig _aqconfig_;
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
  char buff[30];
  struct tm * timeinfo;
  timeinfo = localtime (&now);
  strftime(buff, sizeof(buff), "%y %b %d %H:%M:%S", timeinfo);
  logMessage(LOG_DEBUG, "System time %s\n", buff);
  timeinfo = localtime (&aqualink_time);
  strftime(buff, sizeof(buff), "%y %b %d %H:%M:%s", timeinfo);
  logMessage(LOG_DEBUG, "Aqualink time %s\n", buff);

  
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
/*
void queueGetProgramData()
{
  //aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, &_aqualink_data);
  // Init string good time to get setpoints
  //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, &_aqualink_data);
  //aq_programmer(AQ_SEND_CMD, (char *)*NUL, &_aqualink_data);

#ifndef DEBUG_NO_INIT_TEST_REMOVE
  aq_send_cmd(NUL);
  aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, &_aqualink_data);
  aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, &_aqualink_data);
#endif

  //aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, &_aqualink_data);
  if (_aqconfig_.use_panel_aux_labels == true)
  {
    aq_programmer(AQ_GET_AUX_LABELS, NULL, &_aqualink_data);
  }
  //aq_programmer(AQ_GET_PROGRAMS, NULL, &_aqualink_data); // only displays to log at present, also seems to confuse getting set_points
}
*/
void setUnits(char *msg)
{
  char buf[AQ_MSGLEN*3];

  ascii(buf, msg);
  logMessage(LOG_DEBUG, "Getting temp units from message '%s', looking at '%c'\n", buf, buf[strlen(buf) - 1]);

  if (msg[strlen(msg) - 1] == 'F')
    _aqualink_data.temp_units = FAHRENHEIT;
  else if (msg[strlen(msg) - 1] == 'C')
    _aqualink_data.temp_units = CELSIUS;
  else
    _aqualink_data.temp_units = UNKNOWN;

  logMessage(LOG_INFO, "Temp Units set to %d (F=0, C=1, Unknown=2)\n", _aqualink_data.temp_units);
}



#define MSG_FREEZE   1  // 2^0, bit 0
#define MSG_SERVICE  2  // 2^1, bit 1
#define MSG_SWG      4  // 2^2, bit 2
#define MSG_BOOST    8  // 2^3, bit 3

void processMessage(char *message)
{
  char *msg;
  static bool _initWithRS = false;
  static bool _gotREV = false;
  //static int freeze_msg_count = 0;
  //static int service_msg_count = 0;
  //static int swg_msg_count = 0;
  //static int boost_msg_count = 0;
  static unsigned char msg_loop = '\0';
  // NSF replace message with msg
  msg = stripwhitespace(message);
  
  strcpy(_aqualink_data.last_message, msg);
  //_aqualink_data.last_message = _aqualink_data.message;
  //_aqualink_data.display_message = NULL;

  //aqualink_strcpy(_aqualink_data.message, msg);

  logMessage(LOG_INFO, "RS Message :- '%s'\n", msg);
  //logMessage(LOG_NOTICE, "RS Message :- '%s'\n",msg);

  // Just set this to off, it will re-set since it'll be the only message we get if on
  _aqualink_data.service_mode_state = OFF;

  // Check long messages in this if/elseif block first, as some messages are similar.
  // ie "POOL TEMP" and "POOL TEMP IS SET TO"  so want correct match first.
  //

  if (stristr(msg, "JANDY AquaLinkRS") != NULL) {
    //_aqualink_data.display_message = NULL;
    _aqualink_data.last_display_message[0] = '\0';

    // Anything that wasn't on during the last set of messages, turn off
    if ((msg_loop & MSG_FREEZE) != MSG_FREEZE)
       _aqualink_data.frz_protect_state = OFF;

    //if ((msg_loop & MSG_SERVICE) != MSG_SERVICE)
    //  _aqualink_data.service_mode_state = OFF; // IF we get this message then Service / Timeout is off

    if ((msg_loop & MSG_SWG) != MSG_SWG)
       _aqualink_data.ar_swg_status = SWG_STATUS_OFF;

    if ((msg_loop & MSG_BOOST) != MSG_BOOST) {
      _aqualink_data.boost = false;
      _aqualink_data.boost_msg[0] = '\0';
      if (_aqualink_data.swg_percent >= 101)
        _aqualink_data.swg_percent = 0;
    }

    msg_loop = '\0';
  }

  if (stristr(msg, LNG_MSG_BATTERY_LOW) != NULL)
  {
    _aqualink_data.battery = LOW;
    strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI
  }
  else if (stristr(msg, LNG_MSG_POOL_TEMP_SET) != NULL)
  {
    //logMessage(LOG_DEBUG, "**************** pool htr long message: %s", &message[20]);
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
     msg_loop |= MSG_SERVICE;
    //service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_TIMEOUT_ACTIVE) != NULL)
  {
    if (_aqualink_data.service_mode_state == OFF)
      logMessage(LOG_NOTICE, "AqualinkD set to Timeout Mode\n");
    _aqualink_data.service_mode_state = FLASH;
     msg_loop |= MSG_SERVICE;
    //service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_ACTIVATED) != NULL)
  {
    msg_loop |= MSG_FREEZE;
    _aqualink_data.frz_protect_state = ON;
    //freeze_msg_count = 0;
    strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI
  }
  else if (msg[2] == '/' && msg[5] == '/' && msg[8] == ' ')
  { // date in format '08/29/16 MON'
    strcpy(_aqualink_data.date, msg);
  }
  //else if (strncasecmp(msg, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0)
  else if (strncasecmp(msg, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0 && strncasecmp(msg, "AQUAPURE HRS", 12) != 0)
  {
    _aqualink_data.swg_percent = atoi(msg + MSG_SWG_PCT_LEN);
    if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF) {_aqualink_data.ar_swg_status = SWG_STATUS_ON;}
    //swg_msg_count = 0;
    msg_loop |= MSG_SWG;
    //logMessage(LOG_DEBUG, "*** '%s' ***\n", msg);
    //logMessage(LOG_DEBUG, "SWG set to %d due to message from control panel\n", _aqualink_data.swg_percent);
  }
  else if (strncasecmp(msg, MSG_SWG_PPM, MSG_SWG_PPM_LEN) == 0)
  {
    _aqualink_data.swg_ppm = atoi(msg + MSG_SWG_PPM_LEN);
    if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF) {_aqualink_data.ar_swg_status = SWG_STATUS_ON;}
    msg_loop |= MSG_SWG;
    //swg_msg_count = 0;
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
      //queueGetProgramData(ALLBUTTON, &_aqualink_data);
      queueGetExtendedProgramData(ALLBUTTON, &_aqualink_data, _aqconfig_.use_panel_aux_labels);
      _initWithRS = true;
    }
  }
  else if (stristr(msg, " TURNS ON") != NULL)
  {
    logMessage(LOG_NOTICE, "Program data '%s'\n", msg);
  }
  else if (_aqconfig_.override_freeze_protect == TRUE && strncasecmp(msg, "Press Enter* to override Freeze Protection with", 47) == 0)
  {
    //send_cmd(KEY_ENTER, aq_data);
    //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, &_aqualink_data);
    aq_send_cmd(KEY_ENTER);
  }
  else if ((msg[4] == ':') && (strncasecmp(msg, "AUX", 3) == 0))
  { // AUX label "AUX1:"
    int labelid = atoi(msg + 3);
    if (labelid > 0 && _aqconfig_.use_panel_aux_labels == true)
    {
      // Aux1: on panel = Button 3 in aqualinkd  (button 2 in array)
      logMessage(LOG_NOTICE, "AUX LABEL %d '%s'\n", labelid + 1, msg);
      _aqualink_data.aqbuttons[labelid+1].label = prittyString(cleanalloc(msg+5));
      //_aqualink_data.aqbuttons[labelid + 1].label = cleanalloc(msg + 5);
    }
  }
  // BOOST POOL 23:59 REMAINING
  else if ( (strncasecmp(msg, "BOOST POOL", 10) == 0) && (strcasestr(msg, "REMAINING") != NULL) ) {
    // Ignore messages if in programming mode.  We get one of these turning off for some strange reason.
    if (_aqualink_data.active_thread.thread_id == 0) {
      snprintf(_aqualink_data.boost_msg, 6, &msg[11]);
      _aqualink_data.boost = true;
      msg_loop |= MSG_BOOST;
      msg_loop |= MSG_SWG;
      if (_aqualink_data.ar_swg_status != SWG_STATUS_ON) {_aqualink_data.ar_swg_status = SWG_STATUS_ON;}
      if (_aqualink_data.swg_percent != 101) {_aqualink_data.swg_percent = 101;}
      //boost_msg_count = 0;
    //if (_aqualink_data.active_thread.thread_id == 0)
      strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI if not in programming mode
    }
  }
  else
  {
    logMessage(LOG_DEBUG_SERIAL, "Ignoring '%s'\n", msg);
    //_aqualink_data.display_message = msg;
    if (_aqualink_data.active_thread.thread_id == 0 &&
        stristr(msg, "JANDY AquaLinkRS") == NULL &&
        stristr(msg, "PUMP O") == NULL  &&// Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        stristr(msg, "MAINTAIN") == NULL && // Catch 'MAINTAIN TEMP IS OFF'
        stristr(msg, "0 PSI") == NULL /* // Catch some erronious message on test harness
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
//printf ("Message kicking\n");
  kick_aq_program_thread(&_aqualink_data, ALLBUTTON);
}

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

  if (_aqconfig_.pda_mode == true)
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
    //logMessage(LOG_DEBUG, "RS Received ACK length %d.\n", length);
    break;
  case CMD_STATUS:
    //logMessage(LOG_DEBUG, "RS Received STATUS length %d.\n", length);
    memcpy(_aqualink_data.raw_status, packet + 4, AQ_PSTLEN);
    processLEDstate();
    if (_aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
      //_aqualink_data.spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == OFF && _aqualink_data.single_device != true)
    {
      //_aqualink_data.spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == ON && _aqualink_data.single_device != true)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
    }

    // COLOR MODE programming relies on state changes, so let any threads know
    if (_aqualink_data.active_thread.ptype == AQ_SET_LIGHTPROGRAM_MODE) {
//printf ("Light thread kicking\n");
      kick_aq_program_thread(&_aqualink_data, ALLBUTTON);
    }
    break;
  case CMD_MSG:
  case CMD_MSG_LONG:
     {
       int index = packet[PKT_DATA]; // Will get 0x00 for complete message, 0x01 for start on long message 0x05 last of long message
       //printf("RSM received message at index %d '%.*s'\n",index,AQ_MSGLEN,(char *)packet + PKT_DATA + 1);
       if (index <= 1){
         memset(message, 0, AQ_MSGLONGLEN + 1);
         strncpy(message, (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         processing_long_msg = index;
       } else {
         strncpy(&message[(processing_long_msg * AQ_MSGLEN)], (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         if (++processing_long_msg != index) {
           logMessage(LOG_ERR, "Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
           //printf("RSM Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
         }
         #ifdef  PROCESS_INCOMPLETE_MESSAGES
           kick_aq_program_thread(&_aqualink_data, ALLBUTTON);
         #endif
       }

       if (index == 0 || index == 5) {
         //printf("RSM process message '%s'\n",message);
         processMessage(message); // This will kick thread
       }
       
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

  // If we don't know the units yet, we can't action setpoint, so wait until we do.
  if (_aqualink_data.temp_units == UNKNOWN && 
     (_aqualink_data.unactioned.type == POOL_HTR_SETOINT || _aqualink_data.unactioned.type == SPA_HTR_SETOINT || _aqualink_data.unactioned.type == FREEZE_SETPOINT))
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
  else if (_aqualink_data.unactioned.type == SWG_BOOST)
  {
    //logMessage(LOG_NOTICE, "SWG BOST to %d\n", _aqualink_data.unactioned.value);
    if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF) {
      logMessage(LOG_ERR, "SWG is off, can't Boost pool\n");
    } else if (_aqualink_data.unactioned.value == _aqualink_data.boost ) {
      logMessage(LOG_ERR, "Request to turn Boost %s ignored, Boost is already %s\n",_aqualink_data.unactioned.value?"On":"Off", _aqualink_data.boost?"On":"Off");
    } else {
      aq_programmer(AQ_SET_BOOST, sval, &_aqualink_data);
    }
    // Let's just tell everyone we set it, before we actually did.  Makes homekit happy, and it will re-correct on error.
    _aqualink_data.boost = _aqualink_data.unactioned.value;
  }
  else if (_aqualink_data.unactioned.type == PUMP_RPM)
  {
    snprintf(sval, 9, "%1d|%d", _aqualink_data.unactioned.id, _aqualink_data.unactioned.value);
    //printf("**** program string '%s'\n",sval);
    aq_programmer(AQ_SET_ONETOUCH_PUMP_RPM, sval, &_aqualink_data);
  }

  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.unactioned.value = -1;
  _aqualink_data.unactioned.id = -1;
  _aqualink_data.unactioned.requested = 0;
}

void printHelp()
{
  printf("%s %s\n",AQUALINKD_NAME,AQUALINKD_VERSION);
  printf("\t-h         (this message)\n");
  printf("\t-d         (do not deamonize)\n");
  printf("\t-c <file>  (Configuration file)\n");
  printf("\t-v         (Debug logging)\n");
  printf("\t-vv        (Serial Debug logging)\n");
  printf("\t-rsd       (RS485 debug)\n");
  printf("\t-rsrd      (RS485 raw debug)\n");
}


int main(int argc, char *argv[])
{
  int i, j;
  //char *cfgFile = DEFAULT_CONFIG_FILE;
  char defaultCfg[] = "./aqualinkd.conf";
  char *cfgFile;
  int cmdln_loglevel = -1;
  bool cmdln_debugRS485 = false;
  bool cmdln_lograwRS485 = false;
  _aqualink_data.num_pumps = 0;
  _aqualink_data.num_lights = 0;

/*
  static unsigned char msg_loop; // = '\0';
  //msg_loop &= ~MSG_SERVICE;
  if ((msg_loop & MSG_SERVICE) != MSG_SERVICE)
    printf("Off\n");
  else
    printf("On\n");

  //msg_loop &= ~MSG_SERVICE;
  msg_loop |= MSG_SERVICE;
  if ((msg_loop & MSG_SERVICE) != MSG_SERVICE)
    printf("Off\n");
  else
    printf("On\n");

  msg_loop |= MSG_SERVICE;
  if ((msg_loop & MSG_SERVICE) != MSG_SERVICE)
    printf("Off\n");
  else
    printf("On\n");

  msg_loop |= MSG_SERVICE;
  if ((msg_loop & MSG_SERVICE) != MSG_SERVICE)
    printf("Off\n");
  else
    printf("On\n");


  return 0;
*/

  if (argc > 1 && strcmp(argv[1], "-h") == 0)
  {
    printHelp();
    return 0;
  }

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
  //init_parameters(&_aqconfig_);
  init_config();
  cfgFile = defaultCfg;
  //sprintf(cfgFile, "%s", DEFAULT_CONFIG_FILE);

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-h") == 0)
    {
      printHelp();
      return 0;
    }
    if (strcmp(argv[i], "-d") == 0)
    {
      _aqconfig_.deamonize = false;
    }
    else if (strcmp(argv[i], "-c") == 0)
    {
      cfgFile = argv[++i];
    }
    else if (strcmp(argv[i], "-vv") == 0)
    {
      cmdln_loglevel = LOG_DEBUG_SERIAL;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      cmdln_loglevel = LOG_DEBUG;
    }
    else if (strcmp(argv[i], "-rsd") == 0)
    {
      cmdln_debugRS485 = true;
    }
    else if (strcmp(argv[i], "-rsrd") == 0)
    {
      cmdln_lograwRS485 = true;
    }
  }

  initButtons(&_aqualink_data);
  
  read_config(&_aqualink_data, cfgFile);

  if (cmdln_loglevel != -1)
    _aqconfig_.log_level = cmdln_loglevel;

  if (cmdln_debugRS485)
    _aqconfig_.debug_RSProtocol_packets = true;

  if (cmdln_lograwRS485)
    _aqconfig_.log_raw_RS_bytes = true;
      

  if (_aqconfig_.display_warnings_web == true)
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, _aqualink_data.last_display_message);
  else
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, NULL);

  logMessage(LOG_NOTICE, "%s v%s\n", AQUALINKD_NAME, AQUALINKD_VERSION);

  logMessage(LOG_NOTICE, "Config log_level         = %d\n", _aqconfig_.log_level);
  logMessage(LOG_NOTICE, "Config socket_port       = %s\n", _aqconfig_.socket_port);
  logMessage(LOG_NOTICE, "Config serial_port       = %s\n", _aqconfig_.serial_port);
  logMessage(LOG_NOTICE, "Config web_directory     = %s\n", _aqconfig_.web_directory);
  logMessage(LOG_NOTICE, "Config device_id         = 0x%02hhx\n", _aqconfig_.device_id);
  logMessage(LOG_NOTICE, "Config extra_device_id   = 0x%02hhx\n", _aqconfig_.onetouch_device_id);
  logMessage(LOG_NOTICE, "Config extra_device_prog = %s\n", bool2text(_aqconfig_.extended_device_id_programming));
  logMessage(LOG_NOTICE, "Config read_all_devices  = %s\n", bool2text(_aqconfig_.read_all_devices));
  logMessage(LOG_NOTICE, "Config use_aux_labels    = %s\n", bool2text(_aqconfig_.use_panel_aux_labels));
  logMessage(LOG_NOTICE, "Config override frz prot = %s\n", bool2text(_aqconfig_.override_freeze_protect));
#ifndef MG_DISABLE_MQTT
  logMessage(LOG_NOTICE, "Config mqtt_server       = %s\n", _aqconfig_.mqtt_server);
  logMessage(LOG_NOTICE, "Config mqtt_dz_sub_topic = %s\n", _aqconfig_.mqtt_dz_sub_topic);
  logMessage(LOG_NOTICE, "Config mqtt_dz_pub_topic = %s\n", _aqconfig_.mqtt_dz_pub_topic);
  logMessage(LOG_NOTICE, "Config mqtt_aq_topic     = %s\n", _aqconfig_.mqtt_aq_topic);
  logMessage(LOG_NOTICE, "Config mqtt_user         = %s\n", _aqconfig_.mqtt_user);
  logMessage(LOG_NOTICE, "Config mqtt_passwd       = %s\n", _aqconfig_.mqtt_passwd);
  logMessage(LOG_NOTICE, "Config mqtt_ID           = %s\n", _aqconfig_.mqtt_ID);
  logMessage(LOG_NOTICE, "Config idx water temp    = %d\n", _aqconfig_.dzidx_air_temp);
  logMessage(LOG_NOTICE, "Config idx pool temp     = %d\n", _aqconfig_.dzidx_pool_water_temp);
  logMessage(LOG_NOTICE, "Config idx spa temp      = %d\n", _aqconfig_.dzidx_spa_water_temp);
  logMessage(LOG_NOTICE, "Config idx SWG Percent   = %d\n", _aqconfig_.dzidx_swg_percent);
  logMessage(LOG_NOTICE, "Config idx SWG PPM       = %d\n", _aqconfig_.dzidx_swg_ppm);
  logMessage(LOG_NOTICE, "Config PDA Mode          = %s\n", bool2text(_aqconfig_.pda_mode));
  logMessage(LOG_NOTICE, "Config PDA Sleep Mode    = %s\n", bool2text(_aqconfig_.pda_sleep_mode));
  logMessage(LOG_NOTICE, "Config force SWG         = %s\n", bool2text(_aqconfig_.force_swg));
  /* removed until domoticz has a better virtual thermostat
  logMessage(LOG_NOTICE, "Config idx pool thermostat = %d\n", _aqconfig_.dzidx_pool_thermostat);
  logMessage(LOG_NOTICE, "Config idx spa thermostat  = %d\n", _aqconfig_.dzidx_spa_thermostat);
  */
#endif // MG_DISABLE_MQTT
  logMessage(LOG_NOTICE, "Config deamonize         = %s\n", bool2text(_aqconfig_.deamonize));
  logMessage(LOG_NOTICE, "Config log_file          = %s\n", _aqconfig_.log_file);
  logMessage(LOG_NOTICE, "Config light_pgm_mode    = %.2f\n", _aqconfig_.light_programming_mode);
  logMessage(LOG_NOTICE, "Debug RS485 protocol     = %s\n", bool2text(_aqconfig_.debug_RSProtocol_packets));
  //logMessage(LOG_NOTICE, "Use PDA 4 auxiliary info = %s\n", bool2text(_aqconfig_.use_PDA_auxiliary));
  logMessage(LOG_NOTICE, "Read Pentair Packets     = %s\n", bool2text(_aqconfig_.read_pentair_packets));
  // logMessage (LOG_NOTICE, "Config serial_port = %s\n", config_parameters->serial_port);
  logMessage(LOG_NOTICE, "Display warnings in web  = %s\n", bool2text(_aqconfig_.display_warnings_web));
  
  if (_aqconfig_.swg_zero_ignore > 0)
    logMessage(LOG_NOTICE, "Ignore SWG 0 msg count   = %d\n", _aqconfig_.swg_zero_ignore);


  for (i = 0; i < TOTAL_BUTONS; i++)
  {
    //char ext[] = " VSP ID None | AL ID 0 ";
    char ext[40];
    ext[0] = '\0';
    for (j = 0; j < _aqualink_data.num_pumps; j++) {
      if (_aqualink_data.pumps[j].button == &_aqualink_data.aqbuttons[i]) {
        sprintf(ext, "VSP ID 0x%02hhx | PMP ID %-1d |",_aqualink_data.pumps[j].pumpID, _aqualink_data.pumps[j].pumpIndex);
      }
    }
    for (j = 0; j < _aqualink_data.num_lights; j++) {
      if (_aqualink_data.lights[j].button == &_aqualink_data.aqbuttons[i]) {
        sprintf(ext,"Light Progm | CTYPE %-1d  |",_aqualink_data.lights[j].lightType);
      }
    }
    if (_aqualink_data.aqbuttons[i].dz_idx > 0)
      sprintf(ext+strlen(ext), "dzidx %-3d", _aqualink_data.aqbuttons[i].dz_idx);

    if (!_aqconfig_.pda_mode) {
      logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | %s\n", 
                           _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label, ext);
    } else {
      logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | PDAlabel %-15s | %s\n", 
                           _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label,
                           _aqualink_data.aqbuttons[i].pda_label, ext);
    }
  }

  if (_aqconfig_.deamonize == true)
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


#define MAX_BLOCK_ACK 12
#define MAX_BUSY_ACK  (50 + MAX_BLOCK_ACK)


void caculate_ack_packet(int rs_fd, unsigned char *packet_buffer) {
  static int delayAckCnt = 0;

  if (packet_buffer[PKT_DEST] == _aqconfig_.onetouch_device_id) {
    send_extended_ack(rs_fd, ACK_ONETOUCH, pop_ot_cmd(packet_buffer[PKT_CMD]));
    return;
  } 

  //if (!_aqualink_data.simulate_panel || _aqualink_data.active_thread.thread_id != 0) {
    // if PDA mode, should we sleep? if not Can only send command to status message on PDA.
  if (_aqconfig_.pda_mode == true) {
      //pda_programming_thread_check(&_aqualink_data);
      if (_aqconfig_.pda_sleep_mode && pda_shouldSleep()) {
        logMessage(LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      //} else if (packet_buffer[PKT_CMD] != CMD_STATUS)  // Moved logic to pop_aq_cmd()
      //  send_extended_ack(rs_fd, ACK_PDA, NUL);
      } else {
        send_extended_ack(rs_fd, ACK_PDA, pop_aq_cmd(&_aqualink_data));
      }
    
  } else if (_aqualink_data.simulate_panel && _aqualink_data.active_thread.thread_id == 0) { 
    // We are in simlator mode, ack get's complicated now.
    // If have a command to send, send a normal ack.
    // If we last message is waiting for an input "SELECT xxxxx", then sent a pause ack
    // pause ack starts with around 12 ACK_SCREEN_BUSY_DISPLAY acks, then 50  ACK_SCREEN_BUSY acks
    // if we send a command (ie keypress), the whole count needs to end and go back to sending normal ack.
    // In code below, it jumps to sending ACK_SCREEN_BUSY, which still seems to work ok.
    if (_aqualink_data.last_packet_type == CMD_MSG_LONG) {
      send_extended_ack(rs_fd, ACK_SCREEN_BUSY, pop_aq_cmd(&_aqualink_data));
    } if (strncasecmp(_aqualink_data.last_display_message, "SELECT", 6) != 0) { // Nothing to wait for, send normal ack.
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
  } else {
    // We are in simulate panel mode, but a thread is active, so ignore simulate panel
    send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
  }
}

unsigned char find_unused_address(unsigned char* packet) {
  static int ID[4] = {0,0,0,0};  // 0=0x08, 1=0x09, 2=0x0A, 3=0x0B
  static unsigned char lastID = 0x00;

  if (packet[PKT_DEST] >= 0x08 && packet[PKT_DEST] <= 0x0B && packet[PKT_CMD] == CMD_PROBE) {
    //printf("Probe packet to keypad ID 0x%02hhx\n",packet[PKT_DEST]);
    lastID = packet[PKT_DEST];
  } else if (packet[PKT_DEST] == DEV_MASTER && lastID != 0x00) {
    lastID = 0x00;
  } else if (lastID != 0x00) {
    ID[lastID-8]++;
    if (ID[lastID-8] >= 3) {
      logMessage(LOG_NOTICE, "Found valid unused ID 0x%02hhx\n",lastID);
      return lastID;
    }
    lastID = 0x00;
  } else {
    lastID = 0x00;
  }

  return 0x00;
}

void main_loop()
{
  struct mg_mgr mgr;
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN+1];
  //bool interestedInNextAck = false;
  rsDeviceType interestedInNextAck = DRS_NONE;
  bool changed = false;
  //int swg_zero_cnt = 0;
  int swg_noreply_cnt = 0;
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
  _aqualink_data.battery = OK;
  _aqualink_data.open_websockets = 0;
  _aqualink_data.ph = TEMP_UNKNOWN;
  _aqualink_data.orp = TEMP_UNKNOWN;

  pthread_mutex_init(&_aqualink_data.active_thread.thread_mutex, NULL);
  pthread_cond_init(&_aqualink_data.active_thread.thread_cond, NULL);

  for (i=0; i < MAX_PUMPS; i++) {
    _aqualink_data.pumps[i].rpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].gpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].watts = TEMP_UNKNOWN;
  }

  if (_aqconfig_.force_swg == true) {
     _aqualink_data.swg_percent = 0;
     _aqualink_data.swg_ppm = 0;
  }

  if (!start_net_services(&mgr, &_aqualink_data))
  {
    logMessage(LOG_ERR, "Can not start webserver on port %s.\n", _aqconfig_.socket_port);
    exit(EXIT_FAILURE);
  }

  startPacketLogger(_aqconfig_.debug_RSProtocol_packets, _aqconfig_.read_pentair_packets);

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  int blank_read = 0;
  rs_fd = init_serial_port(_aqconfig_.serial_port);
  logMessage(LOG_NOTICE, "Listening to Aqualink RS8 on serial port: %s\n", _aqconfig_.serial_port);

  if (_aqconfig_.pda_mode == true)
  {
    #ifdef BETA_PDA_AUTOLABEL
      init_pda(&_aqualink_data, &_aqconfig_);
    #else
      init_pda(&_aqualink_data);
    #endif
  }
  if (_aqconfig_.onetouch_device_id != 0x00)
  {
    set_onetouch_enabled(true);
  }
  if (_aqconfig_.extended_device_id_programming == true)
  {
    set_extended_device_id_programming(true);
  }

  if (_aqconfig_.device_id == 0x00) {
    logMessage(LOG_NOTICE, "Searching for valid ID, please configure one for faster startup\n");
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
        close_serial_port(rs_fd);
      }
      rs_fd = init_serial_port(_aqconfig_.serial_port);
      blank_read = 0;
    }

    if (_aqconfig_.log_raw_RS_bytes)
      packet_length = get_packet_lograw(rs_fd, packet_buffer);
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
    else if (_aqconfig_.device_id == 0x00) {
      blank_read = 0;
      _aqconfig_.device_id = find_unused_address(packet_buffer);
      continue;
    }
    else if (packet_length > 0)
    {
      /* // Use this to check wait time in mg_mgr_poll(&mgr, xx); at bottom of for, and adjust as needed. 
      if (blank_read > 0) {
        logMessage(LOG_NOTICE, "RS empry reads %d\n", blank_read);
      }
      */
      blank_read = 0;
      changed = false;

      if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.device_id && getProtocolType(packet_buffer) == JANDY)
      {
        if (getLogLevel() >= LOG_DEBUG)
          logMessage(LOG_DEBUG, "RS received packet of type %s length %d\n", get_packet_type(packet_buffer, packet_length), packet_length);
        
        changed = process_packet(packet_buffer, packet_length);
        // If we are not in PDA or Simulator mode, just sent ACK & any CMD, else caculate the ACK.
        if (!_aqualink_data.simulate_panel && !_aqconfig_.pda_mode) {
          //send_ack(rs_fd, pop_aq_cmd(&_aqualink_data));
          send_extended_ack(rs_fd, (_aqualink_data.last_packet_type==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_aq_cmd(&_aqualink_data));
        } else
          caculate_ack_packet(rs_fd, packet_buffer);

      }
      else if (packet_length > 0 && onetouch_enabled() && packet_buffer[PKT_DEST] == _aqconfig_.onetouch_device_id && getProtocolType(packet_buffer) == JANDY) {
        //if (getLogLevel() >= LOG_DEBUG)
        //  logMessage(LOG_DEBUG, "RS received ONETOUCH packet of type %s length %d\n", get_packet_type(packet_buffer, packet_length), packet_length);
        changed = process_onetouch_packet(packet_buffer, packet_length, &_aqualink_data);
        caculate_ack_packet(rs_fd, packet_buffer);
      }
      else if (packet_length > 0 && _aqconfig_.read_all_devices == true)
      {
        //logPacket(packet_buffer, packet_length);
        if (packet_buffer[PKT_DEST] == DEV_MASTER && interestedInNextAck != DRS_NONE)
        {
          if (interestedInNextAck == DRS_SWG) {
            swg_noreply_cnt = 0;
            changed = processPacketFromSWG(packet_buffer, packet_length, &_aqualink_data);
          } else if (interestedInNextAck == DRS_EPUMP) {
            changed = processPacketFromJandyPump(packet_buffer, packet_length, &_aqualink_data);
          }
          interestedInNextAck = DRS_NONE;
        }
        else if ( packet_buffer[PKT_DEST] != DEV_MASTER && interestedInNextAck != DRS_NONE )
        { // We were expecting an ack from device as next message but didn;t get it, device must be off
          if (interestedInNextAck == DRS_SWG && _aqualink_data.ar_swg_status != SWG_STATUS_OFF) {
            if ( ++swg_noreply_cnt < 3 ) {
              _aqualink_data.ar_swg_status = SWG_STATUS_OFF;
              changed = true;
            }
          }
          interestedInNextAck = DRS_NONE;
        }
        else if (packet_buffer[PKT_DEST] == SWG_DEV_ID)
        {
          interestedInNextAck = DRS_SWG;
          changed = processPacketToSWG(packet_buffer, packet_length, &_aqualink_data, _aqconfig_.swg_zero_ignore);
        }
        else if (packet_buffer[PKT_DEST] >= JANDY_DEC_PUMP_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_PUMP_MAX)
        {
          interestedInNextAck = DRS_EPUMP;
          changed = processPacketToJandyPump(packet_buffer, packet_length, &_aqualink_data);
        }
        else
        {
          interestedInNextAck = DRS_NONE;
        }

        if (_aqconfig_.read_pentair_packets && getProtocolType(packet_buffer) == PENTAIR) {
          if (processPentairPacket(packet_buffer, packet_length, &_aqualink_data)) {
             //broadcast_aqualinkstate(mgr.active_connections);
             changed = true;
          }
        }  
      }

      if (changed)
        broadcast_aqualinkstate(mgr.active_connections);
    }

    //mg_mgr_poll(&mgr, 10);
    mg_mgr_poll(&mgr, 5);
    tcdrain(rs_fd); // Make sure buffer has been sent.

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

    
    //tcdrain(rs_fd); // Make sure buffer has been sent.
    //delay(10);
  }
  
  //if (_aqconfig_.debug_RSProtocol_packets) stopPacketLogger();
  stopPacketLogger();
  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  mg_mgr_free(&mgr);

  // NSF need to run through config memory and clean up.

  logMessage(LOG_NOTICE, "Exit!\n");
  exit(EXIT_FAILURE);
}
