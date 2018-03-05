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


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <termios.h>
#include <signal.h>

#define _GNU_SOURCE 1
#define __USE_XOPEN 1
#include <time.h>      // Need GNU_SOURCE & XOPEN defined for strptime

#include "mongoose.h"
#include "aqualink.h"
#include "utils.h"
#include "config.h"
#include "aq_serial.h"
#include "init_buttons.h"
#include "aq_programmer.h"
#include "net_services.h"
#include "version.h"


#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;
static struct aqconfig _config_parameters;
static struct aqualinkdata _aqualink_data;


void main_loop();

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
}


void processLEDstate()
{

  int i=0;
  int byte;
  int bit;
  
  for (byte=0;byte<5;byte++)
  {
    for (bit=0; bit<8; bit+=2)
    {
      if ( ((_aqualink_data.raw_status[byte] >> (bit+1) ) & 1) == 1 )
      _aqualink_data.aqualinkleds[i].state = FLASH;
      else if ( ((_aqualink_data.raw_status[byte] >> bit) & 1) == 1 )
      _aqualink_data.aqualinkleds[i].state = ON;
      else
      _aqualink_data.aqualinkleds[i].state = OFF;
      
      //logMessage(LOG_DEBUG,"Led %d state %d",i+1,_aqualink_data.aqualinkleds[i].state);
      i++;
    }
  }
  // Reset enabled state for heaters, as they take 2 led states
  if ( _aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX-1].state == OFF && _aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX].state == ON)
  _aqualink_data.aqualinkleds[POOL_HTR_LED_INDEX-1].state = ENABLE;\
  
  if ( _aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX-1].state == OFF && _aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX].state == ON)
  _aqualink_data.aqualinkleds[SPA_HTR_LED_INDEX-1].state = ENABLE;
  
  if ( _aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX-1].state == OFF && _aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX].state == ON)
  _aqualink_data.aqualinkleds[SOLAR_HTR_LED_INDEX-1].state = ENABLE;
}


bool checkAqualinkTime()
{
  static time_t last_checked;
  time_t now = time(0);   // get time now
  int time_difference;
  struct tm aq_tm;
  time_t aqualink_time;
  
  time_difference = (int)difftime(now, last_checked);
  if (time_difference < TIME_CHECK_INTERVAL) {
    logMessage(LOG_DEBUG, "time not checked, will check in %d seconds", TIME_CHECK_INTERVAL - time_difference);
    return true;
  } else {
    last_checked = now;
    //return false;
  }
  
  char datestr[DATE_STRING_LEN];
  strcpy(&datestr[0], _aqualink_data.date);
  strcpy(&datestr[12], " ");
  strcpy(&datestr[13], _aqualink_data.time);
  
  if (strptime(datestr, "%m/%d/%y %a %I:%M %p", &aq_tm) == NULL) {
    logMessage(LOG_ERR, "Could not convert RS time string '%s'", datestr);
    last_checked = (time_t)NULL;
    return true;
  }

  aq_tm.tm_isdst = -1;  // Force mktime to use local timezone
  aqualink_time = mktime(&aq_tm);
  time_difference = (int)difftime(now, aqualink_time);
  
  logMessage(LOG_INFO, "Aqualink time is off by %d seconds...\n",  time_difference);
  
  if(abs(time_difference) <= ACCEPTABLE_TIME_DIFF) {
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
  aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, &_aqualink_data);
  aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, &_aqualink_data);
  //aq_programmer(AQ_GET_PROGRAMS, NULL, &_aqualink_data); // only displays to log at present, also seems to confuse getting set_points
}

