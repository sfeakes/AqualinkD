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
#include "aq_panel.h"
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
#include "iaqtouch.h"
#include "iaqtouch_aq_programmer.h"
#include "version.h"
#include "rs_msg_utils.h"
#include "serialadapter.h"
#include "debug_timer.h"

/*
#if defined AQ_DEBUG || defined AQ_TM_DEBUG
  #include "timespec_subtract.h"
#endif
*/
//#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;
//static struct aqconfig _aqconfig_;
static struct aqualinkdata _aqualink_data;


#ifdef AQ_TM_DEBUG
  //struct timespec _rs_packet_readitme;
  int _rs_packet_timer;
#endif


void main_loop();

void intHandler(int dummy)
{
  _keepRunning = false;
  LOG(AQUA_LOG,LOG_NOTICE, "Stopping!\n");
  //if (dummy){}// stop compile warnings

  // In blocking mode, die as cleanly as possible.
  if (_aqconfig_.rs_poll_speed < 0) {
    stopPacketLogger();
    // This should force port to close and do somewhat gracefull exit.
    close_blocking_serial_port();
    //exit(-1);
  }
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

      //LOG(AQUA_LOG,LOG_DEBUG,"Led %d state %d",i+1,_aqualink_data.aqualinkleds[i].state);
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
    LOG(AQUA_LOG,LOG_NOTICE, "%s = %d", _aqualink_data.aqbuttons[i].name,  _aqualink_data.aqualinkleds[i].state);
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

  if (_aqconfig_.sync_panel_time != true)
    return true; 

  time_difference = (int)difftime(now, last_checked);
  if (time_difference < TIME_CHECK_INTERVAL)
  {
    LOG(AQUA_LOG,LOG_DEBUG, "time not checked, will check in %d seconds\n", TIME_CHECK_INTERVAL - time_difference);
    return true;
  }
  else
  {
    last_checked = now;
    //return false;
  }

  char datestr[DATE_STRING_LEN];
#ifdef AQ_PDA
  if (isPDA_PANEL) {
    LOG(AQUA_LOG,LOG_DEBUG, "PDA Time Check\n");
    // date is simply a day or week for PDA.
    localtime_r(&now, &aq_tm);
    int real_wday = aq_tm.tm_wday; // NSF Need to do this better, we could be off by 7 days
    snprintf(datestr, DATE_STRING_LEN, "%s %s",_aqualink_data.date,_aqualink_data.time);

    if (strptime(datestr, "%A %I:%M%p", &aq_tm) == NULL) {
      LOG(AQUA_LOG,LOG_ERR, "Could not convert PDA RS time string '%s'", datestr);
      last_checked = (time_t)NULL;
      return true;
    }
    if (real_wday != aq_tm.tm_wday) {
      LOG(AQUA_LOG,LOG_INFO, "PDA Day of the week incorrect - request time set\n");
      return false;
    }
  } 
  else
#endif // AQ_PDA
  {
    
    strcpy(&datestr[0], _aqualink_data.date);
    datestr[8] = ' ';
    strcpy(&datestr[9], _aqualink_data.time);
    //datestr[16] = ' ';
    if (strlen(_aqualink_data.time) <= 7) {
      datestr[13] = ' ';
      datestr[16] ='\0';
    } else {
      datestr[14] = ' ';
      datestr[17] ='\0';
    }
    if (strptime(datestr, "%m/%d/%y %I:%M %p", &aq_tm) == NULL)
    
    //sprintf(datestr, "%s %s", _aqualink_data.date, _aqualink_data.time);
    //if (strptime(datestr, "%m/%d/%y %a %I:%M %p", &aq_tm) == NULL)
    {
      LOG(AQUA_LOG,LOG_ERR, "Could not convert RS time string '%s'\n", datestr);
      last_checked = (time_t)NULL;
      return true;
    }
  }

  aq_tm.tm_isdst = localtime(&now)->tm_isdst; // ( Might need to use -1) set daylight savings to same as system time
  aq_tm.tm_sec = 0; // Set seconds to time.  Really messes up when we don't do this.

  char buff[20];
  
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time from : %s\n", datestr);
  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", &aq_tm);
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time      : %s\n", buff);

  aqualink_time = mktime(&aq_tm);

  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&aqualink_time));
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd converted time    : %s\n", buff);
  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
  LOG(AQUA_LOG,LOG_DEBUG, "System time                 : %s\n", buff);


  time_difference = (int)difftime(now, aqualink_time);

  strftime(buff, 20, "%m/%d/%y %I:%M %p", localtime(&now));
  LOG(AQUA_LOG,LOG_INFO, "Aqualink time '%s' is off system time '%s' by %d seconds...\n", datestr, buff, time_difference);

  if (abs(time_difference) < ACCEPTABLE_TIME_DIFF)
  {
    // Time difference is less than or equal to ACCEPTABLE_TIME_DIFF seconds (1 1/2 minutes).
    // Set the return value to true.
    return true;
  }

  return false;
}


void setUnits(char *msg)
{
  char buf[AQ_MSGLEN*3];

  rsm_strncpy(buf, (unsigned char *)msg, AQ_MSGLEN*3, AQ_MSGLONGLEN);

  //ascii(buf, msg);
  LOG(AQUA_LOG,LOG_DEBUG, "Getting temp units from message '%s', looking at '%c'\n", buf, buf[strlen(buf) - 1]);

  if (msg[strlen(msg) - 1] == 'F')
    _aqualink_data.temp_units = FAHRENHEIT;
  else if (msg[strlen(msg) - 1] == 'C')
    _aqualink_data.temp_units = CELSIUS;
  else
    _aqualink_data.temp_units = UNKNOWN;

  LOG(AQUA_LOG,LOG_INFO, "Temp Units set to %d (F=0, C=1, Unknown=2)\n", _aqualink_data.temp_units);
}

// Defined as int16_t so 16 bits to mask
#define MSG_FREEZE      (1 << 0) // 1
#define MSG_SERVICE     (1 << 1) // 1
#define MSG_SWG         (1 << 2)
#define MSG_BOOST       (1 << 3)
#define MSG_TIMEOUT     (1 << 4)
#define MSG_RS13BUTTON  (1 << 5)
#define MSG_RS14BUTTON  (1 << 6)
#define MSG_RS15BUTTON  (1 << 7)
#define MSG_RS16BUTTON  (1 << 8)
#define MSG_BATTERY_LOW (1 << 9)
#define MSG_SWG_DEVICE  (1 << 10)
/*
#define SET_FLAG(n, f) ((n) |= (f))
#define CHK_FLAG(n, f) ((n) & (f))
*/
#ifdef AQ_RS16
int16_t  RS16_endswithLEDstate(char *msg)
{
  char *sp;
  int i;
  aqledstate state = LED_S_UNKNOWN;

  //if (_aqconfig_.rs_panel_size < 16)
  if (PANEL_SIZE() < 16)
    return false;

  sp = strrchr(msg, ' ');

  if( sp == NULL )
    return false;
  
  if (strncasecmp(sp, " on", 3) == 0)
    state = ON;
  else if (strncasecmp(sp, " off", 4) == 0)
    state = OFF;
  else if (strncasecmp(sp, " enabled", 8) == 0) // Total guess, need to check
    state = ENABLE;
  else if (strncasecmp(sp, " no idea", 8) == 0) // need to figure out these states
    state = FLASH;

  if (state == LED_S_UNKNOWN)
    return false;

  // Only need to start at Aux B5->B8 (12-15)
  // Loop over only aqdata->aqbuttons[13] to aqdata->aqbuttons[16]
  for (i = _aqualink_data.rs16_vbutton_start; i <= _aqualink_data.rs16_vbutton_end; i++) {
    //TOTAL_BUTTONS
    if ( stristr(msg, _aqualink_data.aqbuttons[i].label) != NULL) {
      _aqualink_data.aqbuttons[i].led->state = state;
      LOG(AQUA_LOG,LOG_INFO, "Set %s to %d\n", _aqualink_data.aqbuttons[i].label, _aqualink_data.aqbuttons[i].led->state);
      // Return true should be the result, but in the if we want to continue to display message
      //return true;
      if (i == 13)
        return MSG_RS13BUTTON;
      else if (i == 14)
        return MSG_RS14BUTTON;
      else if (i == 15)
        return MSG_RS15BUTTON;
      else if (i == 16)
        return MSG_RS16BUTTON;
      else
      {
        LOG(AQUA_LOG,LOG_ERR, "RS16 Button Set error %s to %d, %d is out of scope\n", _aqualink_data.aqbuttons[i].label, _aqualink_data.aqbuttons[i].led->state, i);
        return false;
      }
      
    }
  }

  return false;
}
#endif


void _processMessage(char *message, bool reset);

