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
#include "allbutton.h"
#include "allbutton_aq_programmer.h"
#include "onetouch.h"
#include "onetouch_aq_programmer.h"
#include "iaqtouch.h"
#include "iaqtouch_aq_programmer.h"
#include "iaqualink.h"
#include "version.h"
#include "rs_msg_utils.h"
#include "serialadapter.h"
#include "simulator.h"
#include "debug_timer.h"
#include "aq_scheduler.h"
#include "json_messages.h"
#include "aq_systemutils.h"

#ifdef AQ_MANAGER
#include "serial_logger.h"
#endif

/*
#if defined AQ_DEBUG || defined AQ_TM_DEBUG
  #include "timespec_subtract.h"
#endif
*/
//#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;
static volatile bool _restart = false;
//char** _argv;
//static struct aqconfig _aqconfig_;
static struct aqualinkdata _aqualink_data;

// NSF Need to remove self
char *_self;
char *_cfgFile;
int _cmdln_loglevel = -1;
bool _cmdln_debugRS485 = false;
bool _cmdln_lograwRS485 = false;
bool _cmdln_nostartupcheck = false;

#ifdef AQ_TM_DEBUG
  //struct timespec _rs_packet_readitme;
  int _rs_packet_timer;
#endif

#define AddAQDstatusMask(mask) (_aqualink_data.status_mask |= mask)
#define RemoveAQDstatusMask(mask) (_aqualink_data.status_mask &= ~mask)


void main_loop();
int startup(char *self, char *cfgFile);


bool isAqualinkDStopping() {
  return !_keepRunning;
}

void intHandler(int sig_num)
{
  if (sig_num == SIGRUPGRADE) {
    if (! run_aqualinkd_upgrade(false)) {
      LOG(AQUA_LOG,LOG_ERR, "AqualinkD upgrade failed!\n");
    }
    return; // Let the upgrade process terminate us.
  }

  LOG(AQUA_LOG,LOG_WARNING, "Stopping!\n");

  _keepRunning = false;

  if (sig_num == SIGRESTART) {
    LOG(AQUA_LOG,LOG_WARNING, "Restarting AqualinkD!\n");
    // If we are deamonized, we need to use the system
    if (_aqconfig_.deamonize) {
      if(fork() == 0) {
        sleep(2);
        char *newargv[] = {"/bin/systemctl", "restart", "aqualinkd", NULL};
        char *newenviron[] = { NULL };
        execve(newargv[0], newargv, newenviron);
        exit (EXIT_SUCCESS);
      }
    } else {
      _restart = true;
    }
  }
  //LOG(AQUA_LOG,LOG_NOTICE, "Stopping!\n");
  //if (dummy){}// stop compile warnings

  // In blocking mode, die as cleanly as possible.
#ifdef AQ_NO_THREAD_NETSERVICE
  if (_aqconfig_.rs_poll_speed < 0) {
    stopPacketLogger();
    // This should force port to close and do somewhat gracefull exit.
    close_blocking_serial_port();
    //exit(-1);
  }
#else
  stopPacketLogger();
  // This should force port to close and do somewhat gracefull exit.
  if (serial_blockingmode())
    close_blocking_serial_port();
#endif
}

bool isVirtualButtonEnabled() {
  return _aqualink_data.virtual_button_start>0?true:false;
}