void processMessage(char *message)
{
  char *msg;
  static bool _initWithRS = false;
  static bool _gotREV = false;
  // NSF replace message with msg 
  msg = cleanwhitespace(message);
  _aqualink_data.last_message = msg;
  
  //aqualink_strcpy(_aqualink_data.message, msg);
  
  logMessage(LOG_DEBUG, "RS Message :- '%s'\n",msg);
  
  // Check long messages in this if/elseif block first, as some messages are similar.  
  // ie "POOL TEMP" and "POOL TEMP IS SET TO"  so want correct match first.
  // 
  if(stristr(message, LNG_MSG_BATTERY_LOW) != NULL) {
    _aqualink_data.battery = LOW;
  }
  else if(stristr(message, LNG_MSG_POOL_TEMP_SET) != NULL) {
    //logMessage(LOG_DEBUG, "pool htr long message: %s", &message[20]);
    _aqualink_data.pool_htr_set_point = atoi(message+20);
  }
  else if(stristr(message, LNG_MSG_SPA_TEMP_SET) != NULL) {
    //logMessage(LOG_DEBUG, "spa htr long message: %s", &message[19]);
    _aqualink_data.spa_htr_set_point = atoi(message+19);
  }
  else if(stristr(message, LNG_MSG_FREEZE_PROTECTION_SET) != NULL) {
    //logMessage(LOG_DEBUG, "frz protect long message: %s", &message[28]);
    _aqualink_data.frz_protect_set_point = atoi(message+28);
  }
  else if(strncasecmp(msg, MSG_AIR_TEMP, MSG_AIR_TEMP_LEN) == 0) {
    _aqualink_data.air_temp = atoi(msg+8);
    if (msg[strlen(msg)-1] == 'F')
    _aqualink_data.temp_units = FAHRENHEIT;
    else if (msg[strlen(msg)-1] == 'C')
    _aqualink_data.temp_units = CELSIUS;
    else
    _aqualink_data.temp_units = UNKNOWN;
  }
  else if(strncasecmp(message, MSG_POOL_TEMP, MSG_POOL_TEMP_LEN) == 0) {
    _aqualink_data.pool_temp = atoi(message+9);
  }
  else if(strncasecmp(message, MSG_SPA_TEMP, MSG_SPA_TEMP_LEN) == 0) {
    _aqualink_data.spa_temp = atoi(message+8);
  }
  else if(msg[2] == '/' && msg[5] == '/' && msg[8] == ' ') {// date in format '08/29/16 MON'
    strcpy(_aqualink_data.date, msg);
  }
  else if(strncasecmp(message, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0) {
    _aqualink_data.swg_percent = atoi(message+MSG_SWG_PCT_LEN);
    logMessage(LOG_DEBUG, "Stored SWG Percent as %d\n", _aqualink_data.swg_percent);
  }
  else if(strncasecmp(message, MSG_SWG_PPM, MSG_SWG_PPM_LEN) == 0) {
    _aqualink_data.swg_ppm = atoi(message+MSG_SWG_PPM_LEN);
    logMessage(LOG_DEBUG, "Stored SWG PPM as %d\n", _aqualink_data.swg_ppm);
  }
  else if( (msg[1] == ':' || msg[2] == ':') && msg[strlen(msg)-1] == 'M') { // time in format '9:45 AM'
    strcpy(_aqualink_data.time, msg);
    // Setting time takes a long time, so don't try until we have all other programmed data.
    if ( (_initWithRS == true) && strlen(_aqualink_data.date) > 1 && checkAqualinkTime() != true ) {
      logMessage(LOG_NOTICE, "RS time is NOT accurate '%s %s', re-setting on controller!\n", _aqualink_data.time, _aqualink_data.date);
      aq_programmer(AQ_SET_TIME, NULL, &_aqualink_data);
    } else {
      logMessage(LOG_DEBUG, "RS time is accurate '%s %s'\n", _aqualink_data.time, _aqualink_data.date);
    }
    // If we get a time message before REV, the controller didn't see us as we started too quickly.
    if ( _gotREV == false ) {
      logMessage(LOG_NOTICE, "Getting control panel information\n",msg);
      aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, &_aqualink_data);
      _gotREV = true; // Force it to true just incase we don't understand the model#
    }
  }
  else if(strstr(msg, " REV ") != NULL) {  // '8157 REV MMM'
    // A master firmware revision message.
    strcpy(_aqualink_data.version, msg);
    _gotREV = true;
    logMessage(LOG_NOTICE, "Control Panel %s\n",msg);
    if ( _initWithRS == false) {
      queueGetProgramData();
      _initWithRS = true;
    }
  }
  else if(stristr(msg, " TURNS ON") != NULL) {
    logMessage(LOG_NOTICE, "Program data '%s'\n",msg);
  }
  else {
    logMessage(LOG_DEBUG, "Ignoring '%s'\n",msg);
  }
  
  // We processed the next message, kick any threads waiting on the message.
  kick_aq_program_thread(&_aqualink_data);
}