void processMessage(char *message)
{
  _processMessage(message, false);
}
void processMessageReset()
{
  _processMessage(NULL, true);
}
void _processMessage(char *message, bool reset)
{
  char *msg;
  static bool _initWithRS = false;
  //static bool _gotREV = false;
  //static int freeze_msg_count = 0;
  //static int service_msg_count = 0;
  //static int swg_msg_count = 0;
  //static int boost_msg_count = 0;
  static int16_t msg_loop = 0;
  // NSF replace message with msg
#ifdef AQ_RS16
  int16_t rs16;
#endif

  //msg = stripwhitespace(message);
  //strcpy(_aqualink_data.last_message, msg);
  //LOG(AQRS_LOG,LOG_INFO, "RS Message :- '%s'\n", msg);

  

  // Check long messages in this if/elseif block first, as some messages are similar.
  // ie "POOL TEMP" and "POOL TEMP IS SET TO"  so want correct match first.
  //

  //if (stristr(msg, "JANDY AquaLinkRS") != NULL) {
  if (!reset) {
    msg = stripwhitespace(message);
    strcpy(_aqualink_data.last_message, msg);
    LOG(AQRS_LOG,LOG_INFO, "RS Message :- '%s'\n", msg);
    // Just set this to off, it will re-set since it'll be the only message we get if on
    _aqualink_data.service_mode_state = OFF;
  } else {
    //_aqualink_data.display_message = NULL;
    _aqualink_data.last_display_message[0] = ' ';
    _aqualink_data.last_display_message[1] = '\0';

    // Anything that wasn't on during the last set of messages, turn off
    if ((msg_loop & MSG_FREEZE) != MSG_FREEZE)
       _aqualink_data.frz_protect_state = OFF;

    if ((msg_loop & MSG_SERVICE) != MSG_SERVICE &&
        (msg_loop & MSG_TIMEOUT) != MSG_TIMEOUT ) {
      _aqualink_data.service_mode_state = OFF; // IF we get this message then Service / Timeout is off
    }

    if ( ((msg_loop & MSG_SWG_DEVICE) != MSG_SWG_DEVICE) && _aqualink_data.swg_led_state != LED_S_UNKNOWN) {
      // No Additional SWG devices messages like "no flow"
      if ((msg_loop & MSG_SWG) != MSG_SWG && _aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF )
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_OFF);
      else
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_ON);
    }

    // If no AQUAPURE message, either (no SWG, it's set 0, or it's off).
    if ((msg_loop & MSG_SWG) != MSG_SWG && _aqualink_data.swg_led_state != LED_S_UNKNOWN ) {
      if (_aqualink_data.swg_percent != 0 || _aqualink_data.swg_led_state == ON) {
        // Something is wrong here.  Let's check pump, if on set SWG to 0, if off turn SWE off
        if ( _aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF) {
          LOG(AQRS_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is off so setting SWG to off\n");
          setSWGoff(&_aqualink_data);
        } else {
          LOG(AQRS_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is on so setting SWG to 0%%\n");
          changeSWGpercent(&_aqualink_data, 0);
        }
      } else if (isIAQT_ENABLED == false && isONET_ENABLED == false && READ_RSDEV_SWG == false ) {
        //We have no other way to read SWG %=0, so turn SWG on with pump
        if ( _aqualink_data.aqbuttons[PUMP_INDEX].led->state == ON) {
           LOG(AQRS_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is off so setting SWG to off\n");
           //changeSWGpercent(&_aqualink_data, 0);
           setSWGenabled(&_aqualink_data);  
        }
      }
        // NSF Need something to catch startup when SWG=0 so we set it to enabeled.
        // when other ways/protocols to detect SWG=0 are turned off.
    }
    /*
    //  AQUAPURE=0 we never get that message on ALLBUTTON so don't turn off unless filter pump if off
    if ((msg_loop & MSG_SWG) != MSG_SWG && _aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF ) {
       //_aqualink_data.ar_swg_status = SWG_STATUS_OFF;
       setSWGoff(&_aqualink_data);
    }
    */
    if ((msg_loop & MSG_BOOST) != MSG_BOOST) {
      _aqualink_data.boost = false;
      _aqualink_data.boost_msg[0] = '\0';
      //if (_aqualink_data.swg_percent >= 101)
      //  _aqualink_data.swg_percent = 0;
    }

    if ((msg_loop & MSG_BATTERY_LOW) != MSG_BATTERY_LOW)
      _aqualink_data.battery = OK;

#ifdef AQ_RS16
    //if ( _aqconfig_.rs_panel_size >= 16) {
    //if ( (int)PANEL_SIZE >= 16) { // NSF No idea why this fails on RS-4, but it does.  Come back and find out why
    if ( PANEL_SIZE() >= 16 ) {
      //printf("Panel size %d What the fuck am I doing here\n",PANEL_SIZE());
      if ((msg_loop & MSG_RS13BUTTON) != MSG_RS13BUTTON)
        _aqualink_data.aqbuttons[13].led->state = OFF;
      if ((msg_loop & MSG_RS14BUTTON) != MSG_RS14BUTTON)
        _aqualink_data.aqbuttons[14].led->state = OFF;
      if ((msg_loop & MSG_RS15BUTTON) != MSG_RS15BUTTON)
        _aqualink_data.aqbuttons[15].led->state = OFF;
      if ((msg_loop & MSG_RS16BUTTON) != MSG_RS16BUTTON)
        _aqualink_data.aqbuttons[16].led->state = OFF;
    }
#endif
    msg_loop = 0;
    return;
  }

  if (stristr(msg, LNG_MSG_BATTERY_LOW) != NULL)
  {
    _aqualink_data.battery = LOW;
    msg_loop |= MSG_BATTERY_LOW;
    strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI
  }
  else if (stristr(msg, LNG_MSG_POOL_TEMP_SET) != NULL)
  {
    //LOG(AQUA_LOG,LOG_DEBUG, "**************** pool htr long message: %s", &message[20]);
    _aqualink_data.pool_htr_set_point = atoi(message + 20);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (stristr(msg, LNG_MSG_SPA_TEMP_SET) != NULL)
  {
    //LOG(AQUA_LOG,LOG_DEBUG, "spa htr long message: %s", &message[19]);
    _aqualink_data.spa_htr_set_point = atoi(message + 19);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_SET) != NULL)
  {
    //LOG(AQUA_LOG,LOG_DEBUG, "frz protect long message: %s", &message[28]);
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

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(AQRS_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP1_SET) != NULL)
  {
    _aqualink_data.pool_htr_set_point = atoi(message + 28);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(AQRS_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP2_SET) != NULL)
  {
    _aqualink_data.spa_htr_set_point = atoi(message + 27);

    if (_aqualink_data.temp_units == UNKNOWN)
      setUnits(msg);

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(AQRS_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_SERVICE_ACTIVE) != NULL)
  {
    if (_aqualink_data.service_mode_state == OFF)
      LOG(AQRS_LOG,LOG_NOTICE, "AqualinkD set to Service Mode\n");
    _aqualink_data.service_mode_state = ON;
     msg_loop |= MSG_SERVICE;
    //service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_TIMEOUT_ACTIVE) != NULL)
  {
    if (_aqualink_data.service_mode_state == OFF)
      LOG(AQRS_LOG,LOG_NOTICE, "AqualinkD set to Timeout Mode\n");
    _aqualink_data.service_mode_state = FLASH;
     msg_loop |= MSG_TIMEOUT;
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
  else if (stristr(msg, MSG_SWG_PCT) != NULL) 
  {
    if (strncasecmp(msg, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0 && strncasecmp(msg, "AQUAPURE HRS", 12) != 0) {
      changeSWGpercent(&_aqualink_data, atoi(msg + MSG_SWG_PCT_LEN));
    } 
    else if (strncasecmp(msg, "AQUAPURE HRS", 12) != 0 && strncasecmp(msg, "SET AQUAPURE", 12) != 0) 
    {
      if (strcasestr(msg, MSG_SWG_NO_FLOW) != NULL)
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_NO_FLOW);
      else if (strcasestr(msg, MSG_SWG_LOW_SALT) != NULL)
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_LOW_SALT);
      else if (strcasestr(msg, MSG_SWG_HIGH_SALT) != NULL)
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_HI_SALT);
      else if (strcasestr(msg, MSG_SWG_FAULT) != NULL)
        setSWGdeviceStatus(&_aqualink_data, ALLBUTTON, SWG_STATUS_CHECK_PCB);
      
      // Any of these messages want to display.
      strcpy(_aqualink_data.last_display_message, msg);

      msg_loop |= MSG_SWG_DEVICE;
    }
    msg_loop |= MSG_SWG;
  }
  else if (strncasecmp(msg, MSG_SWG_PPM, MSG_SWG_PPM_LEN) == 0)
  {
    _aqualink_data.swg_ppm = atoi(msg + MSG_SWG_PPM_LEN);
    msg_loop |= MSG_SWG;
  }
  else if ((msg[1] == ':' || msg[2] == ':') && msg[strlen(msg) - 1] == 'M')
  { // time in format '9:45 AM'
    strcpy(_aqualink_data.time, msg);
    // Setting time takes a long time, so don't try until we have all other programmed data.
    if (_initWithRS == true && strlen(_aqualink_data.date) > 1 && checkAqualinkTime() != true)
    {
      LOG(AQRS_LOG,LOG_NOTICE, "RS time is NOT accurate '%s %s', re-setting on controller!\n", _aqualink_data.time, _aqualink_data.date);
      aq_programmer(AQ_SET_TIME, NULL, &_aqualink_data);
    }
    else if (_initWithRS == false || _aqconfig_.sync_panel_time == false)
    {
      LOG(AQRS_LOG,LOG_DEBUG, "RS time '%s %s' not checking\n", _aqualink_data.time, _aqualink_data.date);
    }
    else if (_initWithRS == true)
    {
      LOG(AQRS_LOG,LOG_DEBUG, "RS time is accurate '%s %s'\n", _aqualink_data.time, _aqualink_data.date);
    }
    // If we get a time message before REV, the controller didn't see us as we started too quickly.
    /* Don't need to check this anymore with the check for probe before startup.
    if (_gotREV == false)
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Getting control panel information\n", msg);
      aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, &_aqualink_data);
      _gotREV = true; // Force it to true just incase we don't understand the model#
    }
    */
  }
  else if (strstr(msg, " REV ") != NULL)
  { // '8157 REV MMM'
    // A master firmware revision message.
    strcpy(_aqualink_data.version, msg);
    //_gotREV = true;
    LOG(AQRS_LOG,LOG_NOTICE, "Control Panel %s\n", msg);
    if (_initWithRS == false)
    {
      //LOG(ALLBUTTON,LOG_NOTICE, "Standard protocol initialization complete\n");
      queueGetProgramData(ALLBUTTON, &_aqualink_data);
      //queueGetExtendedProgramData(ALLBUTTON, &_aqualink_data, _aqconfig_.use_panel_aux_labels);
      _initWithRS = true;
    }
  }
  else if (stristr(msg, " TURNS ON") != NULL)
  {
    LOG(AQRS_LOG,LOG_NOTICE, "Program data '%s'\n", msg);
  }
  else if (_aqconfig_.override_freeze_protect == TRUE && strncasecmp(msg, "Press Enter* to override Freeze Protection with", 47) == 0)
  {
    //send_cmd(KEY_ENTER, aq_data);
    //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, &_aqualink_data);
    aq_send_cmd(KEY_ENTER);
  }
  // Process any button states (fake LED) for RS12 and above keypads
  // Text will be button label on or off  ie Aux_B2 off or WaterFall off
  
#ifdef AQ_RS16
  //else if ( _aqconfig_.rs_panel_size >= 16 && (rs16 = RS16_endswithLEDstate(msg)) != 0 )
  else if (PANEL_SIZE() >= 16 && (rs16 = RS16_endswithLEDstate(msg)) != 0 )
  {
    msg_loop |= rs16;
    // Do nothing, just stop other else if statments executing
    // make sure we also display the message.
    // Note we only get ON messages here, Off messages will not be sent if something else turned it off
    // use the Onetouch or iAqua equiptment page for off.
    strcpy(_aqualink_data.last_display_message, msg);
  }
#endif
  else if (((msg[4] == ':') || (msg[6] == ':')) && (strncasecmp(msg, "AUX", 3) == 0) )
  { // Should probable check we are in programming mode.
    // 'Aux3: No Label'
    // 'Aux B1: No Label'
    int labelid;
    int ni = 3;
    if (msg[4] == 'B') { ni = 5; }
    labelid = atoi(msg + ni);
    if (labelid > 0 && _aqconfig_.use_panel_aux_labels == true)
    {
      if (ni == 5)
        labelid = labelid + 8;
      else
        labelid = labelid + 1;
      // Aux1: on panel = Button 3 in aqualinkd  (button 2 in array)
      if (strncasecmp(msg+ni+3, "No Label", 8) != 0) {
        _aqualink_data.aqbuttons[labelid].label = prittyString(cleanalloc(msg+ni+2));
        LOG(AQRS_LOG,LOG_NOTICE, "AUX ID %s label set to '%s'\n", _aqualink_data.aqbuttons[labelid].name, _aqualink_data.aqbuttons[labelid].label);
      } else {
        LOG(AQRS_LOG,LOG_NOTICE, "AUX ID %s has no control panel label using '%s'\n", _aqualink_data.aqbuttons[labelid].name, _aqualink_data.aqbuttons[labelid].label);
      }
      //_aqualink_data.aqbuttons[labelid + 1].label = cleanalloc(msg + 5);
    }
  }
  // BOOST POOL 23:59 REMAINING
  else if ( (strncasecmp(msg, "BOOST POOL", 10) == 0) && (strcasestr(msg, "REMAINING") != NULL) ) {
    // Ignore messages if in programming mode.  We get one of these turning off for some strange reason.
    if (in_programming_mode(&_aqualink_data) == false) {
      snprintf(_aqualink_data.boost_msg, 6, "%s", &msg[11]);
      _aqualink_data.boost = true;
      msg_loop |= MSG_BOOST;
      msg_loop |= MSG_SWG;
      //if (_aqualink_data.ar_swg_status != SWG_STATUS_ON) {_aqualink_data.ar_swg_status = SWG_STATUS_ON;}
      if (_aqualink_data.swg_percent != 101) {changeSWGpercent(&_aqualink_data, 101);}
      //boost_msg_count = 0;
    //if (_aqualink_data.active_thread.thread_id == 0)
      strcpy(_aqualink_data.last_display_message, msg); // Also display the message on web UI if not in programming mode
    }
  }
  else
  {
    LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "Ignoring '%s'\n", msg);
    //_aqualink_data.display_message = msg;
    if (in_programming_mode(&_aqualink_data) == false && _aqualink_data.simulate_panel == false &&
        stristr(msg, "JANDY AquaLinkRS") == NULL &&
        //stristr(msg, "PUMP O") == NULL  &&// Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        strncasecmp(msg, "PUMP O", 6) != 0  &&// Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        stristr(msg, "MAINTAIN") == NULL && // Catch 'MAINTAIN TEMP IS OFF'
        stristr(msg, "0 PSI") == NULL /* // Catch some erronious message on test harness
        stristr(msg, "CLEANER O") == NULL &&
        stristr(msg, "SPA O") == NULL &&
        stristr(msg, "AUX") == NULL*/
    )
    { // Catch all AUX1 AUX5 messages
      //_aqualink_data.display_last_message = true;
      strcpy(_aqualink_data.last_display_message, msg);
      //rsm_strncpy(_aqualink_data.last_display_message, (unsigned char *)msg, AQ_MSGLONGLEN, AQ_MSGLONGLEN);
    }
  }

  // Send every message if we are in simulate panel mode
  //if (_aqualink_data.simulate_panel)
  //  strcpy(_aqualink_data.last_display_message, msg);
    //rsm_strncpy(_aqualink_data.last_display_message, (unsigned char *)msg, AQ_MSGLONGLEN, AQ_MSGLONGLEN);
    //ascii(_aqualink_data.last_display_message, msg);


  //LOG(AQRS_LOG,LOG_INFO, "RS Message loop :- '%d'\n", msg_loop);

  // We processed the next message, kick any threads waiting on the message.
//printf ("Message kicking\n");


  kick_aq_program_thread(&_aqualink_data, ALLBUTTON);
}