// Should move to panel.
bool checkAqualinkTime()
{
  static time_t last_checked = 0;
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
  else if (strlen(_aqualink_data.date) <=0 ||
           strlen(_aqualink_data.time) <=0) 
  {
    LOG(AQUA_LOG,LOG_DEBUG, "time not checked, no time from panel\n");
    return true;
  }
  else
  {
    last_checked = now;
    //return false;
  }

  char datestr[DATE_STRING_LEN];
#ifdef AQ_PDA
  if (isPDA_PANEL && !isPDA_IAQT) {
    LOG(AQUA_LOG,LOG_DEBUG, "PDA Time Check\n");
    // date is simply a day or week for PDA.
    localtime_r(&now, &aq_tm);
    int real_wday = aq_tm.tm_wday; // NSF Need to do this better, we could be off by 7 days
    snprintf(datestr, DATE_STRING_LEN, "%.12s %.8s",_aqualink_data.date,_aqualink_data.time);

    if (strptime(datestr, "%a %I:%M%p", &aq_tm) == NULL) {
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

  char buff[30];
  
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time from : %s\n", datestr);
  strftime(buff, 30, "%Y-%m-%d %H:%M:%S %a", &aq_tm);
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time      : %s\n", buff);

  aqualink_time = mktime(&aq_tm);

  strftime(buff, 30, "%Y-%m-%d %H:%M:%S", localtime(&aqualink_time));
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd converted time    : %s\n", buff);
  strftime(buff, 30, "%Y-%m-%d %H:%M:%S", localtime(&now));
  LOG(AQUA_LOG,LOG_DEBUG, "System time                 : %s\n", buff);


  time_difference = (int)difftime(now, aqualink_time);

  strftime(buff, 30, "%m/%d/%y %I:%M %p", localtime(&now));
  LOG(AQUA_LOG,LOG_INFO, "Aqualink time '%s' is off system time '%s' by %d seconds...\n", datestr, buff, time_difference);

  if (abs(time_difference) < ACCEPTABLE_TIME_DIFF)
  {
    // Time difference is less than or equal to ACCEPTABLE_TIME_DIFF seconds (1 1/2 minutes).
    // Set the return value to true.
    return true;
  }

  return false;
}



void action_delayed_request()
{
  char sval[10];
  snprintf(sval, 9, "%d", _aqualink_data.unactioned.value);

  // If we don't know the units yet, we can't action setpoint, so wait until we do.
  if (_aqualink_data.temp_units == UNKNOWN && 
     (_aqualink_data.unactioned.type == POOL_HTR_SETPOINT || _aqualink_data.unactioned.type == SPA_HTR_SETPOINT || _aqualink_data.unactioned.type == FREEZE_SETPOINT || _aqualink_data.unactioned.type == CHILLER_SETPOINT))
    return;

  if (_aqualink_data.unactioned.type == POOL_HTR_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(POOL_HTR_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
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
  else if (_aqualink_data.unactioned.type == SPA_HTR_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SPA_HTR_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
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
  else if (_aqualink_data.unactioned.type == CHILLER_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(CHILLER_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.chiller_set_point != _aqualink_data.unactioned.value)
    {
      aq_programmer(AQ_SET_CHILLER_TEMP, sval, &_aqualink_data);
      LOG(AQUA_LOG,LOG_NOTICE, "Setting Chiller setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Chiller setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
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
        LOG(AQUA_LOG,LOG_NOTICE, "SWG %% is already %d, not changing\n", _aqualink_data.unactioned.value);
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
    if ((_aqualink_data.swg_led_state == OFF) && (_aqualink_data.boost == false)) {
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
  else if (_aqualink_data.unactioned.type == LIGHT_MODE) {
    panel_device_request(&_aqualink_data, LIGHT_MODE, _aqualink_data.unactioned.id, _aqualink_data.unactioned.value, UNACTION_TIMER);
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
  _restart = false;
  char defaultCfg[] = "./aqualinkd.conf";
  char *cfgFile;
  
//printf ("TIMER = %d\n",TIMR_LOG);

#ifdef AQ_MEMCMP
  memset(&_aqualink_data, 0, sizeof (struct aqualinkdata));
#endif

  _aqualink_data.num_pumps = 0;
  _aqualink_data.num_lights = 0;
  _aqualink_data.num_sensors = 0;

#ifdef AQ_TM_DEBUG
  addDebugLogMask(DBGT_LOG);
  init_aqd_timer(); // Must clear timers.
#endif
  // Any debug logging masks
  //addDebugLogMask(IAQT_LOG);
  //addDebugLogMask(ONET_LOG);
  //addDebugLogMask(ALLB_LOG);
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
#ifndef AQ_MANAGER
  setlogmask(LOG_UPTO(LOG_NOTICE));
#endif

  if (getuid() != 0)
  {
    //LOG(AQUA_LOG,LOG_ERR, "%s Can only be run as root\n", argv[0]);
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize the daemon's parameters.
  init_config();
  cfgFile = defaultCfg;

  for (int i = 1; i < argc; i++)
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
      _cmdln_loglevel = LOG_DEBUG_SERIAL;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      _cmdln_loglevel = LOG_DEBUG;
    }
    else if (strcmp(argv[i], "-rsd") == 0)
    {
      _cmdln_debugRS485 = true;
    }
    else if (strcmp(argv[i], "-rsrd") == 0)
    {
      _cmdln_lograwRS485 = true;
    }
    else if (strcmp(argv[i], "-nc") == 0)
    {
      _cmdln_nostartupcheck = true;
    }
  }

  // Set this here, so it doesn;t get reset if the manager restarts the AqualinkD process.
  _aqualink_data.aqManagerActive = false;

  return startup(argv[0], cfgFile);
}

void check_upgrade_log()
{
  FILE *fp;
  size_t len = 0;
  ssize_t read_size;
  char *line = NULL;

  fp = fopen("/tmp/aqualinkd_upgrade.log", "r");
  if (fp == NULL)
  {
    // No upgrade file
    return;
  }

  LOG(AQUA_LOG,LOG_NOTICE, "--- AqualinkD Upgrade log ----\n");
  while ((read_size = getline(&line, &len, fp)) != -1) {
    LOG(AQUA_LOG,LOG_NOTICE, "%s", line);
  }
  LOG(AQUA_LOG,LOG_NOTICE, "--- End AqualinkD Upgrade log ----\n");

  free(line);
  fclose(fp);

  remove("/tmp/aqualinkd_upgrade.log");
  // Need to delete the file here.
}


int startup(char *self, char *cfgFile) 
{
  _self = self;
  _cfgFile = cfgFile;

  AddAQDstatusMask(CHECKING_CONFIG);
  AddAQDstatusMask(NOT_CONNECTED);
  _aqualink_data.updated = true;
  //_aqualink_data.chiller_button == NULL; // HATE having this here, but needs to be null before config.

  sprintf(_aqualink_data.self, basename(self));
  clearDebugLogMask();
  read_config(&_aqualink_data, cfgFile);

  if (_cmdln_loglevel != -1)
    _aqconfig_.log_level = _cmdln_loglevel;

  if (_cmdln_debugRS485)
    _aqconfig_.log_protocol_packets = true;

  if (_cmdln_lograwRS485)
    _aqconfig_.log_raw_bytes = true;
      

#ifdef AQ_MANAGER
  setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, (_aqconfig_.display_warnings_web?_aqualink_data.last_display_message:NULL));
#else
  if (_aqconfig_.display_warnings_web == true)
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, _aqualink_data.last_display_message);
  else
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, NULL);
#endif

  LOG(AQUA_LOG,LOG_NOTICE, "Starting %s v%s !\n", AQUALINKD_NAME, AQUALINKD_VERSION);

  check_upgrade_log();

  check_print_config(&_aqualink_data);
  

  // NSF Below probably should be moved into check_print_config()

  // Sanity check on Device ID's against panel type
  if (isRS_PANEL) {
    if ( (_aqconfig_.device_id >= 0x08 && _aqconfig_.device_id <= 0x0B) || _aqconfig_.device_id == 0x00 ||  _aqconfig_.device_id == 0xFF) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Device ID 0x%02hhx does not match RS panel, Going to search for ID!\n", _aqconfig_.device_id);
      _aqconfig_.device_id = 0x00;
      //return EXIT_FAILURE;
    }
  } else if (isPDA_PANEL) {
    if ( (_aqconfig_.device_id >= 0x60 && _aqconfig_.device_id <= 0x63) || _aqconfig_.device_id == 0x33 ) {
      if ( _aqconfig_.device_id == 0x33 ) {
        LOG(AQUA_LOG,LOG_NOTICE, "Enabeling iAqualink protocol.\n");
        _aqconfig_.enable_iaqualink = true;
      }
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
    if (_aqconfig_.rssa_device_id >= 0x48 && _aqconfig_.rssa_device_id <= 0x4B  /*&& _aqconfig_.rssa_device_id != 0xFF*/) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "RSSA Device ID 0x%02hhx does not match RS panel, please check config!\n", _aqconfig_.rssa_device_id);
      return EXIT_FAILURE;
    }
  }


  if (_aqconfig_.extended_device_id != 0x00) {
    if ( (_aqconfig_.extended_device_id >= 0x30 && _aqconfig_.extended_device_id <= 0x33) || 
         (_aqconfig_.extended_device_id >= 0x40 && _aqconfig_.extended_device_id <= 0x43) /*||
          _aqconfig_.extended_device_id != 0xFF*/ ) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Extended Device ID 0x%02hhx does not match OneTouch or AqualinkTouch ID, please check config!\n", _aqconfig_.extended_device_id);
      return EXIT_FAILURE;
    }
  }



  if (_aqconfig_.deamonize == true)
  {
    char pidfile[256];
    // sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    //sprintf(pidfile, "%s/%s.pid", "/run", basename(argv[0]));
    //sprintf(pidfile, "%s/%s.pid", "/run", basename(self));
    sprintf(pidfile, "%s/%s.pid", "/run", _aqualink_data.self);
    daemonise(pidfile, main_loop);
  }
  else
  {
    main_loop();
  }

  exit(EXIT_SUCCESS);
}

/*
#define MAX_BLOCK_ACK 12
#define MAX_BUSY_ACK  (50 + MAX_BLOCK_ACK)
*/

/* Point of this is to sent ack as quickly as possible, all checks should be done prior to calling this.*/
void caculate_ack_packet(int rs_fd, unsigned char *packet_buffer, emulation_type source) 
{
  unsigned char *cmd;
  int size;

  switch (source) {
    case ALLBUTTON:
      send_extended_ack(rs_fd, (packet_buffer[PKT_CMD]==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_allb_cmd(&_aqualink_data));
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
    case ONETOUCH:
      send_extended_ack(rs_fd, ACK_ONETOUCH, pop_ot_cmd(packet_buffer[PKT_CMD]));
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"OneTouch Emulation type Processed packet in");
    break;
    case IAQTOUCH:
      if (packet_buffer[PKT_CMD] != CMD_IAQ_CTRL_READY)
        send_extended_ack(rs_fd, ACK_IAQ_TOUCH, pop_iaqt_cmd(packet_buffer[PKT_CMD]));
      else {
        size = ref_iaqt_control_cmd(&cmd);
        send_jandy_command(rs_fd, cmd, size);
        rem_iaqt_control_cmd(cmd);
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AquaTouch Emulation type Processed packet in");
    break;
    case IAQUALNK:
      //send_iaqualink_ack(rs_fd, packet_buffer);
      size = get_iaqualink_cmd(packet_buffer[PKT_CMD], &cmd);
      if (size == 2){
        send_extended_ack(rs_fd, cmd[0], cmd[1]);
      } else {
        send_jandy_command(rs_fd, cmd, size);
      }
      remove_iaqualink_cmd();
    break;
#ifdef AQ_PDA
    case AQUAPDA:
      if (_aqconfig_.pda_sleep_mode && pda_shouldSleep()) {
        LOG(PDA_LOG,LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      } else {
        send_extended_ack(rs_fd, ACK_PDA, pop_pda_cmd(&_aqualink_data));
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"PDA Emulation type Processed packet in");
    break;
#endif
    case SIMULATOR:
      if (_aqualink_data.simulator_active == ALLBUTTON) {
        send_extended_ack(rs_fd, (packet_buffer[PKT_CMD]==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else if (_aqualink_data.simulator_active == ONETOUCH) {
        send_extended_ack(rs_fd, ACK_ONETOUCH, pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else if (_aqualink_data.simulator_active == IAQTOUCH) {
        LOG(SIM_LOG,LOG_WARNING, "IAQTOUCH not implimented yet!\n");
      } else if (_aqualink_data.simulator_active == AQUAPDA) {
        send_extended_ack(rs_fd, ACK_PDA, pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else {
        LOG(SIM_LOG,LOG_ERR, "No idea on this protocol (%d), not implimented!!!\n",_aqualink_data.simulator_active);
      }
    break;

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

#define MAX_AUTO_PACKETS 1200

bool auto_configure(unsigned char* packet) {
  // Loop over PROBE packets and store any we can use,
  // once we see the 2nd probe of any ID we fave stored, then the loop is complete, 
  // set ID's and exit true, exit falce to get called again.
/*
  unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
  unsigned char _goodPDAID[] = {0x60, 0x61, 0x62, 0x63}; // PDA Panel only supports one PDA.
  unsigned char _goodONETID[] = {0x40, 0x41, 0x42, 0x43};
  unsigned char _goodIAQTID[] = {0x30, 0x31, 0x32, 0x33};
  unsigned char _goodRSSAID[] = {0x48, 0x49};  // Know there are only 2 good RS SA id's, guess 0x49 is the second.
*/

  static unsigned char firstprobe = 0x00;
  static unsigned char lastID = 0x00;
  static bool seen_iAqualink2 = false;
  static int foundIDs = 0;
  static int packetsReceived=0;

  if (++packetsReceived >= MAX_AUTO_PACKETS ) {
    LOG(AQUA_LOG,LOG_ERR, "Received %d packets, and didn't get a full probe cycle, stoping Auto Configure!\n",packetsReceived);
    return true;
  }

  if ( packet[PKT_CMD] == CMD_PROBE ) {
    LOG(AQUA_LOG,LOG_INFO, "Got Probe on ID 0x%02hhx\n",packet[PKT_DEST]);
    //printf(" *** Got Probe on ID 0x%02hhx\n",packet[PKT_DEST]);
  }

  if (lastID != 0x00 && packet[PKT_DEST] == DEV_MASTER ) { // Can't use got a reply to the late probe.
    lastID = 0x00; 
  } else if (lastID != 0x00 && packet[PKT_DEST] != DEV_MASTER) {
    // We can use last ID.
    // Save the first good ID.
    if (firstprobe == 0x00 && lastID != 0x60) {
      // NOTE IF can't use 0x60 (or PDA ID's) for probe, as they are way too often.
      //printf("*** First Probe 0x%02hhx\n",lastID);
      firstprobe = lastID;
      _aqconfig_.device_id = 0x00;
      _aqconfig_.rssa_device_id = 0x00;
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
      AddAQDstatusMask(AUTOCONFIGURE_ID);
      _aqualink_data.updated = true;
      //AddAQDstatusMask(AUTOCONFIGURE_PANEL); // Not implimented yet.
    }


    if ( (lastID >= 0x08 && lastID <= 0x0B) && 
         (_aqconfig_.device_id == 0x00 || _aqconfig_.device_id == 0xFF) ) {
      _aqconfig_.device_id = lastID;
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused device ID 0x%02hhx\n",lastID);
      foundIDs++;
    } else if ( (lastID >= 0x48 && lastID <= 0x49) && 
                (_aqconfig_.rssa_device_id == 0x00 || _aqconfig_.rssa_device_id == 0xFF) ) {
      _aqconfig_.rssa_device_id = lastID;
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused RSSA ID 0x%02hhx\n",lastID);
      foundIDs++;
    } else if ( (lastID >= 0x40 && lastID <= 0x43) && 
                (_aqconfig_.extended_device_id == 0x00 || _aqconfig_.extended_device_id == 0xFF) ) {
      _aqconfig_.extended_device_id = lastID;
      _aqconfig_.extended_device_id_programming = true;
      // Don't increase  foundIDs as we prefer not to use this one.
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused extended ID 0x%02hhx\n",lastID);
    } else if ( (lastID >= 0x30 && lastID <= 0x33) && 
                  (_aqconfig_.extended_device_id < 0x30 || _aqconfig_.extended_device_id > 0x33)) { //Overide is it's been set to Touch or not set.
      _aqconfig_.extended_device_id = lastID;
      _aqconfig_.extended_device_id_programming = true;
      if (!seen_iAqualink2) {
        _aqconfig_.enable_iaqualink = true;
        _aqconfig_.read_RS485_devmask &= ~ READ_RS485_IAQUALNK; // Remove this mask, as no need since we enabled iaqualink 
      }
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused extended ID 0x%02hhx\n",lastID);
      foundIDs++;
    }
    // Now reset ID
    lastID = 0x00;
  }

  if ( foundIDs >= 3 || (packet[PKT_DEST] == firstprobe && packet[PKT_CMD] == CMD_PROBE) ) {
    // We should have seen one complete probe cycle my now.
    LOG(AQUA_LOG,LOG_NOTICE, "Finished Autoconfigure using device_id=0x%02hhx rssa_device_id=0x%02hhx extended_device_id=0x%02hhx (%s iAqualink2/3)\n",
                              _aqconfig_.device_id,_aqconfig_.rssa_device_id,_aqconfig_.extended_device_id,  _aqconfig_.enable_iaqualink?"Enable":"Disable");
    RemoveAQDstatusMask(AUTOCONFIGURE_ID);
    _aqualink_data.updated = true;
    return true;  // we can exit finally.
  }

  if ( (packet[PKT_CMD] == CMD_PROBE) && (
       (packet[PKT_DEST] >= 0x08 && packet[PKT_DEST] <= 0x0B) ||
       //(packet[PKT_DEST] >= 0x60 && packet[PKT_DEST] <= 0x63) ||
       (packet[PKT_DEST] >= 0x40 && packet[PKT_DEST] <= 0x43) ||
       (packet[PKT_DEST] >= 0x30 && packet[PKT_DEST] <= 0x33) ||
       (packet[PKT_DEST] >= 0x48 && packet[PKT_DEST] <= 0x49) ))
  {
    lastID = packet[PKT_DEST]; // Store the valid ID.
  } else if (lastID != 0x00 && packet[PKT_CMD] != CMD_PROBE &&
            (packet[PKT_DEST] >= 0xA0 && packet[PKT_DEST] <= 0xA3) ) // we get a packet to iAqualink2/3 make sure to turn off
  { // Saw a iAqualink2/3 device, so can't use ID, but set to read device info.
    // LOG Nessage as such
    _aqconfig_.extended_device_id2 = 0x00;
    _aqconfig_.enable_iaqualink = false;
    _aqconfig_.read_RS485_devmask |= READ_RS485_IAQUALNK;
    seen_iAqualink2 = true;
    LOG(AQUA_LOG,LOG_NOTICE, "Saw inuse iAqualink2/3 ID 0x%02hhx, turning off AqualinkD on that ID\n",lastID);
  }
  
  return false;
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
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN+1];
  int i;
  //int delayAckCnt = 0;
  bool got_probe = false;
  bool got_probe_extended = false;
  bool got_probe_rssa = false;
  bool print_once = false;
  int blank_read_reconnect = MAX_ZERO_READ_BEFORE_RECONNECT_BLOCKING; // Will get reset if non blocking
  bool auto_config_complete = true;

  //_aqualink_data.panelstatus = STARTING;
  AddAQDstatusMask(CHECKING_CONFIG);
  //_aqualink_data.panel_rev = NULL;
  //_aqualink_data.panel_cpu = NULL;
  //_aqualink_data.panel_string = NULL;
  _aqualink_data.updated = true;
  sprintf(_aqualink_data.last_display_message, "%s", "Connecting to Control Panel");
  _aqualink_data.is_display_message_programming = false;
  //_aqualink_data.simulate_panel = false;
  _aqualink_data.active_thread.thread_id = 0;
  _aqualink_data.air_temp = TEMP_UNKNOWN;
  _aqualink_data.pool_temp = TEMP_UNKNOWN;
  _aqualink_data.spa_temp = TEMP_UNKNOWN;
  _aqualink_data.frz_protect_set_point = TEMP_UNKNOWN;
  _aqualink_data.pool_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.spa_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.chiller_set_point = TEMP_UNKNOWN;
  //_aqualink_data.chiller_state = LED_S_UNKNOWN;
  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.swg_percent = TEMP_UNKNOWN;
  _aqualink_data.swg_ppm = TEMP_UNKNOWN;
  _aqualink_data.ar_swg_device_status = SWG_STATUS_UNKNOWN;
  _aqualink_data.heater_err_status = NUL; // 0x00 is no error
  _aqualink_data.swg_led_state = LED_S_UNKNOWN;
  _aqualink_data.swg_delayed_percent = TEMP_UNKNOWN;
  _aqualink_data.temp_units = UNKNOWN;
  _aqualink_data.service_mode_state = OFF;
  _aqualink_data.frz_protect_state = OFF;
  _aqualink_data.battery = OK;
  _aqualink_data.open_websockets = 0;
  _aqualink_data.ph = TEMP_UNKNOWN;
  _aqualink_data.orp = TEMP_UNKNOWN;
  _aqualink_data.simulator_id = NUL;
  _aqualink_data.simulator_active = SIM_NONE;
  _aqualink_data.boost_duration = 0;
  _aqualink_data.boost = false;
  

  pthread_mutex_init(&_aqualink_data.active_thread.thread_mutex, NULL);
  pthread_cond_init(&_aqualink_data.active_thread.thread_cond, NULL);

  //for (i=0; i < MAX_PUMPS; i++) {
  for (i=0; i < _aqualink_data.num_pumps; i++) {
    _aqualink_data.pumps[i].rpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].gpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].watts = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].mode = TEMP_UNKNOWN;
    //_aqualink_data.pumps[i].driveState = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].status = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].pStatus = PS_OFF;
    _aqualink_data.pumps[i].pressureCurve = TEMP_UNKNOWN;

    if (_aqualink_data.pumps[i].maxSpeed <= 0) {
      _aqualink_data.pumps[i].maxSpeed = (_aqualink_data.pumps[i].pumpType==VFPUMP?PUMP_GPM_MAX:PUMP_RPM_MAX);
    }
    if (_aqualink_data.pumps[i].minSpeed <= 0) {
      _aqualink_data.pumps[i].minSpeed = (_aqualink_data.pumps[i].pumpType==VFPUMP?PUMP_GPM_MIN:PUMP_RPM_MIN);
    }

    //printf("arrayindex=%d, pump=%d, min=%d, max=%d\n",i,_aqualink_data.pumps[i].pumpIndex, _aqualink_data.pumps[i].minSpeed ,_aqualink_data.pumps[i].maxSpeed);
  }

  for (i=0; i < MAX_LIGHTS; i++) {
     //_aqualink_data.lights[i].currentValue = TEMP_UNKNOWN;
     _aqualink_data.lights[i].currentValue = 0;
     _aqualink_data.lights[i].RSSDstate = OFF;
  }

  for (i=0; i < _aqualink_data.num_sensors; i++) {
    _aqualink_data.sensors[i].value = TEMP_UNKNOWN;
  }

  if (ENABLE_SWG) {
    //_aqualink_data.ar_swg_device_status = SWG_STATUS_OFF;
    _aqualink_data.swg_led_state = OFF;
    _aqualink_data.swg_percent = 0;
    _aqualink_data.swg_ppm = 0;
  }

  if (ENABLE_CHEM_FEEDER) {
    _aqualink_data.ph = 0;
    _aqualink_data.orp = 0;
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGQUIT, intHandler);
  signal(SIGRESTART, intHandler);
  signal(SIGRUPGRADE, intHandler);

  if (!start_net_services(&_aqualink_data))
  {
    LOG(AQUA_LOG,LOG_ERR, "Can not start webserver on port %s.\n", _aqconfig_.socket_port);
    exit(EXIT_FAILURE);
  }

  startPacketLogger();

  int blank_read = 0;

  rs_fd = init_serial_port(_aqconfig_.serial_port);

  /*
  if (rs_fd == -1) {
    LOG(AQUA_LOG,LOG_ERR, "Error Aqualink setting serial port: %s\n", _aqconfig_.serial_port);
    //_aqualink_data.panelstatus = SERIAL_ERROR;
    AddAQDstatusMask(ERROR_SERIAL);
    _aqualink_data.updated = true;
#ifndef AQ_CONTAINER    
    exit(EXIT_FAILURE);
#endif 
  } else {
    //AddAQDstatusMask(CHECKING_CONFIG);
  }
*/
  if (is_valid_port(rs_fd)) {
    LOG(AQUA_LOG,LOG_NOTICE, "Listening to Aqualink RS8 on serial port: %s\n", _aqconfig_.serial_port);
  } else {
    LOG(AQUA_LOG,LOG_ERR, "Error Aqualink bad serial port: %s\n", _aqconfig_.serial_port);
    AddAQDstatusMask(ERROR_SERIAL);
  }

  if (!serial_blockingmode())
    blank_read_reconnect = MAX_ZERO_READ_BEFORE_RECONNECT_NONBLOCKING;

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

  // Set probes to true for any device we are not searching for.
   
  RemoveAQDstatusMask(CHECKING_CONFIG);
  _aqualink_data.updated = true;
  
  if (_aqconfig_.rssa_device_id == 0x00)
    got_probe_rssa = true;

  if (_aqconfig_.extended_device_id == 0x00)
    got_probe_extended = true;

  if (_aqconfig_.device_id == 0x00) {
    LOG(AQUA_LOG,LOG_WARNING, "Searching for valid ID, please configure `device_id` for faster startup");
  }

  if (_aqconfig_.device_id == 0xFF) {
    LOG(AQUA_LOG,LOG_NOTICE, "Waiting for Control Panel information");
    LOG(AQUA_LOG,LOG_WARNING, "Unsing Auto configure, this will take some time, (make sure to undate aqualinkd configuration to speed up startup!)\n");
    auto_config_complete = false;
    //_aqualink_data.panelstatus = LOOKING_IDS;
    AddAQDstatusMask(AUTOCONFIGURE_ID);
    _aqualink_data.updated = true;
  } else {
    LOG(AQUA_LOG,LOG_NOTICE, "Waiting for Control Panel probe\n");
    //_aqualink_data.panelstatus = CONECTING;
    AddAQDstatusMask(CONNECTING);
    _aqualink_data.updated = true;
  }
  i=0;

  // Loop until we get the probe messages, that means we didn;t start too soon after last shutdown.
  while ( is_valid_port(rs_fd) && (got_probe == false || got_probe_rssa == false || got_probe_extended == false || auto_config_complete == false) && _keepRunning == true && _cmdln_nostartupcheck == false)
  {
    if (blank_read == blank_read_reconnect / 2) {
      LOG(AQUA_LOG,LOG_ERR, "Nothing read on '%s', are you sure that's right?\n",_aqconfig_.serial_port);
//#ifdef AQ_CONTAINER
        // Reset blank reads here, we want to ignore TTY errors in container to keep it running
        blank_read = 1;
//#endif
      if (_aqconfig_.device_id == 0x00) {
        blank_read = 1; // if device id=0x00 it's code for don't exit
      }
      _aqualink_data.updated = true; // Make sure to show erros if ui is up
    } else if (blank_read == blank_read_reconnect*2 ) {
      LOG(AQUA_LOG,LOG_ERR, "I'm done, exiting, please check '%s'\n",_aqconfig_.serial_port);
      stopPacketLogger();
      close_serial_port(rs_fd);
      stop_net_services();
      return;
    }
/*
    if (_aqconfig_.log_raw_RS_bytes)
      packet_length = get_packet_lograw(rs_fd, packet_buffer);
    else
      packet_length = get_packet(rs_fd, packet_buffer);
*/ 
    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length > 0 && auto_config_complete == false) {
      blank_read = 0;
      auto_config_complete = auto_configure(packet_buffer);
      AddAQDstatusMask(AUTOCONFIGURE_ID);
      _aqualink_data.updated = true;
      if (auto_config_complete) {
        //if (_aqconfig_.device_id != 0x00)
          got_probe = true;
        //if (_aqconfig_.rssa_device_id != 0x00)
          got_probe_rssa = true;
        //if (_aqconfig_.extended_device_id != 0x00)
          got_probe_extended = true;
      }
      continue;
    }
    if (packet_length > 0 && _aqconfig_.device_id == 0x00) {
      blank_read = 0;
      AddAQDstatusMask(AUTOCONFIGURE_ID);
      _aqualink_data.updated = true;
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

    else if (packet_length <= 0) {
      blank_read++;
#ifdef AQ_NO_THREAD_NETSERVICE
      if (_aqconfig_.rs_poll_speed < 0)
        LOG(AQUA_LOG,LOG_DEBUG, "Blank RS485 read\n");
      else
        delay(2);
#else
      if (serial_blockingmode())
        LOG(AQUA_LOG,LOG_DEBUG, "Blank RS485 read\n");
#endif
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
        if(!got_probe_extended) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', giving up! (please check config)\n",_aqconfig_.extended_device_id);    
        }
        stopPacketLogger();
        close_serial_port(rs_fd);
        stop_net_services();
        return;
      }
    }
  }
  
  RemoveAQDstatusMask(AUTOCONFIGURE_ID);
  RemoveAQDstatusMask(NOT_CONNECTED);
  AddAQDstatusMask(CONNECTING);
  _aqualink_data.updated = true;

  //At this point we should have correct ID and seen probes on those ID's.
  // Setup the panel
  if (_aqconfig_.device_id <= 0x08 && _aqconfig_.device_id >= 0x0B && _aqconfig_.device_id != 0x60 && _aqconfig_.device_id != 0x33) {
    LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid device_id, can't connect to control panel");
    //_aqualink_data.panelstatus = NO_IDS_ERROR;
    RemoveAQDstatusMask(CONNECTING); // Not sure if we should remove this
    AddAQDstatusMask(ERROR_NO_DEVICE_ID);
    _aqualink_data.updated = true;
  }

  if (_aqconfig_.rssa_device_id >= 0x48 && _aqconfig_.rssa_device_id <= 0x49) {
    addPanelRSserialAdapterInterface();
  }

  if (_aqconfig_.extended_device_id >= 0x40 && _aqconfig_.extended_device_id <= 0x43) {
    addPanelOneTouchInterface();
  } else if (_aqconfig_.extended_device_id >= 0x30 && _aqconfig_.extended_device_id <= 0x33) {
    addPanelIAQTouchInterface();
  }

  // We can only get panel size info from extended ID
  if (_aqconfig_.extended_device_id != 0x00) {
    RemoveAQDstatusMask(AUTOCONFIGURE_PANEL);
    _aqualink_data.updated = true;
  }

  if (_aqconfig_.extended_device_id_programming == true && (isONET_ENABLED || isIAQT_ENABLED) )
  {
    changePanelToExtendedIDProgramming();
  } else if (_aqconfig_.extended_device_id_programming == true) {
    LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid extended_device_id, ignoring value '%s' from cfg\n",CFG_N_extended_device_id_programming);
    _aqconfig_.extended_device_id = 0x00;
    _aqconfig_.extended_device_id_programming = false;
  }

  /*
   *
   *    This is the main loop  
   * 
   * 
   * 
   */

  LOG(AQUA_LOG,LOG_NOTICE, "Starting communication with Control Panel\n");

  // Not the best way to do this, but ok for moment
#ifdef AQ_NO_THREAD_NETSERVICE
  if (_aqconfig_.rs_poll_speed == 0)
    blank_read_reconnect = blank_read_reconnect * 50;
#endif

  int loopnum=0;
  blank_read = 0;
  // OK, Now go into infinate loop
  while (_keepRunning == true)
  {
    //printf("%d ",blank_read);
    while ((rs_fd < 0 || blank_read >= blank_read_reconnect) && _keepRunning == true)
    {
      //printf("rs_fd  =% d\n",rs_fd);
      if (!is_valid_port(rs_fd))
      {
        sleep(1);
        LOG(AQUA_LOG,LOG_ERR, "Bad serial port '%s', are you sure that's right?\n",_aqconfig_.serial_port);   
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        //LOG(AQUA_LOG,LOG_ERR, "Serial port error, Aqualink daemon waiting to connect to master device...\n");    
        _aqualink_data.updated = true;
        AddAQDstatusMask(ERROR_SERIAL);
        //broadcast_aqualinkstate_error(CONNECTION_ERROR);
        broadcast_aqualinkstate_error(getAqualinkDStatusMessage(&_aqualink_data));
        sleep(10);
#ifdef AQ_NO_THREAD_NETSERVICE
        poll_net_services(1000);
        poll_net_services(3000);
#endif
        // broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
      }
      else
      {
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon looks like serial error, resetting.\n");
        _aqualink_data.updated = true;
        AddAQDstatusMask(ERROR_SERIAL);
        //broadcast_aqualinkstate_error(CONNECTION_ERROR);
        broadcast_aqualinkstate_error(getAqualinkDStatusMessage(&_aqualink_data));
        close_serial_port(rs_fd);
        //rs_fd = init_serial_port(_aqconfig_.serial_port);
      }
      rs_fd = init_serial_port(_aqconfig_.serial_port);
      blank_read = 0;
    }

#ifdef AQ_MANAGER
    if (_aqualink_data.run_slogger) {
       LOG(AQUA_LOG,LOG_WARNING, "Starting serial_logger, this will take some time!\n");
       broadcast_aqualinkstate_error(CONNECTION_RUNNING_SLOG);

       if (_aqualink_data.slogger_debug)
         addDebugLogMask(SLOG_LOG);

       serial_logger(rs_fd, _aqconfig_.serial_port, _aqualink_data.slogger_debug?LOG_DEBUG:getSystemLogLevel(), _aqualink_data.slogger_packets, _aqualink_data.slogger_ids);
       _aqualink_data.run_slogger = false;

       if (_aqualink_data.slogger_debug)
         removeDebugLogMask(SLOG_LOG);
    }
#endif

    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length <= 0)
    {
      
      // AQSERR_2SMALL // no reset (-5)
      // AQSERR_2LARGE // no reset (-4)
      // AQSERR_CHKSUM // no reset (-3)
      // AQSERR_TIMEOUT // reset blocking mode (-2)
      // AQSERR_READ // reset (-1)
      
#ifdef AQ_NO_THREAD_NETSERVICE
      if (_aqconfig_.rs_poll_speed < 0) {
#else
      if (serial_blockingmode() && (packet_length == AQSERR_READ || packet_length == AQSERR_TIMEOUT) ) {
#endif
        LOG(AQUA_LOG,LOG_ERR, "Nothing read on blocking serial port\n");
        blank_read = blank_read_reconnect;
      } else if (packet_length == AQSERR_READ) {
        blank_read = blank_read_reconnect;
      } else {
        // In non blocking, so sleep for 2 milliseconds
        delay(NONBLOCKING_SERIAL_DELAY);
      }
      //if (blank_read > max_blank_read) {
      //  LOG(AQUA_LOG,LOG_NOTICE, "Nothing read on serial %d\n",blank_read);
      //  max_blank_read = blank_read;
      //}
      blank_read++;
    }
    else if (packet_length > 0)
    {
      RemoveAQDstatusMask(ERROR_SERIAL);
      RemoveAQDstatusMask(CONNECTING);
      AddAQDstatusMask(CONNECTED);
      _aqualink_data.updated = true;
      DEBUG_TIMER_START(&_rs_packet_timer);

      blank_read = 0;
      //changed = false;

      if (_aqualink_data.simulator_active != SIM_NONE) {
        // Check if we have a valid connection
        if ( _aqualink_data.simulator_id != NUL && packet_buffer[PKT_DEST] == _aqualink_data.simulator_id) {
          // Action comand and Send to web
          processSimulatorPacket(packet_buffer, packet_length, &_aqualink_data);
          caculate_ack_packet(rs_fd, packet_buffer, SIMULATOR);
          DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Simulator Emulation Processed packet in");
        }
        else if ( _aqualink_data.simulator_id == NUL   
                  && packet_buffer[PKT_CMD] == CMD_PROBE 
                  && packet_buffer[PKT_DEST] != _aqconfig_.device_id // Check no conflicting id's
                  && packet_buffer[PKT_DEST] != _aqconfig_.extended_device_id // Check no conflicting id's
                  ) 
        {
          if (is_simulator_packet(&_aqualink_data, packet_buffer, packet_length)) {
            _aqualink_data.simulator_id = packet_buffer[PKT_DEST];
            // reply to probe
            LOG(SIM_LOG,LOG_NOTICE, "Got probe on '0x%02hhx', using for simulator ID\n",packet_buffer[PKT_DEST]);
            processSimulatorPacket(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, SIMULATOR);
          } else {
            LOG(SIM_LOG,LOG_INFO, "Got probe on '0x%02hhx' Still waiting for valid simulator probe\n",packet_buffer[PKT_DEST]);
          }
          DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Simulator Emulation Processed packet in");
        }
      }

      // Process and packets of devices we are acting as
      if (packet_length > 0 && getProtocolType(packet_buffer) == JANDY && packet_buffer[PKT_DEST] != 0x00 &&
          (packet_buffer[PKT_DEST] == _aqconfig_.device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.rssa_device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id2
           ))
      {
        AddAQDstatusMask(CONNECTED);
        switch(getJandyDeviceType(packet_buffer[PKT_DEST])){
          case ALLBUTTON:
            _aqualink_data.updated = process_allbutton_packet(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, ALLBUTTON);
          break;
          case RSSADAPTER:
            _aqualink_data.updated = process_rssadapter_packet(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, RSSADAPTER);    
          break;
          case IAQTOUCH:
            _aqualink_data.updated = process_iaqtouch_packet(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, IAQTOUCH);
          break;
          case ONETOUCH:
            _aqualink_data.updated = process_onetouch_packet(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, ONETOUCH);
          break;
          case AQUAPDA:
            _aqualink_data.updated = process_pda_packet(packet_buffer, packet_length);
            caculate_ack_packet(rs_fd, packet_buffer, AQUAPDA);
          break;
          case IAQUALNK:
            _aqualink_data.updated = process_iaqualink_packet(packet_buffer, packet_length, &_aqualink_data);
            caculate_ack_packet(rs_fd, packet_buffer, IAQUALNK);
          break;
          default:
          break;
        }
#ifdef AQ_TM_DEBUG
        char message[128];
        sprintf(message,"%s Emulation Processed packet in",getJandyDeviceName(getJandyDeviceType(packet_buffer[PKT_DEST])));
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,message);
#endif
      }
      // Process any packets to readonly devices.
      else if (packet_length > 0 && _aqconfig_.read_RS485_devmask > 0)
      {
        if (getProtocolType(packet_buffer) == JANDY)
        {
          _aqualink_data.updated = processJandyPacket(packet_buffer, packet_length, &_aqualink_data);
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

#ifdef AQ_NO_THREAD_NETSERVICE
      if (_aqualink_data.updated) {
        broadcast_aqualinkstate();
      }
#endif
    }

#ifdef AQ_NO_THREAD_NETSERVICE
    poll_net_services(packet_length>0?0:_aqconfig_.rs_poll_speed); // Don;t wait if we read something.
#endif
   // NSF might want to wait if we are on a non blocking serial port.

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

    if ( _aqualink_data.num_sensors > 0 && ++loopnum >= 200 ) {
      loopnum=0;
      for (int i=0; i < _aqualink_data.num_sensors; i++) {
        if (read_sensor(&_aqualink_data.sensors[i]) ) {
          _aqualink_data.updated = true;
        }
      }
    }    

    //tcdrain(rs_fd); // Make sure buffer has been sent.
    //delay(10);
  }
  
  //if (_aqconfig_.debug_RSProtocol_packets) stopPacketLogger();
  stopPacketLogger();

  if (! _restart) { // Stop network if we are not restarting
     stop_net_services();
  }

  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  //mg_mgr_free(&mgr);

  if (! _restart) {
  // NSF need to run through config memory and clean up.
    LOG(AQUA_LOG,LOG_NOTICE, "Exit!\n");
    exit(EXIT_FAILURE);
  } else {
    LOG(AQUA_LOG,LOG_WARNING, "Waiting for process to fininish!\n");
    delay(5 * 1000);
    LOG(AQUA_LOG,LOG_WARNING, "Restarting!\n");
    _keepRunning = true;
    _restart = false;
    startup(_self, _cfgFile);
  }

}

/*

void debugtestePump()
{
  LOG(DJAN_LOG, LOG_INFO, "Jandy Pump code check\n");

  unsigned char toPumpWatts[] = {0x10,0x02,0x78,0x45,0x00,0x05,0xd4,0x10,0x03};
  unsigned char fromPumpWatts[] = {0x10,0x02,0x00,0x1f,0x45,0x00,0x05,0x1d,0x05,0x9d,0x10,0x03};
                                 

  unsigned char toPumpRPM[] = {0x10,0x02,0x78,0x44,0x00,0x60,0x27,0x55,0x10,0x03};
  unsigned char fromPumpRPM[] = {0x10,0x02,0x00,0x1f,0x44,0x00,0x60,0x27,0x00,0xfc,0x10,0x03};

  processJandyPacket(toPumpWatts, 8, &_aqualink_data);
  processJandyPacket(fromPumpWatts, 11, &_aqualink_data);

  processJandyPacket(toPumpRPM, 8, &_aqualink_data);
  processJandyPacket(fromPumpRPM, 11, &_aqualink_data);
}
*/