bool process_packet(unsigned char* packet, int length)
{
  bool rtn = false;
  static unsigned char last_packet[AQ_MAXPKTLEN];
  static char message[AQ_MSGLONGLEN+1];
  static int processing_long_msg = 0;
  
  // Check packet against last check if different.
  if (memcmp(packet, last_packet, length) == 0) {
    //logMessage(LOG_DEBUG, "RS Received duplicate, ignoring.\n",length);
    return rtn;
  } else {
    memcpy(last_packet, packet, length);
    rtn = true;
  }
  
  if (processing_long_msg > 0 && packet[PKT_CMD] != CMD_MSG_LONG) {
    processing_long_msg = 0;
    //logMessage(LOG_ERR, "RS failed to receive complete long message, received '%s'\n",message);
    //logMessage(LOG_DEBUG, "RS Finished receiving of MSG_LONG '%s'\n",message);
    processMessage(message);
  }
  
  switch (packet[PKT_CMD]) {
  case CMD_ACK:
    logMessage(LOG_DEBUG, "RS Received ACK length %d.\n",length);
    break;
  case CMD_STATUS:
    logMessage(LOG_DEBUG, "RS Received STATUS length %d.\n",length);
    memcpy(_aqualink_data.raw_status, packet+4, AQ_PSTLEN);
    processLEDstate(); 
    if (_aqualink_data.aqbuttons[PUMP_INDEX].led->state == OFF ) {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
    } else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == OFF ) {
      _aqualink_data.spa_temp = TEMP_UNKNOWN;
    } else if (_aqualink_data.aqbuttons[SPA_INDEX].led->state == ON ) {
      _aqualink_data.pool_temp = TEMP_UNKNOWN;
    }
    //logMessage(LOG_DEBUG, "RS Pool temp set to %d | Spa temp %d\n",_aqualink_data.pool_temp,_aqualink_data.spa_temp);
    break;
  case CMD_MSG:
    memset(message, 0, AQ_MSGLONGLEN+1);
    strncpy(message, (char*)packet+PKT_DATA+1, AQ_MSGLEN);
    
    if (packet[PKT_DATA] == 1) // Start of long message, get them all before processing
    break;

    processMessage(message);
    break;
  case CMD_MSG_LONG:
    // First in sequence is normal message.
    processing_long_msg++;
    strncpy(&message[processing_long_msg*AQ_MSGLEN], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
    if (processing_long_msg == 3) {
      //logMessage(LOG_DEBUG, "RS Finished receiving of MSG_LONG '%s'\n",message);
      processMessage(message);
      processing_long_msg=0;
    }
    break;
  case CMD_PROBE:
    logMessage(LOG_DEBUG, "RS Received PROBE length %d.\n",length);
    //logMessage(LOG_INFO, "Synch'ing with Aqualink master device...\n");
    rtn = false;
    break;
  default:
    logMessage(LOG_INFO, "RS Received unknown packet.\n",length);
    rtn = false;
    break;
  }
  
  return rtn;
}

void action_delayed_request()
{
  char sval[10];
  snprintf(sval, 9, "%d", _aqualink_data.unactioned.value);

  if (_aqualink_data.unactioned.type == POOL_HTR_SETOINT) {
    aq_programmer(AQ_SET_POOL_HEATER_TEMP, sval, &_aqualink_data);
    logMessage(LOG_NOTICE, "Setting pool heater setpoint to %d\n",_aqualink_data.unactioned.value);
  } else if (_aqualink_data.unactioned.type == SPA_HTR_SETOINT) {
    aq_programmer(AQ_SET_SPA_HEATER_TEMP, sval, &_aqualink_data);
    logMessage(LOG_NOTICE, "Setting spa heater setpoint to %d\n",_aqualink_data.unactioned.value);
  } else if (_aqualink_data.unactioned.type == FREEZE_SETPOINT) {
    aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, sval, &_aqualink_data);
    logMessage(LOG_NOTICE, "Setting freeze protect to %d\n",_aqualink_data.unactioned.value);
  }

  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.unactioned.value = -1;
  _aqualink_data.unactioned.requested = 0;
}

int main(int argc, char *argv[]) {
  // main_loop ();

  int i;
  char *cfgFile = DEFAULT_CONFIG_FILE;

  // struct lws_context_creation_info info;
  // Log only NOTICE messages and above. Debug and info messages
  // will not be logged to syslog.
  setlogmask(LOG_UPTO(LOG_NOTICE));

  if (getuid() != 0) {
    //logMessage(LOG_ERR, "%s Can only be run as root\n", argv[0]);
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize the daemon's parameters.
  init_parameters(&_config_parameters);

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      _config_parameters.deamonize = false;
    } else if (strcmp(argv[i], "-c") == 0) {
      cfgFile = argv[++i];
    }
  }

  initButtons(&_aqualink_data);

  readCfg(&_config_parameters, &_aqualink_data, cfgFile);

  setLoggingPrms(_config_parameters.log_level, _config_parameters.deamonize, _config_parameters.log_file);

  logMessage(LOG_NOTICE, "%s v%s\n", AQUALINKD_NAME, AQUALINKD_VERSION);

  logMessage(LOG_NOTICE, "Config log_level         = %d\n", _config_parameters.log_level);
  logMessage(LOG_NOTICE, "Config socket_port       = %s\n", _config_parameters.socket_port);
  logMessage(LOG_NOTICE, "Config serial_port       = %s\n", _config_parameters.serial_port);
  logMessage(LOG_NOTICE, "Config web_directory     = %s\n", _config_parameters.web_directory);
  logMessage(LOG_NOTICE, "Config device_id         = 0x%02hhx\n", _config_parameters.device_id);
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
  /* removed until domoticz has a better virtual thermostat
  logMessage(LOG_NOTICE, "Config idx pool thermostat = %d\n", _config_parameters.dzidx_pool_thermostat);
  logMessage(LOG_NOTICE, "Config idx spa thermostat  = %d\n", _config_parameters.dzidx_spa_thermostat);
  */