bool process_packet(unsigned char *packet, int length)
{
  bool rtn = false;
  //static unsigned char last_packet[AQ_MAXPKTLEN];
  static unsigned char last_checksum;
  static char message[AQ_MSGLONGLEN + 1];
  static int processing_long_msg = 0;

  // Check packet against last check if different.
  // Should only use the checksum, not whole packet since it's status messages.
  /*
  if ( packet[PKT_CMD] == CMD_STATUS && (memcmp(packet, last_packet, length) == 0))
  {
    LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "RS Received duplicate, ignoring.\n", length);
    return rtn;
  }
  else
  {
    memcpy(last_packet, packet, length);
    _aqualink_data.last_packet_type = packet[PKT_CMD];
    rtn = true;
  }
  */

  _aqualink_data.last_packet_type = packet[PKT_CMD];

#ifdef AQ_PDA
  if (isPDA_PANEL)
  {
    return process_pda_packet(packet, length);
  }
#endif

  if ( packet[PKT_CMD] == CMD_STATUS && packet[length-3] == last_checksum && ! in_programming_mode(&_aqualink_data) )
  {
    LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "RS Received duplicate, ignoring.\n", length);
    return false;
  }
  else
  {
    last_checksum = packet[length-3];
    rtn = true;
  }

  if (processing_long_msg > 0 && packet[PKT_CMD] != CMD_MSG_LONG)
  {
    processing_long_msg = 0;
    //LOG(AQUA_LOG,LOG_ERR, "RS failed to receive complete long message, received '%s'\n",message);
    //LOG(AQUA_LOG,LOG_DEBUG, "RS didn't finished receiving of MSG_LONG '%s'\n",message);
    processMessage(message);
  }

  LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "RS Received packet type 0x%02hhx length %d.\n", packet[PKT_CMD], length);

  switch (packet[PKT_CMD])
  {
  case CMD_ACK:
    //LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "RS Received ACK length %d.\n", length);
    break;
  case CMD_STATUS:
    //LOG(AQRS_LOG,LOG_DEBUG_SERIAL, "RS Received STATUS length %d.\n", length);
    memcpy(_aqualink_data.raw_status, packet + 4, AQ_PSTLEN);
    processLEDstate();
    if (_aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
      //_aqualink_data.spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == OFF && isSINGLE_DEV_PANEL != true)
    {
      //_aqualink_data.spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
    }
    else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == ON && isSINGLE_DEV_PANEL != true)
    {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
    }

    // COLOR MODE programming relies on state changes, so let any threads know
    //if (_aqualink_data.active_thread.ptype == AQ_SET_LIGHTPROGRAM_MODE) {
    if ( in_light_programming_mode(&_aqualink_data) ) {
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
         //strncpy(message, (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         rsm_strncpy(message, packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         processing_long_msg = index;
         //LOG(AQRS_LOG,LOG_ERR, "Message %s\n",message);
       } else {
         //strncpy(&message[(processing_long_msg * AQ_MSGLEN)], (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         //rsm_strncpy(&message[(processing_long_msg * AQ_MSGLEN)], (unsigned char *)packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         rsm_strncpy(&message[( (index-1) * AQ_MSGLEN)], (unsigned char *)packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         //LOG(AQRS_LOG,LOG_ERR, "Long Message %s\n",message);
         if (++processing_long_msg != index) {
           LOG(AQRS_LOG,LOG_DEBUG, "Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
           //printf("RSM Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
         }
         #ifdef  PROCESS_INCOMPLETE_MESSAGES
           kick_aq_program_thread(&_aqualink_data, ALLBUTTON);
         #endif
       }

       if (index == 0 || index == 5) {
         //printf("RSM process message '%s'\n",message);
         
         // MOVED FROM LINE 701 see if less errors
         //kick_aq_program_thread(&_aqualink_data, ALLBUTTON);

         LOG(AQRS_LOG,LOG_DEBUG, "Processing Message - '%s'\n",message);
         processMessage(message); // This will kick thread
       }
       
     }
    break;
  case CMD_PROBE:
    LOG(AQRS_LOG,LOG_DEBUG, "RS Received PROBE length %d.\n", length);
    //LOG(AQUA_LOG,LOG_INFO, "Synch'ing with Aqualink master device...\n");
    rtn = false;
    break;
  case CMD_MSG_LOOP_ST:
    LOG(AQRS_LOG,LOG_INFO, "RS Received message loop start\n");
    processMessageReset();
    rtn = false;
    break;
  default:
    LOG(AQRS_LOG,LOG_INFO, "RS Received unknown packet, 0x%02hhx\n", packet[PKT_CMD]);
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
      LOG(AQUA_LOG,LOG_NOTICE, "Setting pool heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Pool heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SPA_HTR_SETOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SPA_HTR_SETOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.spa_htr_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, sval, &_aqualink_data);
      LOG(AQUA_LOG,LOG_NOTICE, "Setting spa heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Spa heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == FREEZE_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(FREEZE_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.frz_protect_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, sval, &_aqualink_data);
      LOG(AQUA_LOG,LOG_NOTICE, "Setting freeze protect to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Freeze setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SWG_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SWG_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    //if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF)
    if (_aqualink_data.swg_led_state == OFF)
    {
      // SWG is off, can't set %, so delay the set until it's on.
      LOG(AQUA_LOG,LOG_NOTICE, "SWG is off, delaying request to set %% to %d\n", _aqualink_data.unactioned.value);
      _aqualink_data.swg_delayed_percent = _aqualink_data.unactioned.value;
    }
    else
    {
      if (_aqualink_data.swg_percent != _aqualink_data.unactioned.value)
      {
        aq_programmer(AQ_SET_SWG_PERCENT, sval, &_aqualink_data);
        LOG(AQUA_LOG,LOG_NOTICE, "Setting SWG %% to %d\n", _aqualink_data.unactioned.value);
      }
      else
      {
        LOG(AQUA_LOG,LOG_NOTICE, "SWG % is already %d, not changing\n", _aqualink_data.unactioned.value);
      }
    }
    // Let's just tell everyone we set it, before we actually did.  Makes homekit happy, and it will re-correct on error.
    //_aqualink_data.swg_percent = _aqualink_data.unactioned.value;
#ifdef PRESTATE_SWG_SETPOINT
    setSWGpercent(&_aqualink_data, _aqualink_data.unactioned.value);
#endif
  }
  else if (_aqualink_data.unactioned.type == SWG_BOOST)
  {
    //LOG(AQUA_LOG,LOG_NOTICE, "SWG BOST to %d\n", _aqualink_data.unactioned.value);
    //if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF) {
    if (_aqualink_data.swg_led_state == OFF) {
      LOG(AQUA_LOG,LOG_ERR, "SWG is off, can't Boost pool\n");
    } else if (_aqualink_data.unactioned.value == _aqualink_data.boost ) {
      LOG(AQUA_LOG,LOG_ERR, "Request to turn Boost %s ignored, Boost is already %s\n",_aqualink_data.unactioned.value?"On":"Off", _aqualink_data.boost?"On":"Off");
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
    aq_programmer(AQ_SET_PUMP_RPM, sval, &_aqualink_data);
  }
  else if (_aqualink_data.unactioned.type == PUMP_VSPROGRAM)
  {
    snprintf(sval, 9, "%1d|%d", _aqualink_data.unactioned.id, _aqualink_data.unactioned.value);
    //printf("**** program string '%s'\n",sval);
    aq_programmer(AQ_SET_PUMP_VS_PROGRAM, sval, &_aqualink_data);
  }
  else if (_aqualink_data.unactioned.type == POOL_HTR_INCREMENT && isRSSA_ENABLED) // RSSA for this to work
  {
    LOG(AQUA_LOG,LOG_NOTICE, "Changing pool heater setpoint by %d | %s\n", _aqualink_data.unactioned.value, sval);
    aq_programmer(AQ_ADD_RSSADAPTER_POOL_HEATER_TEMP, sval, &_aqualink_data);
  }
  else if (_aqualink_data.unactioned.type == SPA_HTR_INCREMENT && isRSSA_ENABLED)  // RSSA for this to work
  {
    LOG(AQUA_LOG,LOG_NOTICE, "Changing spa heater setpoint by %d\n", _aqualink_data.unactioned.value);
    aq_programmer(AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP, sval, &_aqualink_data); 
  } 
  else 
  {
    LOG(AQUA_LOG,LOG_ERR, "Unknown request of type %d\n", _aqualink_data.unactioned.type);
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


//printf ("TIMER = %d\n",TIMR_LOG);

#ifdef AQ_MEMCMP
  memset(&_aqualink_data, 0, sizeof (struct aqualinkdata));
#endif

  _aqualink_data.num_pumps = 0;
  _aqualink_data.num_lights = 0;

#ifdef AQ_TM_DEBUG
  addDebugLogMask(DBGT_LOG);
  init_aqd_timer(); // Must clear timers.
#endif
  // Any debug logging masks
  //addDebugLogMask(IAQT_LOG);
  //addDebugLogMask(ONET_LOG);
  //addDebugLogMask(AQRS_LOG);
  //addDebugLogMask(PDA_LOG);
  //addDebugLogMask(NET_LOG);
  //addDebugLogMask(AQUA_LOG);
  //addDebugLogMask(DJAN_LOG);
  //addDebugLogMask(DPEN_LOG);

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
    //LOG(AQUA_LOG,LOG_ERR, "%s Can only be run as root\n", argv[0]);
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize the daemon's parameters.
  init_config();
  cfgFile = defaultCfg;

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

  //initButtons(&_aqualink_data);
  
  read_config(&_aqualink_data, cfgFile);

  // Sanity check on Device ID's against panel type
  if (isRS_PANEL) {
    if (_aqconfig_.device_id >= 0x08 && _aqconfig_.device_id <= 0x0B) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Device ID 0x%02hhx does not match RS panel, please check config!\n", _aqconfig_.device_id);
      return EXIT_FAILURE;
    }
  } else if (isPDA_PANEL) {
    if (_aqconfig_.device_id >= 0x60 && _aqconfig_.device_id <= 0x63) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Device ID 0x%02hhx does not match PDA panel, please check config!\n", _aqconfig_.device_id);
      return EXIT_FAILURE;
    }
  } else {
    LOG(AQUA_LOG,LOG_ERR, "Error unknown panel type, please check config!\n");
    return EXIT_FAILURE;
  }

  if (_aqconfig_.rssa_device_id != 0x00) {
    if (_aqconfig_.rssa_device_id >= 0x48 && _aqconfig_.rssa_device_id <= 0x4B ) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "RSSA Device ID 0x%02hhx does not match RS panel, please check config!\n", _aqconfig_.rssa_device_id);
      return EXIT_FAILURE;
    }
  }

#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  if (_aqconfig_.extended_device_id != 0x00) {
    if ( (_aqconfig_.extended_device_id >= 0x30 && _aqconfig_.extended_device_id <= 0x33) || 
         (_aqconfig_.extended_device_id >= 0x40 && _aqconfig_.extended_device_id <= 0x43) ) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Extended Device ID 0x%02hhx does not match OneTouch or AqualinkTouch ID, please check config!\n", _aqconfig_.extended_device_id);
      return EXIT_FAILURE;
    }
  }
#endif
  /*
  // Just so we catch the heaters, make all panels RS8 or RS16
  //_aqualink_data.total_buttons = _aqconfig_.rs_panel_size + 4; // This would be correct if we re-index heaters.
#ifdef AQ_RS16
  if (_aqconfig_.rs_panel_size >= 12)
    _aqualink_data.total_buttons = 20;
  else
#endif
    _aqualink_data.total_buttons = 12;
  */
  if (cmdln_loglevel != -1)
    _aqconfig_.log_level = cmdln_loglevel;

  if (cmdln_debugRS485)
    _aqconfig_.log_protocol_packets = true;

  if (cmdln_lograwRS485)
    _aqconfig_.log_raw_bytes = true;
      

  if (_aqconfig_.display_warnings_web == true)
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, _aqualink_data.last_display_message);
  else
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, NULL);

  LOG(AQUA_LOG,LOG_NOTICE, "%s v%s\n", AQUALINKD_NAME, AQUALINKD_VERSION);
/*
  LOG(AQUA_LOG,LOG_NOTICE, "Panel set to %s%s-%d %s%s %s\n",
      isRS_PANEL?"RS":"",
      isPDA_PANEL?"PDA":"", // No need for both of these, but for error validation leave it in.
      PANEL_SIZE(),
      isCOMBO_PANEL?"Combo Pool/Spa":"",
      isSINGLE_DEV_PANEL?"Pool/Spa Only":"",
      isDUAL_EQPT_PANEL?"Dual Equipment":"");
*/
  LOG(AQUA_LOG,LOG_NOTICE, "Panel set to %s\n", getPanelString());
  LOG(AQUA_LOG,LOG_NOTICE, "Config log_level         = %d\n", _aqconfig_.log_level);
  LOG(AQUA_LOG,LOG_NOTICE, "Config device_id         = 0x%02hhx\n", _aqconfig_.device_id);
  LOG(AQUA_LOG,LOG_NOTICE, "Config rssa_device_id    = 0x%02hhx\n", _aqconfig_.rssa_device_id);
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  LOG(AQUA_LOG,LOG_NOTICE, "Config extra_device_id   = 0x%02hhx\n", _aqconfig_.extended_device_id);
  LOG(AQUA_LOG,LOG_NOTICE, "Config extra_device_prog = %s\n", bool2text(_aqconfig_.extended_device_id_programming));