#endif // MG_DISABLE_MQTT
  logMessage(LOG_NOTICE, "Config deamonize         = %s\n", bool2text(_config_parameters.deamonize));
  logMessage(LOG_NOTICE, "Config log_file          = %s\n", _config_parameters.log_file);
  logMessage(LOG_NOTICE, "Config light_pgm_mode    = %.2f\n", _config_parameters.light_programming_mode);
  // logMessage (LOG_NOTICE, "Config serial_port = %s\n", config_parameters->serial_port);

  

  for (i=0; i < TOTAL_BUTONS; i++) {
    logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | dzidx %d\n", _aqualink_data.aqbuttons[i].name, _aqualink_data.aqbuttons[i].label , _aqualink_data.aqbuttons[i].dz_idx);
    //logMessage(LOG_NOTICE, "Button %d\n", i+1, _aqualink_data.aqbuttons[i].label , _aqualink_data.aqbuttons[i].dz_idx);
  }

  if (_config_parameters.deamonize == true) {
    char pidfile[256];
    // sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    sprintf(pidfile, "%s/%s.pid", "/run", basename(argv[0]));
    daemonise(pidfile, main_loop);
  } else {
    main_loop();
  }

  exit(EXIT_SUCCESS);
}

void main_loop() {
  struct mg_mgr mgr;
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];

  // NSF need to find a better place to init this.
  _aqualink_data.aq_command = 0x00;
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


  if (!start_net_services(&mgr, &_aqualink_data, &_config_parameters)) {
    logMessage(LOG_ERR, "Can not start webserver on port %s.\n", _config_parameters.socket_port);
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  int blank_read = 0;
  rs_fd = init_serial_port(_config_parameters.serial_port);
  logMessage(LOG_NOTICE, "Listening to Aqualink RS8 on serial port: %s\n", _config_parameters.serial_port);

  while (_keepRunning == true) {
    while ((rs_fd < 0 || blank_read >= MAX_ZERO_READ_BEFORE_RECONNECT) && _keepRunning == true) {
      if (rs_fd < 0) {
        // sleep(1);
        logMessage(LOG_ERR, "Aqualink daemon attempting to connect to master device...\n");
        broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
        mg_mgr_poll(&mgr, 1000); // Sevice messages
        mg_mgr_poll(&mgr, 3000); // should donothing for 3 seconds.
        // broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
      } else {
        logMessage(LOG_ERR, "Aqualink daemon looks like serial error, resetting.\n");
      }
      rs_fd = init_serial_port(_config_parameters.serial_port);
      blank_read = 0;
    }

    packet_length = get_packet(rs_fd, packet_buffer);
    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_ERR, "Bad packet length, reconnecting\n");
      blank_read = MAX_ZERO_READ_BEFORE_RECONNECT;
    } else if (packet_length == 0) {
      logMessage(LOG_DEBUG_SERIAL, "Nothing read on serial\n");
      blank_read++;
    } else if (packet_length > 0) {
      blank_read = 0;

      if (packet_length > 0 && packet_buffer[PKT_DEST] == _config_parameters.device_id) {
        send_ack(rs_fd, _aqualink_data.aq_command);
        _aqualink_data.aq_command = NUL;

        // Process the packet. This includes deriving general status, and identifying
        // warnings and errors.  If something changed, notify any listeners
        if (process_packet(packet_buffer, packet_length) != false) {
          broadcast_aqualinkstate(mgr.active_connections);
        }
      } else if (packet_length > 0) {
        // printf("packet not for us %02x\n",packet_buffer[PKT_DEST]);
        logMessage(LOG_DEBUG_SERIAL, "Received Packet, but not for us, ID is for %02x\n",packet_buffer[PKT_DEST]);
      }
    }
    mg_mgr_poll(&mgr, 0);

    // Any unactioned commands
    if (_aqualink_data.unactioned.type != NO_ACTION) {
      time_t now;
      time(&now);
      if (difftime(now, _aqualink_data.unactioned.requested) > 2){
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

  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  mg_mgr_free(&mgr);

  // NSF need to run through config memory and clean up.

  logMessage(LOG_NOTICE, "Exit!\n");
  exit(EXIT_FAILURE);
}