#endif
  LOG(AQUA_LOG,LOG_NOTICE, "Config serial_port       = %s\n", _aqconfig_.serial_port);
  //LOG(AQUA_LOG,LOG_NOTICE, "Config rs_panel_size     = %d\n", _aqconfig_.rs_panel_size);
  LOG(AQUA_LOG,LOG_NOTICE, "Config socket_port       = %s\n", _aqconfig_.socket_port);
  LOG(AQUA_LOG,LOG_NOTICE, "Config web_directory     = %s\n", _aqconfig_.web_directory);
  //LOG(AQUA_LOG,LOG_NOTICE, "Config read_all_devices  = %s\n", bool2text(_aqconfig_.read_all_devices));
  LOG(AQUA_LOG,LOG_NOTICE, "Config use_aux_labels    = %s\n", bool2text(_aqconfig_.use_panel_aux_labels));
  LOG(AQUA_LOG,LOG_NOTICE, "Config override frz prot = %s\n", bool2text(_aqconfig_.override_freeze_protect));
#ifndef MG_DISABLE_MQTT
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_server       = %s\n", _aqconfig_.mqtt_server);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_dz_sub_topic = %s\n", _aqconfig_.mqtt_dz_sub_topic);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_dz_pub_topic = %s\n", _aqconfig_.mqtt_dz_pub_topic);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_aq_topic     = %s\n", _aqconfig_.mqtt_aq_topic);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_user         = %s\n", _aqconfig_.mqtt_user);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_passwd       = %s\n", _aqconfig_.mqtt_passwd);
  LOG(AQUA_LOG,LOG_NOTICE, "Config mqtt_ID           = %s\n", _aqconfig_.mqtt_ID);
  if (_aqconfig_.dzidx_air_temp !=TEMP_UNKNOWN) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config idx water temp    = %d\n", _aqconfig_.dzidx_air_temp);
  }
  if (_aqconfig_.dzidx_pool_water_temp !=TEMP_UNKNOWN) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config idx pool temp     = %d\n", _aqconfig_.dzidx_pool_water_temp);
  }
  if (_aqconfig_.dzidx_spa_water_temp !=TEMP_UNKNOWN) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config idx spa temp      = %d\n", _aqconfig_.dzidx_spa_water_temp);
  }
  if (_aqconfig_.dzidx_swg_percent !=TEMP_UNKNOWN) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config idx SWG Percent   = %d\n", _aqconfig_.dzidx_swg_percent);
  }
  if (_aqconfig_.dzidx_swg_ppm !=TEMP_UNKNOWN) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config idx SWG PPM       = %d\n", _aqconfig_.dzidx_swg_ppm);
  }
#endif // MG_DISABLE_MQTT

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    LOG(AQUA_LOG,LOG_NOTICE, "Config PDA Mode          = %s\n", bool2text(isPDA_PANEL));
    LOG(AQUA_LOG,LOG_NOTICE, "Config PDA Sleep Mode    = %s\n", bool2text(_aqconfig_.pda_sleep_mode));
  }
#endif
  LOG(AQUA_LOG,LOG_NOTICE, "Config force SWG         = %s\n", bool2text(_aqconfig_.force_swg));
  /* removed until domoticz has a better virtual thermostat
  LOG(AQUA_LOG,LOG_NOTICE, "Config idx pool thermostat = %d\n", _aqconfig_.dzidx_pool_thermostat);
  LOG(AQUA_LOG,LOG_NOTICE, "Config idx spa thermostat  = %d\n", _aqconfig_.dzidx_spa_thermostat);
  */

  LOG(AQUA_LOG,LOG_NOTICE, "Config deamonize         = %s\n", bool2text(_aqconfig_.deamonize));
  LOG(AQUA_LOG,LOG_NOTICE, "Config log_file          = %s\n", _aqconfig_.log_file);
  LOG(AQUA_LOG,LOG_NOTICE, "Config enable scheduler  = %s\n", bool2text(_aqconfig_.enable_scheduler));
  LOG(AQUA_LOG,LOG_NOTICE, "Config light_pgm_mode    = %.2f\n", _aqconfig_.light_programming_mode);
  LOG(AQUA_LOG,LOG_NOTICE, "Debug RS485 protocol     = %s\n", bool2text(_aqconfig_.log_protocol_packets));
  LOG(AQUA_LOG,LOG_NOTICE, "Debug RS485 protocol raw = %s\n", bool2text(_aqconfig_.log_raw_bytes));
  //LOG(AQUA_LOG,LOG_NOTICE, "Use PDA 4 auxiliary info = %s\n", bool2text(_aqconfig_.use_PDA_auxiliary));
  //LOG(AQUA_LOG,LOG_NOTICE, "Read Pentair Packets     = %s\n", bool2text(_aqconfig_.read_pentair_packets));
  // logMessage (LOG_NOTICE, "Config serial_port = %s\n", config_parameters->serial_port);
  LOG(AQUA_LOG,LOG_NOTICE, "Display warnings in web  = %s\n", bool2text(_aqconfig_.display_warnings_web));
  LOG(AQUA_LOG,LOG_NOTICE, "Keep panel time in sync  = %s\n", bool2text(_aqconfig_.sync_panel_time));
  
  LOG(AQUA_LOG,LOG_NOTICE, "Read SWG direct          = %s\n", bool2text(READ_RSDEV_SWG));
  LOG(AQUA_LOG,LOG_NOTICE, "Read ePump direct        = %s\n", bool2text(READ_RSDEV_ePUMP));
  LOG(AQUA_LOG,LOG_NOTICE, "Read vsfPump direct      = %s\n", bool2text(READ_RSDEV_vsfPUMP));

  if (READ_RSDEV_SWG && _aqconfig_.swg_zero_ignore != DEFAULT_SWG_ZERO_IGNORE_COUNT)
    LOG(AQUA_LOG,LOG_NOTICE, "Ignore SWG 0 msg count   = %d\n", _aqconfig_.swg_zero_ignore);

  if (_aqconfig_.readahead_b4_write == true)
    LOG(AQUA_LOG,LOG_NOTICE, "Serial Read Ahead Write  = %s\n", bool2text(_aqconfig_.readahead_b4_write));

  if (_aqconfig_.thread_netservices)
    LOG(AQUA_LOG,LOG_NOTICE, "Thread Network Services  = %s\n", bool2text(_aqconfig_.thread_netservices));

  if (_aqconfig_.rs_poll_speed < 0 && !_aqconfig_.thread_netservices) {
    LOG(AQUA_LOG,LOG_WARNING, "Negative RS Poll Speed is only valid when using Thread Network Services, resetting to %d\n",DEFAULT_POLL_SPEED_NON_THREADDED);
    _aqconfig_.rs_poll_speed = DEFAULT_POLL_SPEED_NON_THREADDED;
  }
  if (_aqconfig_.rs_poll_speed != DEFAULT_POLL_SPEED)
    LOG(AQUA_LOG,LOG_NOTICE, "RS Poll Speed            = %d\n", _aqconfig_.rs_poll_speed);

  if (_aqconfig_.rs_poll_speed < 0 && _aqconfig_.readahead_b4_write) {
    LOG(AQUA_LOG,LOG_WARNING, "Serial Read Ahead Write is not valid when using Negative RS Poll Speed, turning Serial Read Ahead Write off\n");
    _aqconfig_.readahead_b4_write = false;
  }

  //for (i = 0; i < TOTAL_BUTONS; i++)
  for (i = 0; i < _aqualink_data.total_buttons; i++)
  {
    //char ext[] = " VSP ID None | AL ID 0 ";
    char ext[40];
    ext[0] = '\0';
    for (j = 0; j < _aqualink_data.num_pumps; j++) {
      if (_aqualink_data.pumps[j].button == &_aqualink_data.aqbuttons[i]) {
        sprintf(ext, "VSP ID 0x%02hhx | PumpID %-1d |",_aqualink_data.pumps[j].pumpID, _aqualink_data.pumps[j].pumpIndex);
      }
    }
    for (j = 0; j < _aqualink_data.num_lights; j++) {
      if (_aqualink_data.lights[j].button == &_aqualink_data.aqbuttons[i]) {
        sprintf(ext,"Light Progm | CTYPE %-1d  |",_aqualink_data.lights[j].lightType);
      }
    }
    if (_aqualink_data.aqbuttons[i].dz_idx > 0)
      sprintf(ext+strlen(ext), "dzidx %-3d", _aqualink_data.aqbuttons[i].dz_idx);
/*
#ifdef AQ_PDA
    if (isPDA_PANEL) {
      LOG(AQUA_LOG,LOG_NOTICE, "Config BTN %-13s = label %-15s | PDAlabel %-15s | %s\n", 
                           _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label,
                           _aqualink_data.aqbuttons[i].pda_label, ext);
    } else
#endif
*/
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Config BTN %-13s = label %-15s | %s\n", 
                           _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label, ext);  
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


/* Point of this is to sent ack as quickly as possible, all checks should be done prior to calling this.*/
void caculate_ack_packet(int rs_fd, unsigned char *packet_buffer, emulation_type source) 
{
  switch (source) {
    case ALLBUTTON:
      send_extended_ack(rs_fd, (packet_buffer[PKT_CMD]==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_aq_cmd(&_aqualink_data));
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AllButton Emulation type Processed packet in");
    break;
    case RSSADAPTER:
      send_jandy_command(rs_fd, get_rssa_cmd(packet_buffer[PKT_CMD]), 4);
      remove_rssa_cmd();
    /*
      if (packet_buffer[PKT_CMD] == CMD_PROBE)
        send_extended_ack(rs_fd, 0x00, 0x05);
      else
        send_extended_ack(rs_fd, 0x00, 0x00);*/
    break;
#ifdef AQ_ONETOUCH
    case ONETOUCH:
      send_extended_ack(rs_fd, ACK_ONETOUCH, pop_ot_cmd(packet_buffer[PKT_CMD]));
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"OneTouch Emulation type Processed packet in");
    break;
#endif
#ifdef AQ_IAQTOUCH
    case IAQTOUCH:
      if (packet_buffer[PKT_CMD] != CMD_IAQ_CTRL_READY)
        send_extended_ack(rs_fd, ACK_IAQ_TOUCH, pop_iaqt_cmd(packet_buffer[PKT_CMD]));
      else {
        unsigned char *cmd;
        int size = ref_iaqt_control_cmd(&cmd);
        send_jandy_command(rs_fd, cmd, size);
        rem_iaqt_control_cmd(cmd);
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AquaTouch Emulation type Processed packet in");
    break;
#endif
#ifdef AQ_PDA
    case AQUAPDA:
      if (_aqconfig_.pda_sleep_mode && pda_shouldSleep()) {
        LOG(PDA_LOG,LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      } else {
        send_extended_ack(rs_fd, ACK_PDA, pop_aq_cmd(&_aqualink_data));
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"PDA Emulation type Processed packet in");
    break;
#endif
    default:
      LOG(AQUA_LOG,LOG_WARNING, "Can't caculate ACK, No idea what packet this source packet was for!\n");
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Unknown Emulation type Processed packet in");
    break;
  }

/*
#ifdef AQ_TM_DEBUG
  static struct timespec now;
  static struct timespec elapsed;
  clock_gettime(CLOCK_REALTIME, &now);
  timespec_subtract(&elapsed, &now, &_rs_packet_readitme);
  LOG(AQUA_LOG,LOG_NOTICE, "Emulation type %d. Processed packet in %d.%02ld sec (%08ld ns)\n",source, elapsed.tv_sec, elapsed.tv_nsec / 1000000L, elapsed.tv_nsec);
#endif
*/

}
/*
void caculate_ack_packet_old(int rs_fd, unsigned char *packet_buffer) {
  static int delayAckCnt = 0;

  if (packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id) {
    if (onetouch_enabled())
      send_extended_ack(rs_fd, ACK_ONETOUCH, pop_ot_cmd(packet_buffer[PKT_CMD]));
    else if (iaqtouch_enabled()) {
      if (packet_buffer[PKT_CMD] != CMD_IAQ_CTRL_READY)
        send_extended_ack(rs_fd, ACK_IAQ_TOUCH, pop_iaqt_cmd(packet_buffer[PKT_CMD]));
      else {
        unsigned char *cmd;
        int size = ref_iaqt_control_cmd(&cmd);
        send_jandy_command(rs_fd, cmd, size);
        rem_iaqt_control_cmd(cmd);
      }
    }

    return;
  } 

  // if PDA mode, should we sleep? if not Can only send command to status message on PDA.
#ifdef AQ_PDA
  if (_aqconfig_.pda_mode == true) {
      //pda_programming_thread_check(&_aqualink_data);
      if (_aqconfig_.pda_sleep_mode && pda_shouldSleep()) {
        LOG(AQUA_LOG,LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      } else {
        send_extended_ack(rs_fd, ACK_PDA, pop_aq_cmd(&_aqualink_data));
      }
    
  } else
#endif 
  if (_aqualink_data.simulate_panel && in_programming_mode(&_aqualink_data) == false) { 
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
      LOG(AQUA_LOG,LOG_NOTICE, "Sending display busy due to Simulator mode \n");
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
*/
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
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused ID 0x%02hhx\n",lastID);
      LOG(AQUA_LOG,LOG_WARNING, "Please add 'device_id=0x%02hhx', to AqualinkD's configuration file\n",lastID);
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
  //rsDeviceType interestedInNextAck = DRS_NONE;
  //bool changed = false;
  //int swg_zero_cnt = 0;
  int i;
  //int delayAckCnt = 0;
  bool got_probe = false;
  bool got_probe_extended = false;
  bool got_probe_rssa = false;
  bool print_once = false;
  //unsigned char previous_packet_to = NUL; // bad name, it's not previous, it's previous that we were interested in.

  // NSF need to find a better place to init this.
  //_aqualink_data.aq_command = 0x00;
  sprintf(_aqualink_data.last_display_message, "%s", "Connecting to Control Panel");
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
  _aqualink_data.ar_swg_device_status = SWG_STATUS_UNKNOWN;
  _aqualink_data.swg_led_state = LED_S_UNKNOWN;
  _aqualink_data.swg_delayed_percent = TEMP_UNKNOWN;
  _aqualink_data.temp_units = UNKNOWN;
  //_aqualink_data.single_device = false;
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
    //_aqualink_data.ar_swg_device_status = SWG_STATUS_OFF;
    _aqualink_data.swg_led_state = OFF;
    _aqualink_data.swg_percent = 0;
    _aqualink_data.swg_ppm = 0;
  }

  if (!start_net_services(&mgr, &_aqualink_data))
  {
    LOG(AQUA_LOG,LOG_ERR, "Can not start webserver on port %s.\n", _aqconfig_.socket_port);
    exit(EXIT_FAILURE);
  }

  startPacketLogger();

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGQUIT, intHandler);

  int blank_read = 0;
  if (_aqconfig_.rs_poll_speed < 0)
    rs_fd = init_blocking_serial_port(_aqconfig_.serial_port);
  else if (_aqconfig_.readahead_b4_write)
    rs_fd = init_readahead_serial_port(_aqconfig_.serial_port);
  else
    rs_fd = init_serial_port(_aqconfig_.serial_port);

  if (rs_fd == -1) {
   LOG(AQUA_LOG,LOG_ERR, "Error Aqualink setting serial port: %s\n", _aqconfig_.serial_port);
   exit(EXIT_FAILURE); 
  }
  LOG(AQUA_LOG,LOG_NOTICE, "Listening to Aqualink RS8 on serial port: %s\n", _aqconfig_.serial_port);

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    init_pda(&_aqualink_data);
    if (_aqconfig_.extended_device_id != 0x00)
    {
      LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon can't use extended_device_id in PDA mode, ignoring value '0x%02hhx' from cfg\n",_aqconfig_.extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
    }
  }
#endif

#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  if (_aqconfig_.extended_device_id != 0x00)
  {
#ifdef AQ_ONETOUCH
    if (_aqconfig_.extended_device_id >= 0x40 && _aqconfig_.extended_device_id <= 0x43)
      addPanelOneTouchInterface();
    else
#endif
#ifdef AQ_IAQTOUCH
    if (_aqconfig_.extended_device_id >= 0x30 && _aqconfig_.extended_device_id <= 0x33)
      addPanelIAQTouchInterface();
    else
#endif 
    {
      LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid extended_device_id, ignoring value '0x%02hhx' from cfg\n",_aqconfig_.extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
      //set_extended_device_id_programming(false);
    }
  
    if (_aqconfig_.extended_device_id_programming == true && (isONET_ENABLED || isIAQT_ENABLED) )
    {
      changePanelToExtendedIDProgramming();
    }
  } else {
    got_probe_extended = true;
    // Just to stop any confusion from bad cfg enteries
    if (_aqconfig_.extended_device_id_programming == true)
      LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid extended_device_id, ignoring value 'extended_device_id_programming' from cfg\n");

    _aqconfig_.extended_device_id_programming = false;
    //set_extended_device_id_programming(false);
  }
#else
  // If no extended protocol, we can't test for probe
  got_probe_extended = true;
#endif 

  // Enable or disable rs serial adapter interface.
  if (_aqconfig_.rssa_device_id != 0x00)
    addPanelRSserialAdapterInterface();
  else
    got_probe_rssa = true;
  
  if (_aqconfig_.device_id == 0x00) {
    LOG(AQUA_LOG,LOG_NOTICE, "Searching for valid ID, please configure one for faster startup\n");
  }

  LOG(AQUA_LOG,LOG_NOTICE, "Waiting for Control Panel probe\n");
  i=0;

  // Turn off read ahead while dealing with probes
  bool read_ahead = _aqconfig_.readahead_b4_write;
  _aqconfig_.readahead_b4_write = false;

  // Loop until we get the probe messages, that means we didn;t start too soon after last shutdown.
  while ( (got_probe == false || got_probe_rssa == false || got_probe_extended == false ) && _keepRunning == true)
  {
    if (blank_read == MAX_ZERO_READ_BEFORE_RECONNECT) {
      LOG(AQUA_LOG,LOG_ERR, "Nothing read on '%s', are you sure that's right?\n",_aqconfig_.serial_port);
    } else if (blank_read == MAX_ZERO_READ_BEFORE_RECONNECT*2) {
      LOG(AQUA_LOG,LOG_ERR, "Nothing read on '%s', are you sure that's right?\n",_aqconfig_.serial_port);
    } else if (blank_read == MAX_ZERO_READ_BEFORE_RECONNECT*3) {
      LOG(AQUA_LOG,LOG_ERR, "I'm done, exiting, please check '%s'\n",_aqconfig_.serial_port);
      return;
    }
/*
    if (_aqconfig_.log_raw_RS_bytes)
      packet_length = get_packet_lograw(rs_fd, packet_buffer);
    else
      packet_length = get_packet(rs_fd, packet_buffer);
*/ 
    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length > 0 && _aqconfig_.device_id == 0x00) {
      blank_read = 0;
      _aqconfig_.device_id = find_unused_address(packet_buffer);
      continue;
    }
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.device_id && got_probe == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' Standard Protocol\n",_aqconfig_.device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.device_id);
          print_once=true;
        } 
      }
      // NSF Should put some form of timeout here and exit.
    }
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.rssa_device_id && got_probe_rssa == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe_rssa = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' RS SerialAdapter Protocol\n",_aqconfig_.rssa_device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.rssa_device_id);
          print_once=true;
        } 
      }
      // NSF Should put some form of timeout here and exit.
    }
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id && got_probe_extended == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe_extended = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' Extended Protocol\n",_aqconfig_.extended_device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.extended_device_id);
          print_once=true;
        }
      }
      // NSF Should put some form of timeout here and continue with no extended ID
    }
#endif
    else if (packet_length <= 0) {
      blank_read++;
      //printf("Blank Reads %d\n",blank_read);
      if (_aqconfig_.rs_poll_speed < 0)
        LOG(AQUA_LOG,LOG_DEBUG, "Blank RS485 read\n");
      else
        delay(2);
    }
    else if (packet_length > 0) {
      blank_read = 0;
      if (i++ > 1000) {
        if(!got_probe) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', giving up! (please check config)\n",_aqconfig_.device_id);    
        }
        if(!got_probe_rssa) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', giving up! (please check config)\n",_aqconfig_.rssa_device_id);    
        }
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
        if(!got_probe_extended) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', giving up! (please check config)\n",_aqconfig_.extended_device_id);    
        }
#endif
        stopPacketLogger();
        close_serial_port(rs_fd);
        //mg_mgr_free(&mgr);
        stop_net_services(&mgr);
        return;
      }
    }
  }
  


  /*
   *
   *    This is the main loop  
   * 
   * 
   * 
   */

//int max_blank_read = 0;

  _aqconfig_.readahead_b4_write = read_ahead;

  LOG(AQUA_LOG,LOG_NOTICE, "Starting communication with Control Panel\n");

  int blank_read_reconnect = MAX_ZERO_READ_BEFORE_RECONNECT;
  // Not the best way to do this, but ok for moment
  if (_aqconfig_.rs_poll_speed == 0)
    blank_read_reconnect = blank_read_reconnect * 50;

  blank_read = 0;
  // OK, Now go into infinate loop
  while (_keepRunning == true)
  {
    //printf("%d ",blank_read);
    while ((rs_fd < 0 || blank_read >= blank_read_reconnect) && _keepRunning == true)
    {
      //printf("rs_fd  =% d\n",rs_fd);
      if (rs_fd < 0)
      {
        // sleep(1);
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon waiting to connect to master device...\n");
        broadcast_aqualinkstate_error(mgr.active_connections, CONNECTION_ERROR);
        //mg_mgr_poll(&mgr, 1000); // Sevice messages
        //mg_mgr_poll(&mgr, 3000); // should do nothing for 3 seconds.
        poll_net_services(&mgr, 1000);
        poll_net_services(&mgr, 3000);
        // broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
      }
      else
      {
        LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon looks like serial error, resetting.\n");
        close_serial_port(rs_fd);

        if (_aqconfig_.rs_poll_speed < 0)
          rs_fd = init_blocking_serial_port(_aqconfig_.serial_port);
        else if (_aqconfig_.readahead_b4_write)
          rs_fd = init_readahead_serial_port(_aqconfig_.serial_port);
        else
          rs_fd = init_serial_port(_aqconfig_.serial_port);
      }

      blank_read = 0;
    }

    packet_length = get_packet(rs_fd, packet_buffer);
    /*
    if (_aqconfig_.log_raw_RS_bytes)
      packet_length = get_packet_lograw(rs_fd, packet_buffer);
    else
      packet_length = get_packet(rs_fd, packet_buffer);
    */
    /*
    if (packet_length == AQSERR_READ || packet_length == AQSERR_TIMEOUT)
    {
      // Unrecoverable read error. Force an attempt to reconnect.
      if (_aqconfig_.rs_poll_speed < 0) {
        LOG(AQUA_LOG,LOG_ERR, "Bad serial read or connection\n");
        blank_read = blank_read_reconnect;
      } else {
        blank_read++;
      }
    }
    else*/ 
    if (packet_length <= 0)
    {
      if (_aqconfig_.rs_poll_speed < 0) {
        LOG(AQUA_LOG,LOG_ERR, "Nothing read on blocking serial port\n");
        blank_read = blank_read_reconnect;
      } else if (packet_length == AQSERR_READ)
        blank_read = blank_read_reconnect;
      //if (blank_read > max_blank_read) {
      //  LOG(AQUA_LOG,LOG_NOTICE, "Nothing read on serial %d\n",blank_read);
      //  max_blank_read = blank_read;
      //}
      blank_read++;
    }
    else if (packet_length > 0)
    {
      DEBUG_TIMER_START(&_rs_packet_timer);

      blank_read = 0;
      //changed = false;

      if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.device_id && getProtocolType(packet_buffer) == JANDY)
      {
        if (getLogLevel(AQUA_LOG) >= LOG_DEBUG) {
          LOG(AQUA_LOG,LOG_DEBUG, "RS received packet of type %s length %d\n", get_packet_type(packet_buffer, packet_length), packet_length);
          logPacketRead(packet_buffer, packet_length);
        }
        
        _aqualink_data.updated = process_packet(packet_buffer, packet_length);
#ifdef AQ_PDA 
        if (isPDA_PANEL)
          caculate_ack_packet(rs_fd, packet_buffer, AQUAPDA);
        else
#endif
          caculate_ack_packet(rs_fd, packet_buffer, ALLBUTTON);

        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AllButton Emulation Processed packet in");
      }
      else if (packet_length > 0 && isRSSA_ENABLED && packet_buffer[PKT_DEST] == _aqconfig_.rssa_device_id && getProtocolType(packet_buffer) == JANDY) {
        _aqualink_data.updated = process_rssadapter_packet(packet_buffer, packet_length, &_aqualink_data);
        caculate_ack_packet(rs_fd, packet_buffer, RSSADAPTER);
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"SerialAdapter Emulation Processed packet in");
      }
#ifdef AQ_ONETOUCH
      else if (packet_length > 0 && isONET_ENABLED && packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id && getProtocolType(packet_buffer) == JANDY) {
        _aqualink_data.updated = process_onetouch_packet(packet_buffer, packet_length, &_aqualink_data);
        caculate_ack_packet(rs_fd, packet_buffer, ONETOUCH);
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"OneTouch Emulation Processed packet in");
      }
#endif
#ifdef AQ_IAQTOUCH
      else if (packet_length > 0 && isIAQT_ENABLED && packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id && getProtocolType(packet_buffer) == JANDY) {
        _aqualink_data.updated = process_iaqtouch_packet(packet_buffer, packet_length, &_aqualink_data);
        caculate_ack_packet(rs_fd, packet_buffer, IAQTOUCH);
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AquaTouch Emulation Processed packet in");
      }
#endif
      //else if (packet_length > 0 && _aqconfig_.read_all_devices == true)
      else if (packet_length > 0 && _aqconfig_.read_RS485_devmask > 0)
      {
        if (getProtocolType(packet_buffer) == JANDY)
        {
          _aqualink_data.updated = processJandyPacket(packet_buffer, packet_length, &_aqualink_data);
          /*
          // We received the ack from a Jandy device we are interested in
          if (packet_buffer[PKT_DEST] == DEV_MASTER && interestedInNextAck != DRS_NONE)
          {
            if (interestedInNextAck == DRS_SWG)
            {
              _aqualink_data.updated = processPacketFromSWG(packet_buffer, packet_length, &_aqualink_data);
            }
            else if (interestedInNextAck == DRS_EPUMP)
            {
              _aqualink_data.updated = processPacketFromJandyPump(packet_buffer, packet_length, &_aqualink_data);
            }
            interestedInNextAck = DRS_NONE;
            previous_packet_to = NUL;
          }
          // We were expecting an ack from Jandy device but didn't receive it.
          else if (packet_buffer[PKT_DEST] != DEV_MASTER && interestedInNextAck != DRS_NONE )
          {
            if (interestedInNextAck == DRS_SWG && _aqualink_data.ar_swg_device_status != SWG_STATUS_OFF)
            { // SWG Offline
              processMissingAckPacketFromSWG(previous_packet_to, &_aqualink_data);
            }
            else if (interestedInNextAck == DRS_EPUMP)
            { // ePump offline
              processMissingAckPacketFromJandyPump(previous_packet_to, &_aqualink_data);
            }
            interestedInNextAck = DRS_NONE;
            previous_packet_to = NUL;
          }
          else if (READ_RSDEV_SWG && packet_buffer[PKT_DEST] == SWG_DEV_ID)
          {
            interestedInNextAck = DRS_SWG;
            _aqualink_data.updated = processPacketToSWG(packet_buffer, packet_length, &_aqualink_data, _aqconfig_.swg_zero_ignore);
            previous_packet_to = packet_buffer[PKT_DEST];
          }
          else if (READ_RSDEV_ePUMP && packet_buffer[PKT_DEST] >= JANDY_DEC_PUMP_MIN && packet_buffer[PKT_DEST] <= JANDY_DEC_PUMP_MAX)
          {
            interestedInNextAck = DRS_EPUMP;
            _aqualink_data.updated = processPacketToJandyPump(packet_buffer, packet_length, &_aqualink_data);
            previous_packet_to = packet_buffer[PKT_DEST];
          }
          else
          {
            interestedInNextAck = DRS_NONE;
            previous_packet_to = NUL;
          }*/
        }
        // Process Pentair Device Packed (pentair have to & from in message, so no need to)
        else if (getProtocolType(packet_buffer) == PENTAIR && READ_RSDEV_vsfPUMP) {
          _aqualink_data.updated = processPentairPacket(packet_buffer, packet_length, &_aqualink_data);
          // In the future probably add code to catch device offline (ie missing reply message)
        }
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Processed (readonly) packet in");
      } else {
        DEBUG_TIMER_CLEAR(_rs_packet_timer); // Clear timer, no need to print anything
      }

      if (_aqualink_data.updated) {
        broadcast_aqualinkstate(mgr.active_connections);
        //_aqualink_data.updated = false;
      }
    }

    //mg_mgr_poll(&mgr, 10);
    //mg_mgr_poll(&mgr, 5);
    //mg_mgr_poll(&mgr, packet_length>0?0:_aqconfig_.net_poll_wait); // Don;t wait if we read something.
    poll_net_services(&mgr, packet_length>0?0:_aqconfig_.rs_poll_speed); // Don;t wait if we read something.

    //tcdrain(rs_fd); // Make sure buffer has been sent.
    //mg_mgr_poll(&mgr, 0);

    // Any unactioned commands
    if (_aqualink_data.unactioned.type != NO_ACTION)
    {
      time_t now;
      time(&now);
      if (difftime(now, _aqualink_data.unactioned.requested) > 2)
      {
        LOG(AQUA_LOG,LOG_DEBUG, "Actioning delayed request\n");
        action_delayed_request();
      }
    }

    //tcdrain(rs_fd); // Make sure buffer has been sent.
    //delay(10);
  }
  
  //if (_aqconfig_.debug_RSProtocol_packets) stopPacketLogger();
  stopPacketLogger();

  // Stop network
  stop_net_services(&mgr);

  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  //mg_mgr_free(&mgr);

  // NSF need to run through config memory and clean up.
  LOG(AQUA_LOG,LOG_NOTICE, "Exit!\n");
  exit(EXIT_FAILURE);
}


