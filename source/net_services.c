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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>

#ifdef AQ_MANAGER
#include <systemd/sd-journal.h>
#endif

#include "mongoose.h"

#include "aqualink.h"
#include "config.h"
#include "aq_programmer.h"
#include "utils.h"
#include "net_services.h"
#include "json_messages.h"
#include "domoticz.h"
#include "aq_mqtt.h"
#include "devices_jandy.h"
#include "web_config.h"
#include "debug_timer.h"
#include "serialadapter.h"
#include "aq_timer.h"
#include "aq_scheduler.h"
#include "rs_msg_utils.h"
#include "simulator.h"
#include "hassio.h"
#include "version.h"

#ifdef AQ_PDA
#include "pda.h"
#endif

/*
#if defined AQ_DEBUG || defined AQ_TM_DEBUG
  #include "timespec_subtract.h"
  //#define AQ_TM_DEBUG
#endif
*/

//static struct aqconfig *_aqconfig_;
static struct aqualinkdata *_aqualink_data;
//static char *_web_root;

static pthread_t _net_thread_id = 0;
static bool _keepNetServicesRunning = false;
static struct mg_mgr _mgr;
static int _mqtt_exit_flag = false;

#ifndef MG_DISABLE_MQTT
void start_mqtt(struct mg_mgr *mgr);
static struct aqualinkdata _last_mqtt_aqualinkdata;
void mqtt_broadcast_aqualinkstate(struct mg_connection *nc);
#endif

void reset_last_mqtt_status();

//static const char *s_http_port = "8080";
static struct mg_serve_http_opts _http_server_opts;


#ifdef AQ_NO_THREAD_NETSERVICE
  static sig_atomic_t s_signal_received = 0;
#endif

static void net_signal_handler(int sig_num) {
  //printf("** net_signal_handler **\n");
#ifdef AQ_NO_THREAD_NETSERVICE
  if (!_aqconfig_.thread_netservices) {
    signal(sig_num, net_signal_handler);  // Reinstantiate signal handler to aqualinkd.c
    s_signal_received = sig_num;
  } else {  
    intHandler(sig_num); // Force signal handler to aqualinkd.c
  }
#else
  intHandler(sig_num); // Force signal handler to aqualinkd.c
#endif
}


static int is_websocket(const struct mg_connection *nc) {
  //return nc->flags & MG_F_IS_WEBSOCKET && !(nc->flags & MG_F_USER_2); // WS only, not WS simulator
  return nc->flags & MG_F_IS_WEBSOCKET;
}
static void set_websocket_simulator(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_2; 
}
static int is_websocket_simulator(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_2;
}
static void set_websocket_aqmanager(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_3; 
}
static int is_websocket_aqmanager(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_3;
}
static int is_mqtt(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_1;
}
static void set_mqtt(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_1; 
}

static void ws_send(struct mg_connection *nc, char *msg)
{
  int size = strlen(msg);
  
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg, size);
  
  //LOG(NET_LOG,LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

void _broadcast_aqualinkstate_error(struct mg_connection *nc, char *msg) 
{
  struct mg_connection *c;
  char data[JSON_STATUS_SIZE];
  build_aqualink_error_status_JSON(data, JSON_STATUS_SIZE, msg);

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c))
      ws_send(c, data);
  }
  // Maybe enhacment in future to sent error messages to MQTT
}


void _broadcast_simulator_message(struct mg_connection *nc) {
  struct mg_connection *c;
  char data[JSON_SIMULATOR_SIZE];

  build_aqualink_simulator_packet_JSON(_aqualink_data, data, JSON_SIMULATOR_SIZE);

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c) && is_websocket_simulator(c)) {
      ws_send(c, data);
    }
  }

  //LOG(NET_LOG,LOG_DEBUG, "Sent to simulator '%s'\n",data);

  _aqualink_data->simulator_packet_updated = false;
}

#ifdef AQ_MANAGER

#define WS_LOG_LENGTH 200
// Send log message to any aqManager websocket.
void ws_send_logmsg(struct mg_connection *nc, char *msg) {
  struct mg_connection *c;

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c) && is_websocket_aqmanager(c)) {
      ws_send(c, msg);
    }
  }
}

sd_journal *open_journal() {
  sd_journal *journal;
  char filter[51];

#ifndef AQ_CONTAINER
  // Below works for local
  if (sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY) < 0)
#else
  // Container doesn't have local systemd_journal so use hosts through mapped filesystem
  if (sd_journal_open_directory(&journal, "/var/log/journal", SD_JOURNAL_SYSTEM) < 0)
#endif
  {
    LOGSystemError(errno, NET_LOG, "Failed to open journal");
    return journal;
  }
  snprintf(filter, 50, "SYSLOG_IDENTIFIER=%s",_aqualink_data->self );
  if (sd_journal_add_match(journal, filter, 0) < 0)
  {
    LOGSystemError(errno, NET_LOG, "Failed to set journal syslog filter");
    sd_journal_close(journal);
    return journal;
  }
  /* Docker wll also have problem with this
  // Daemon will change PID after printing startup message, so don't filter on current PID
  if (_aqconfig_.deamonize != true) {
    snprintf(filter, 50, "_PID=%d",getpid());
    if (sd_journal_add_match(journal, filter, 0) < 0)
    {
      LOGSystemError(errno, NET_LOG, "Failed to set journal pid filter");
      sd_journal_close(journal);
      return journal;
    }  
  }*/

  if (sd_journal_set_data_threshold(journal, LOGBUFFER) < 0)
  {
    LOG(NET_LOG, LOG_WARNING, "Failed to set journal message size\n");
  }

  return journal;
}

void find_aqualinkd_startupmsg(sd_journal *journal)
{
  static bool once=false;
  const void *log;
  size_t len;

  // Only going to do this one time, incase re reset while reading.
  if (once) {
    return;
  }
  once=true;

  sd_journal_previous_skip(journal, 200);

  while ( sd_journal_next(journal) > 0) // need to capture return of this
  {
    if (sd_journal_get_data(journal, "MESSAGE", &log, &len) >= 0) {
      if (rsm_strnstr((const char *)log+8, AQUALINKD_NAME, len-8) != NULL) {
        // Go back one and return
        sd_journal_previous_skip(journal, 1);
        return;
      }
    }
  }
  
  // Blindly go back 100 messages since above didn;t find start
  sd_journal_previous_skip(journal, 100);
}

#define BLANK_JOURNAL_READ_RESET 100

bool _broadcast_systemd_logmessages(bool aqMgrActive, bool reOpenStaleConnection);

bool broadcast_systemd_logmessages(bool aqMgrActive) {
  return _broadcast_systemd_logmessages(aqMgrActive, false);
}

bool _broadcast_systemd_logmessages(bool aqMgrActive, bool reOpenStaleConnection) {
  static sd_journal *journal;
  static bool active = false;
  char msg[WS_LOG_LENGTH];
  static int cnt=0;
  static char *cursor = NULL;
  //char filter[51];

  if (reOpenStaleConnection) {
    sd_journal_close(journal);
    active = false;
  }
  if (!aqMgrActive) {
    if (!active) {
      return true;
    } else {
      sd_journal_close(journal);
      active = false;
      return true;
      cursor = NULL;
    }
  } 
  // aqManager is active
  if (!active) {
      if ( (journal = open_journal()) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to open journal", WS_LOG_LENGTH,22);
        ws_send_logmsg(_mgr.active_connections, msg);
        return false;
      }
      if (sd_journal_seek_tail(journal) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to seek to journal end", WS_LOG_LENGTH,29);
        ws_send_logmsg(_mgr.active_connections, msg);
        sd_journal_close(journal);
        return false;
      }
      //if we have cusror go to it, otherwise jump back and try to find startup message
      if (cursor != NULL) {
        sd_journal_seek_cursor(journal, cursor);
        sd_journal_next(journal);
      } else
        find_aqualinkd_startupmsg(journal);

      active = true;
  }

  const void *log;
  size_t len;
  const void *pri;
  size_t plen;
  int rtn;

  while ( (rtn = sd_journal_next(journal)) > 0) // need to capture return of this
  {
    if (sd_journal_get_data(journal, "MESSAGE", &log, &len) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to get journal message", WS_LOG_LENGTH,29);
        ws_send_logmsg(_mgr.active_connections, msg);
    } else if (sd_journal_get_data(journal, "PRIORITY", &pri, &plen) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to seek to journal message priority", WS_LOG_LENGTH,42);
        ws_send_logmsg(_mgr.active_connections, msg);
    } else {
        build_logmsg_JSON(msg, atoi((const char *)pri+9), (const char *)log+8, WS_LOG_LENGTH,(int)len-8);
        ws_send_logmsg(_mgr.active_connections, msg);
        cnt=0;
        sd_journal_get_cursor(journal, &cursor);
    }
  }
  if (rtn < 0) {
    build_logmsg_JSON(msg, LOG_ERR, "Failed to get seen to next journal message", WS_LOG_LENGTH,42);
    ws_send_logmsg(_mgr.active_connections, msg);
    sd_journal_close(journal);
    active = false;
  } else if (rtn == 0) {
    // Sometimes we get no errors, and nothing to read, even when their is.
    // So if we get too many, restart but don;t reset the cursor.
    // Could test moving  sd_journal_get_cursor(journal, &cursor); line to here from above.

    // Quick way to times blank reads by log level, since less logs written higher number of blank reads
    if ( cnt++ >= BLANK_JOURNAL_READ_RESET * ( (LOG_DEBUG_SERIAL+1) - getSystemLogLevel() ) ) {
      /* Stale connection, call ourselves to reopen*/
      if (!reOpenStaleConnection) {
        //LOG(NET_LOG, LOG_WARNING, "**** %d Too many blank reads, resetting!! ****\n",cnt);
        return _broadcast_systemd_logmessages(aqMgrActive, true);
      }
      //LOG(NET_LOG, LOG_WARNING, "**** Reset didn't work ****\n",cnt);
      //return false;
    }
  }
  
  return true;
}


#define USEC_PER_SEC	1000000L

bool write_systemd_logmessages_2file(char *fname, int lines)
{
  FILE *fp = NULL;
  sd_journal *journal;
  const void *log;
  size_t len;
  const void *pri;
  size_t plen;
  char tsbuffer[20];
  uint64_t realtime;
  struct tm tm;
  time_t sec;


  fp = fopen (fname, "w");
  if (fp == NULL) {
    LOG(NET_LOG, LOG_WARNING, "Failed to open tmp log file '%s'\n",fname);
    return false;
  }
   if ( (journal = open_journal()) < 0) {
    fclose (fp);
    return false;
  }
  if (sd_journal_seek_tail(journal) < 0)
  {
    LOG(NET_LOG, LOG_WARNING, "Failed to seek to journal end");
    fclose (fp);
    sd_journal_close(journal);
    return false;
  }
  if (sd_journal_previous_skip(journal, lines) < 0)
  {
    LOG(NET_LOG, LOG_WARNING, "Failed to seek to journal start");
    fclose (fp);
    sd_journal_close(journal);
    return false;
  }

  while ( sd_journal_next(journal) > 0) // need to capture return of this
  {
    if (sd_journal_get_data(journal, "MESSAGE", &log, &len) < 0) {
        LOG(NET_LOG, LOG_WARNING, "Failed to get journal message");
    } else if (sd_journal_get_data(journal, "PRIORITY", &pri, &plen) < 0) {
        LOG(NET_LOG, LOG_WARNING, "Failed to get journal message priority");
    } else if (sd_journal_get_realtime_usec(journal, &realtime) < 0) {
        LOG(NET_LOG, LOG_WARNING, "Failed to get journal message timestamp");
    } else {
      sec = (time_t)(realtime/USEC_PER_SEC);
      localtime_r(&sec, &tm);
      strftime(tsbuffer, sizeof(tsbuffer), "%b %d %T", &tm); // need to capture return of this
      fprintf(fp, "%-15s %-7s %.*s\n",tsbuffer,elevel2text(atoi((const char *)pri+9)), (int)len-8,(const char *)log+8);
    }
  }

  sd_journal_close(journal);
  fclose (fp);

  return true;
}

#endif

void _broadcast_aqualinkstate(struct mg_connection *nc) 
{
  static int mqtt_count=0;
  struct mg_connection *c;
  char data[JSON_STATUS_SIZE];
#ifdef AQ_TM_DEBUG
  int tid;
#endif
  DEBUG_TIMER_START(&tid);

  build_aqualink_status_JSON(_aqualink_data, data, JSON_STATUS_SIZE);
  
  #ifdef AQ_MEMCMP
    if ( memcmp(_aqualink_data, &_last_mqtt_aqualinkdata, sizeof(struct aqualinkdata) ) == 0) {
      LOG(NET_LOG,LOG_NOTICE, "**********Structure buffs the same, ignoring request************\n");
      DEBUG_TIMER_CLEAR(&tid);
      return;
    }
  #endif

#ifndef MG_DISABLE_MQTT  
  if (_mqtt_exit_flag == true) {
    mqtt_count++;
    if (mqtt_count >= 10) {
      start_mqtt(nc->mgr);
      mqtt_count = 0;
    }
  }
#endif

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    //if (is_websocket(c) && !is_websocket_simulator(c)) // No need to broadcast status messages to simulator.
    if (is_websocket(c)) // All button simulator needs status messages
      ws_send(c, data);
#ifndef MG_DISABLE_MQTT
    else if (is_mqtt(c))
      mqtt_broadcast_aqualinkstate(c);
#endif
  }

  #ifdef AQ_MEMCMP
    memcpy(&_last_mqtt_aqualinkdata, _aqualink_data, sizeof(struct aqualinkdata));
  #endif

  DEBUG_TIMER_STOP(tid, NET_LOG, "broadcast_aqualinkstate() completed, took ");

  return;
}


void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message)
{
  static uint16_t msg_id = 0;

  if (toppic == NULL)
    return;

  if (msg_id >= 65535){msg_id=1;}else{msg_id++;}

  //mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_QOS(0), message, strlen(message));
  mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_RETAIN | MG_MQTT_QOS(1), message, strlen(message));

  LOG(NET_LOG,LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}


void send_domoticz_mqtt_state_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, value, TEMP_UNKNOWN);
  send_mqtt(nc, _aqconfig_.mqtt_dz_pub_topic, mqtt_msg);
}

void send_domoticz_mqtt_temp_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, 0, (_aqualink_data->temp_units==FAHRENHEIT && _aqconfig_.convert_dz_temp)?roundf(degFtoC(value)):value);
  send_mqtt(nc, _aqconfig_.mqtt_dz_pub_topic, mqtt_msg);
}
void send_domoticz_mqtt_numeric_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, 0, value);
  send_mqtt(nc, _aqconfig_.mqtt_dz_pub_topic, mqtt_msg);
}
void send_domoticz_mqtt_status_message(struct mg_connection *nc, int idx, int value, char *svalue) {
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_message_JSON(mqtt_msg, JSON_MQTT_MSG_SIZE, idx, value, svalue);

  send_mqtt(nc, _aqconfig_.mqtt_dz_pub_topic, mqtt_msg);
}

void send_mqtt_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s/delay",_aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, (state==FLASH?MQTT_ON:MQTT_OFF));

  sprintf(mqtt_pub_topic, "%s/%s",_aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, (state==OFF?MQTT_OFF:MQTT_ON));
}


void send_mqtt_timer_duration_msg(struct mg_connection *nc, char *dev_name, aqkey *button)
{
  static char mqtt_pub_topic[250];
  sprintf(mqtt_pub_topic, "%s/%s/timer/duration",_aqconfig_.mqtt_aq_topic, dev_name);
  if ((button->special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
    char val[10];
    sprintf(val, "%d", get_timer_left(button));
    send_mqtt(nc, mqtt_pub_topic, val);
  } else {
    send_mqtt(nc, mqtt_pub_topic, "0");
  }
}

void send_mqtt_timer_state_msg(struct mg_connection *nc, char *dev_name, aqkey *button)
{
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s/timer",_aqconfig_.mqtt_aq_topic, dev_name);

  send_mqtt(nc, mqtt_pub_topic, ( ((button->special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) && (button->led->state != OFF) )?MQTT_ON:MQTT_OFF );

  send_mqtt_timer_duration_msg(nc, dev_name, button);
}

//void send_mqtt_aux_msg(struct mg_connection *nc, char *root_topic, int dev_index, char *dev_topic, int value)
void send_mqtt_aux_msg(struct mg_connection *nc, char *dev_name, char *dev_topic, int value)
{
  static char mqtt_pub_topic[250];
  static char msg[10];
  
  sprintf(msg, "%d", value);

  //sprintf(mqtt_pub_topic, "%s/%s%d%s",_aqconfig_.mqtt_aq_topic, root_topic, dev_index, dev_topic);
  sprintf(mqtt_pub_topic, "%s/%s%s",_aqconfig_.mqtt_aq_topic, dev_name, dev_topic);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_led_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state, char *onS, char *offS)
{

  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s",_aqconfig_.mqtt_aq_topic, dev_name);

  if (state == ENABLE) {
    send_mqtt(nc, mqtt_pub_topic, offS);
    sprintf(mqtt_pub_topic, "%s/%s%s",_aqconfig_.mqtt_aq_topic, dev_name, ENABELED_SUBT);
    send_mqtt(nc, mqtt_pub_topic, onS);
  } else {
    send_mqtt(nc, mqtt_pub_topic, (state==OFF?offS:onS));
    sprintf(mqtt_pub_topic, "%s/%s%s",_aqconfig_.mqtt_aq_topic, dev_name, ENABELED_SUBT);
    send_mqtt(nc, mqtt_pub_topic, (state==OFF?offS:onS));
  }
}

void send_mqtt_swg_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  //send_mqtt_led_state_msg(nc, dev_name, state, SWG_ON, SWG_OFF);
  send_mqtt_led_state_msg(nc, dev_name, state, "2", "0");
}

void send_mqtt_heater_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  send_mqtt_led_state_msg(nc, dev_name, state, MQTT_ON, MQTT_OFF);
/*
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s",_aqconfig_.mqtt_aq_topic, dev_name);

  if (state == ENABLE) {
    send_mqtt(nc, mqtt_pub_topic, MQTT_OFF);
    sprintf(mqtt_pub_topic, "%s/%s%s",_aqconfig_.mqtt_aq_topic, dev_name, ENABELED_SUBT);
    send_mqtt(nc, mqtt_pub_topic, MQTT_ON);
  } else {
    send_mqtt(nc, mqtt_pub_topic, (state==OFF?MQTT_OFF:MQTT_ON));
    sprintf(mqtt_pub_topic, "%s/%s%s",_aqconfig_.mqtt_aq_topic, dev_name, ENABELED_SUBT);
    send_mqtt(nc, mqtt_pub_topic, (state==OFF?MQTT_OFF:MQTT_ON));
  }
*/
}


// NSF need to change this function to the _new once finished.
void send_mqtt_temp_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[10];
  // Use "not CELS" over "equal FAHR" so we default to FAHR for unknown units
  //sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(degC, "%.2f", (_aqualink_data->temp_units!=CELSIUS && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
/*
void send_mqtt_temp_msg_new(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[5];
  // NSF remove false below once we have finished.
  sprintf(degC, "%.2f", (false && _aqualink_data->temp_units==FAHRENHEIT && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  //sprintf(degC, "%d", value );
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
*/
void send_mqtt_setpoint_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[10];
  // Use "not CELS" over "equal FAHR" so we default to FAHR for unknown units
  //sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(degC, "%.2f", (_aqualink_data->temp_units!=CELSIUS && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s/setpoint", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
void send_mqtt_numeric_msg(struct mg_connection *nc, char *dev_name, int value)
{
  static char mqtt_pub_topic[250];
  static char msg[10];
  
  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%.2f", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_int_msg(struct mg_connection *nc, char *dev_name, int value) {
  send_mqtt_numeric_msg(nc, dev_name, value);
  /*
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
  */
}

void send_mqtt_string_msg(struct mg_connection *nc, const char *dev_name, const char *msg) {
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}


void mqtt_broadcast_aqualinkstate(struct mg_connection *nc)
{
  static int cnt=0;
  int i;
  const char *status;

  if (_aqconfig_.mqtt_timed_update) {
#ifdef AQ_NO_THREAD_NETSERVICE
    if (cnt > 300) {  // 100 = about every 2 minutes.
#else
    if (cnt > 30) {  // 30 = about every 2 minutes.
#endif
      reset_last_mqtt_status();
      cnt = 0;
    } else {
      cnt++;
    }
  }

//LOG(NET_LOG,LOG_INFO, "mqtt_broadcast_aqualinkstate: START\n");

  if (_aqualink_data->service_mode_state != _last_mqtt_aqualinkdata.service_mode_state) {
     _last_mqtt_aqualinkdata.service_mode_state = _aqualink_data->service_mode_state;
     send_mqtt_string_msg(nc, SERVICE_MODE_TOPIC, _aqualink_data->service_mode_state==OFF?MQTT_OFF:(_aqualink_data->service_mode_state==FLASH?MQTT_FLASH:MQTT_ON));
  }

  // Only send to display messag topic if not in simulator mode
  //if (!_aqualink_data->simulate_panel) {
    status = getAqualinkDStatusMessage(_aqualink_data);
    if (strcmp(status, _last_mqtt_aqualinkdata.last_display_message) != 0) {
      strcpy(_last_mqtt_aqualinkdata.last_display_message, status);
      send_mqtt_string_msg(nc, DISPLAY_MSG_TOPIC, status);
    }
  //}

  if (_aqualink_data->air_temp != TEMP_UNKNOWN && _aqualink_data->air_temp != _last_mqtt_aqualinkdata.air_temp) {
    _last_mqtt_aqualinkdata.air_temp = _aqualink_data->air_temp;
    send_mqtt_temp_msg(nc, AIR_TEMP_TOPIC, _aqualink_data->air_temp);
    //send_mqtt_temp_msg_new(nc, AIR_TEMPERATURE_TOPIC, _aqualink_data->air_temp);
    send_domoticz_mqtt_temp_msg(nc, _aqconfig_.dzidx_air_temp, _aqualink_data->air_temp);
  }

  if (_aqualink_data->pool_temp != _last_mqtt_aqualinkdata.pool_temp) {
    if (_aqualink_data->pool_temp == TEMP_UNKNOWN && _aqconfig_.report_zero_pool_temp) {
      _last_mqtt_aqualinkdata.pool_temp = TEMP_UNKNOWN;
      send_mqtt_temp_msg(nc, POOL_TEMP_TOPIC, 0);
    } if (_aqualink_data->pool_temp == TEMP_UNKNOWN && ! _aqconfig_.report_zero_pool_temp) {
      // Don't post anything in this case, ie leave last posted value alone
    } else if (_aqualink_data->pool_temp != TEMP_UNKNOWN) {
      _last_mqtt_aqualinkdata.pool_temp = _aqualink_data->pool_temp;
      send_mqtt_temp_msg(nc, POOL_TEMP_TOPIC, _aqualink_data->pool_temp);
      send_domoticz_mqtt_temp_msg(nc, _aqconfig_.dzidx_pool_water_temp, _aqualink_data->pool_temp);
      // IF spa is off, report pool water temp to Domoticz.
      if (_aqualink_data->spa_temp == TEMP_UNKNOWN)
        send_domoticz_mqtt_temp_msg(nc, _aqconfig_.dzidx_spa_water_temp, _aqualink_data->pool_temp);
    }
  } 
  
  if (_aqualink_data->spa_temp != _last_mqtt_aqualinkdata.spa_temp) {
    if (_aqualink_data->spa_temp == TEMP_UNKNOWN && _aqconfig_.report_zero_spa_temp) {
      _last_mqtt_aqualinkdata.spa_temp = TEMP_UNKNOWN;
      send_mqtt_temp_msg(nc, SPA_TEMP_TOPIC, 0);
    } if (_aqualink_data->spa_temp == TEMP_UNKNOWN && ! _aqconfig_.report_zero_spa_temp && _aqualink_data->pool_temp != TEMP_UNKNOWN ) {
      // Use Pool Temp as spa temp
      if (_last_mqtt_aqualinkdata.spa_temp != _aqualink_data->pool_temp) {
        _last_mqtt_aqualinkdata.spa_temp = _aqualink_data->pool_temp;
        send_mqtt_temp_msg(nc, SPA_TEMP_TOPIC, _aqualink_data->pool_temp);
      }
    } else if (_aqualink_data->spa_temp != TEMP_UNKNOWN) {
      _last_mqtt_aqualinkdata.spa_temp = _aqualink_data->spa_temp;
      send_mqtt_temp_msg(nc, SPA_TEMP_TOPIC, _aqualink_data->spa_temp);
      send_domoticz_mqtt_temp_msg(nc, _aqconfig_.dzidx_spa_water_temp, _aqualink_data->spa_temp);
    }
  } 

  if (_aqualink_data->pool_htr_set_point != TEMP_UNKNOWN && _aqualink_data->pool_htr_set_point != _last_mqtt_aqualinkdata.pool_htr_set_point) {
    _last_mqtt_aqualinkdata.pool_htr_set_point = _aqualink_data->pool_htr_set_point;
    send_mqtt_setpoint_msg(nc, BTN_POOL_HTR, _aqualink_data->pool_htr_set_point);
  }

  if (_aqualink_data->spa_htr_set_point != TEMP_UNKNOWN && _aqualink_data->spa_htr_set_point != _last_mqtt_aqualinkdata.spa_htr_set_point) {
    _last_mqtt_aqualinkdata.spa_htr_set_point = _aqualink_data->spa_htr_set_point;
    send_mqtt_setpoint_msg(nc, BTN_SPA_HTR, _aqualink_data->spa_htr_set_point);
  }

  if (_aqualink_data->frz_protect_set_point != TEMP_UNKNOWN && _aqualink_data->frz_protect_set_point != _last_mqtt_aqualinkdata.frz_protect_set_point) {
    _last_mqtt_aqualinkdata.frz_protect_set_point = _aqualink_data->frz_protect_set_point;
    send_mqtt_setpoint_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_set_point);
    send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_ON);
    // Duplicate of below if statment.  NSF come back and check if necessary for startup.
    send_mqtt_string_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_state==ON?MQTT_ON:MQTT_OFF);
    _last_mqtt_aqualinkdata.frz_protect_state = _aqualink_data->frz_protect_state;
  }

  if (_aqualink_data->frz_protect_state != _last_mqtt_aqualinkdata.frz_protect_state) {
    _last_mqtt_aqualinkdata.frz_protect_state = _aqualink_data->frz_protect_state;
    send_mqtt_string_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_state==ON?MQTT_ON:MQTT_OFF);
    //send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_ON);
  }

  if (_aqualink_data->battery != _last_mqtt_aqualinkdata.battery) {
    _last_mqtt_aqualinkdata.battery = _aqualink_data->battery;
    send_mqtt_string_msg(nc, BATTERY_STATE, _aqualink_data->battery==OK?MQTT_ON:MQTT_OFF); 
  }

  if (_aqualink_data->ph != TEMP_UNKNOWN && _aqualink_data->ph != _last_mqtt_aqualinkdata.ph) {
    _last_mqtt_aqualinkdata.ph = _aqualink_data->ph;
    send_mqtt_float_msg(nc, CHEM_PH_TOPIC, _aqualink_data->ph);
    send_mqtt_float_msg(nc, CHRM_PH_F_TOPIC, roundf(degFtoC(_aqualink_data->ph)));
  }
  if (_aqualink_data->orp != TEMP_UNKNOWN && _aqualink_data->orp != _last_mqtt_aqualinkdata.orp) {
    _last_mqtt_aqualinkdata.orp = _aqualink_data->orp;
    send_mqtt_numeric_msg(nc, CHEM_ORP_TOPIC, _aqualink_data->orp);
    send_mqtt_float_msg(nc, CHRM_ORP_F_TOPIC, roundf(degFtoC(_aqualink_data->orp)));
  }

  // Salt Water Generator
  if (_aqualink_data->swg_led_state != LED_S_UNKNOWN) {

    //LOG(NET_LOG,LOG_DEBUG, "Sending MQTT SWG MEssages\n");

    if (_aqualink_data->swg_led_state != _last_mqtt_aqualinkdata.swg_led_state) {
       send_mqtt_swg_state_msg(nc, SWG_TOPIC, _aqualink_data->swg_led_state);
       _last_mqtt_aqualinkdata.swg_led_state = _aqualink_data->swg_led_state;
    }

    if (_aqualink_data->swg_percent != TEMP_UNKNOWN && (_aqualink_data->swg_percent != _last_mqtt_aqualinkdata.swg_percent)) {
      _last_mqtt_aqualinkdata.swg_percent = _aqualink_data->swg_percent;
      send_mqtt_numeric_msg(nc, SWG_PERCENT_TOPIC, _aqualink_data->swg_percent);
      send_mqtt_float_msg(nc, SWG_PERCENT_F_TOPIC, roundf(degFtoC(_aqualink_data->swg_percent)));
      send_mqtt_float_msg(nc, SWG_SETPOINT_TOPIC, roundf(degFtoC(_aqualink_data->swg_percent)));
      send_domoticz_mqtt_numeric_msg(nc, _aqconfig_.dzidx_swg_percent, _aqualink_data->swg_percent);
    }
    if (_aqualink_data->swg_ppm != TEMP_UNKNOWN && (_aqualink_data->swg_ppm != _last_mqtt_aqualinkdata.swg_ppm)) {
      _last_mqtt_aqualinkdata.swg_ppm = _aqualink_data->swg_ppm;
      send_mqtt_numeric_msg(nc, SWG_PPM_TOPIC, _aqualink_data->swg_ppm);
      send_mqtt_float_msg(nc, SWG_PPM_F_TOPIC, roundf(degFtoC(_aqualink_data->swg_ppm)));
      send_domoticz_mqtt_numeric_msg(nc, _aqconfig_.dzidx_swg_ppm, _aqualink_data->swg_ppm);
    }

    if (_aqualink_data->boost != _last_mqtt_aqualinkdata.boost) {
      send_mqtt_int_msg(nc, SWG_BOOST_TOPIC, _aqualink_data->boost);
      _last_mqtt_aqualinkdata.boost = _aqualink_data->boost;
    }

    if ( _aqualink_data->boost_duration != _last_mqtt_aqualinkdata.boost_duration ) {
      send_mqtt_int_msg(nc, SWG_BOOST_DURATION_TOPIC, _aqualink_data->boost_duration);
      _last_mqtt_aqualinkdata.boost_duration = _aqualink_data->boost_duration;
    }
    
  } else {
    //LOG(NET_LOG,LOG_DEBUG, "SWG status unknown\n");
  }

  if (_aqualink_data->ar_swg_device_status != SWG_STATUS_UNKNOWN) {
    //LOG(NET_LOG,LOG_DEBUG, "Sending MQTT SWG Extended %d\n",_aqualink_data->ar_swg_device_status);
    if (_aqualink_data->ar_swg_device_status != _last_mqtt_aqualinkdata.ar_swg_device_status) {
      char message[30];
      int status;
      int dzalert;
    
      get_swg_status_mqtt(_aqualink_data, message, &status, &dzalert);

      send_domoticz_mqtt_status_message(nc, _aqconfig_.dzidx_swg_status, dzalert, &message[9]);
      send_mqtt_int_msg(nc, SWG_EXTENDED_TOPIC, (int)_aqualink_data->ar_swg_device_status);
      send_mqtt_string_msg(nc, SWG_STATUS_MSG_TOPIC, message);

      _last_mqtt_aqualinkdata.ar_swg_device_status = _aqualink_data->ar_swg_device_status;
      //LOG(NET_LOG,LOG_DEBUG, "SWG Extended sending cur=%d sent=%d\n",_aqualink_data->ar_swg_device_status,_last_mqtt_aqualinkdata.ar_swg_device_status);
    } else {
      //LOG(NET_LOG,LOG_DEBUG, "SWG Extended already sent cur=%d sent=%d\n",_aqualink_data->ar_swg_device_status,_last_mqtt_aqualinkdata.ar_swg_device_status);
    }
  } else {
    //LOG(NET_LOG,LOG_DEBUG, "SWG Extended unknown\n");
  }

  if (READ_RSDEV_JXI && _aqualink_data->heater_err_status != _last_mqtt_aqualinkdata.heater_err_status) {
    char message[30];

    if (_aqualink_data->heater_err_status == NUL) {
      send_mqtt_int_msg(nc, LXI_ERROR_CODE, (int)_aqualink_data->heater_err_status);
      send_mqtt_string_msg(nc, LXI_ERROR_MESSAGE, "");
    } else {
      //send_mqtt_int_msg(nc, LXI_STATUS, (int)_aqualink_data->heater_err_status);
      send_mqtt_int_msg(nc, LXI_ERROR_CODE, (int)_aqualink_data->heater_err_status);
      getJandyHeaterErrorMQTT(_aqualink_data, message);
      send_mqtt_string_msg(nc, LXI_ERROR_MESSAGE, status);
    }

    _last_mqtt_aqualinkdata.heater_err_status = _aqualink_data->heater_err_status;
  }

  // LOG(NET_LOG,LOG_INFO, "mqtt_broadcast_aqualinkstate: START LEDs\n");

  // if (time(NULL) % 2) {}   <-- use to determin odd/even second in time to make state flash on enabled.

  // Loop over LED's and send any changes.
  for (i=0; i < _aqualink_data->total_buttons; i++) {
    if (_last_mqtt_aqualinkdata.aqualinkleds[i].state != _aqualink_data->aqbuttons[i].led->state) {
      _last_mqtt_aqualinkdata.aqualinkleds[i].state = _aqualink_data->aqbuttons[i].led->state;
      if (_aqualink_data->aqbuttons[i].code == KEY_POOL_HTR || _aqualink_data->aqbuttons[i].code == KEY_SPA_HTR) {
        send_mqtt_heater_state_msg(nc, _aqualink_data->aqbuttons[i].name, _aqualink_data->aqbuttons[i].led->state);
      } else {
        send_mqtt_state_msg(nc, _aqualink_data->aqbuttons[i].name, _aqualink_data->aqbuttons[i].led->state);
      }

      send_mqtt_timer_state_msg(nc, _aqualink_data->aqbuttons[i].name, &_aqualink_data->aqbuttons[i]);

      if (_aqualink_data->aqbuttons[i].dz_idx != DZ_NULL_IDX) {
        send_domoticz_mqtt_state_msg(nc, _aqualink_data->aqbuttons[i].dz_idx, (_aqualink_data->aqbuttons[i].led->state==OFF?DZ_OFF:DZ_ON));
      }
    } else if ((_aqualink_data->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
      //send_mqtt_timer_duration_msg(nc, _aqualink_data->aqbuttons[i].name, &_aqualink_data->aqbuttons[i]);
      // send_mqtt_timer_state_msg will call send_mqtt_timer_duration_msg so no need to do it here.
      // Have to use send_mqtt_timer_state_msg due to a timer being set on a device that's already on, (ir no state change so above code does't get hit)
      send_mqtt_timer_state_msg(nc, _aqualink_data->aqbuttons[i].name, &_aqualink_data->aqbuttons[i]);

    }
  }

  // Loop over Pumps
  for (i=0; i < _aqualink_data->num_pumps; i++) {
    //_aqualink_data->pumps[i].rpm = TEMP_UNKNOWN;
    //_aqualink_data->pumps[i].gph = TEMP_UNKNOWN;
    //_aqualink_data->pumps[i].watts = TEMP_UNKNOWN;

    if (_aqualink_data->pumps[i].rpm != TEMP_UNKNOWN && _aqualink_data->pumps[i].rpm != _last_mqtt_aqualinkdata.pumps[i].rpm) {
      _last_mqtt_aqualinkdata.pumps[i].rpm = _aqualink_data->pumps[i].rpm;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_RPM_TOPIC, _aqualink_data->pumps[i].rpm);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_RPM_TOPIC, _aqualink_data->pumps[i].rpm);
    }
    if (_aqualink_data->pumps[i].gpm != TEMP_UNKNOWN && _aqualink_data->pumps[i].gpm != _last_mqtt_aqualinkdata.pumps[i].gpm) {
      _last_mqtt_aqualinkdata.pumps[i].gpm = _aqualink_data->pumps[i].gpm;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_GPH_TOPIC, _aqualink_data->pumps[i].gph);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_GPM_TOPIC, _aqualink_data->pumps[i].gpm);
    }
    if (_aqualink_data->pumps[i].watts != TEMP_UNKNOWN && _aqualink_data->pumps[i].watts != _last_mqtt_aqualinkdata.pumps[i].watts) {
      _last_mqtt_aqualinkdata.pumps[i].watts = _aqualink_data->pumps[i].watts;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_WATTS_TOPIC, _aqualink_data->pumps[i].watts);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_WATTS_TOPIC, _aqualink_data->pumps[i].watts);
    }
    if (_aqualink_data->pumps[i].mode != TEMP_UNKNOWN && _aqualink_data->pumps[i].mode != _last_mqtt_aqualinkdata.pumps[i].mode) {
      _last_mqtt_aqualinkdata.pumps[i].mode = _aqualink_data->pumps[i].mode;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_MODE_TOPIC, _aqualink_data->pumps[i].mode);
    }
    if (_aqualink_data->pumps[i].status != TEMP_UNKNOWN && _aqualink_data->pumps[i].status != _last_mqtt_aqualinkdata.pumps[i].status) {
      _last_mqtt_aqualinkdata.pumps[i].status = _aqualink_data->pumps[i].status;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_STATUS_TOPIC, _aqualink_data->pumps[i].status);
    }
    if (_aqualink_data->pumps[i].pressureCurve != TEMP_UNKNOWN && _aqualink_data->pumps[i].pressureCurve != _last_mqtt_aqualinkdata.pumps[i].pressureCurve) {
      _last_mqtt_aqualinkdata.pumps[i].pressureCurve = _aqualink_data->pumps[i].pressureCurve;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_PPC_TOPIC, _aqualink_data->pumps[i].pressureCurve);
    }
  }
}


typedef enum {uActioned, uBad, uDevices, uStatus, uHomebridge, uDynamicconf, uDebugStatus, uDebugDownload, uSimulator, uSchedules, uSetSchedules, uAQmanager, uLogDownload, uNotAvailable} uriAtype;
//typedef enum {NET_MQTT=0, NET_API, NET_WS, DZ_MQTT} netRequest;
const char actionName[][5] = {"MQTT", "API", "WS", "DZ"};

#define BAD_SETPOINT      "No device for setpoint found"
#define NO_PLIGHT_DEVICE  "No programable light found"
#define NO_VSP_SUPPORT    "Pump VS programs not supported yet"
#define PUMP_NOT_FOUND    "No matching Pump found"
#define NO_DEVICE         "No matching Device found"
#define INVALID_VALUE     "Invalid value"
#define NOCHANGE_IGNORING "No change, device is already in that state"
#define UNKNOWN_REQUEST   "Didn't understand request"



#ifdef AQ_PDA
void create_PDA_on_off_request(aqkey *button, bool isON) 
{
  int i;
   char msg[PTHREAD_ARG];

  for (i=0; i < _aqualink_data->total_buttons; i++) {
    if (_aqualink_data->aqbuttons[i].code == button->code) {
      sprintf(msg, "%-5d%-5d", i, (isON? ON : OFF));
      aq_programmer(AQ_PDA_DEVICE_ON_OFF, msg, _aqualink_data);
      break;
    }
  }
}
#endif


//uriAtype action_URI(char *from, const char *URI, int uri_length, float value, bool convertTemp) {
//uriAtype action_URI(netRequest from, const char *URI, int uri_length, float value, bool convertTemp) {
uriAtype action_URI(request_source from, const char *URI, int uri_length, float value, bool convertTemp, char **rtnmsg) {
  /* Example URI ()
  * Note URI is NOT terminated
  * devices
  * status
  * Freeze/setpoint/set
  * Filter_Pump/set
  * Pool_Heater/setpoint/set
  * Pool_Heater/set
  * SWG/Percent_f/set
  * Filter_Pump/RPM/set
  * Pump_1/RPM/set
  * Pool Light/color/set
  * Pool Light/program/set
  */
  uriAtype rtn = uBad;
  bool found = false;
  int i;
  char *ri1 = (char *)URI;
  char *ri2 = NULL;
  char *ri3 = NULL;
  //bool charvalue=false;
  //char *ri4 = NULL;

  LOG(NET_LOG,LOG_DEBUG, "%s: URI Request '%.*s': value %.2f\n", actionName[from], uri_length, URI, value);

  // Split up the URI into parts.
  for (i=1; i < uri_length; i++) {
    if ( URI[i] == '/' ) {
      if (ri2 == NULL) {
        ri2 = (char *)&URI[++i];
      } else if (ri3 == NULL) {
        ri3 = (char *)&URI[++i];
        break;
      } /*else if (ri4 == NULL) {
        ri4 = (char *)&URI[++i];
        break;
      }*/
    }
  }

  //LOG(NET_LOG,LOG_NOTICE, "URI Request: %.*s, %.*s, %.*s | %f\n", uri_length, ri1, uri_length - (ri2 - ri1), ri2, uri_length - (ri3 - ri1), ri3, value);
  
  if (strncmp(ri1, "devices", 7) == 0) {
    return uDevices;
  } else if (strncmp(ri1, "status", 6) == 0) {
    return uStatus;
  } else if (strncmp(ri1, "homebridge", 10) == 0) {
    return uHomebridge;
  } else if (strncmp(ri1, "dynamicconfig", 13) == 0) {
    return uDynamicconf;
  } else if (strncmp(ri1, "schedules/set", 13) == 0) {
    return uSetSchedules;
  } else if (strncmp(ri1, "schedules", 9) == 0) {
    return uSchedules;
  } else if (strncmp(ri1, "simulator", 9) == 0 && from == NET_WS) { // Only valid from websocket.
    if (ri2 != NULL && strncmp(ri2, "onetouch", 8) == 0) {
      start_simulator(_aqualink_data, ONETOUCH);
    } else if (ri2 != NULL && strncmp(ri2, "allbutton", 9) == 0) {
      start_simulator(_aqualink_data, ALLBUTTON);
    } else if (ri2 != NULL && strncmp(ri2, "aquapda", 7) == 0) {
      start_simulator(_aqualink_data, AQUAPDA);
    } else if (ri2 != NULL && strncmp(ri2, "iaqtouch", 8) == 0) {
      start_simulator(_aqualink_data, IAQTOUCH);
    } else  {
      return uBad;
    }
    return uSimulator;
  } else if (strncmp(ri1, "simcmd", 10) == 0 && from == NET_WS) { // Only valid from websocket.
    simulator_send_cmd((unsigned char)value);
    return uActioned;
#ifdef AQ_MANAGER
  } else if (strncmp(ri1, "aqmanager", 9) == 0 && from == NET_WS) { // Only valid from websocket.
    return uAQmanager;
  } else if (strncmp(ri1, "setloglevel", 11) == 0 && from == NET_WS) { // Only valid from websocket.
    setSystemLogLevel(round(value));
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "addlogmask", 10) == 0 && from == NET_WS) { // Only valid from websocket.
    addDebugLogMask(round(value));
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "removelogmask", 13) == 0 && from == NET_WS) { // Only valid from websocket.
    removeDebugLogMask(round(value));
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "logfile", 7) == 0) {
    /*
    if (ri2 != NULL && strncmp(ri2, "start", 5) == 0) {
      startInlineLog2File();
    } else if (ri2 != NULL && strncmp(ri2, "stop", 4) == 0) {
      stopInlineLog2File();
    } else if (ri2 != NULL && strncmp(ri2, "clean", 5) == 0) {
      cleanInlineLog2File();
    } else*/ if (ri2 != NULL && strncmp(ri2, "download", 8) == 0) {
      return uLogDownload;
    } 
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "restart", 7) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received restart request!\n");
    raise(SIGRESTART);
    return uActioned; 
  } else if (strncmp(ri1, "seriallogger", 12) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received request to run serial_logger!\n");
    _aqualink_data->run_slogger = true;
    return uActioned; 
#else // AQ_MANAGER
  } else if (strncmp(ri1, "aqmanager", 9) == 0 && from == NET_WS) { // Only valid from websocket.
    return uNotAvailable;
  // BELOW IS FOR OLD DEBUG.HTML, Need to remove in future release with aqmanager goes live
  } else if (strncmp(ri1, "debug", 5) == 0) {
    if (ri2 != NULL && strncmp(ri2, "start", 5) == 0) {
      startInlineDebug();
    } else if (ri2 != NULL && strncmp(ri2, "stop", 4) == 0) {
      stopInlineDebug();
    } else if (ri2 != NULL && strncmp(ri2, "serialstart", 11) == 0) {
      startInlineSerialDebug();
    } else if (ri2 != NULL && strncmp(ri2, "serialstop", 10) == 0) {
      stopInlineDebug();
    } else if (ri2 != NULL && strncmp(ri2, "clean", 5) == 0) {
      cleanInlineDebug();
    } else if (ri2 != NULL && strncmp(ri2, "download", 8) == 0) {
      return uDebugDownload;
    } 
    return uDebugStatus;
#endif //AQ_MANAGER
// couple of debug items for testing 
  } else if (strncmp(ri1, "set_date_time", 13) == 0) {
    //aq_programmer(AQ_SET_TIME, NULL, _aqualink_data);
    panel_device_request(_aqualink_data, DATE_TIME, 0, 0, from);
    return uActioned;
  } else if (strncmp(ri1, "startup_program", 15) == 0) {
    if(isRS_PANEL)
      queueGetProgramData(ALLBUTTON, _aqualink_data);
    if(isRSSA_ENABLED)
      queueGetProgramData(RSSADAPTER, _aqualink_data);
#ifdef AQ_ONETOUCH
    if(isONET_ENABLED)
      queueGetProgramData(ONETOUCH, _aqualink_data);
#endif
#ifdef AQ_IAQTOUCH
    if(isIAQT_ENABLED)
      queueGetProgramData(IAQTOUCH, _aqualink_data);
#endif
#ifdef AQ_PDA
    if(isPDA_PANEL)
      queueGetProgramData(AQUAPDA, _aqualink_data);
#endif
    return uActioned;
// Action a setpoint message
  } else if (ri3 != NULL && (strncasecmp(ri2, "setpoint", 8) == 0) && (strncasecmp(ri3, "increment", 9) == 0)) {
    if (!isRSSA_ENABLED) {
      LOG(NET_LOG,LOG_WARNING, "%s: ignoring %.*s setpoint increment only valid when RS Serial adapter protocol is enabeled\n", actionName[from], uri_length, URI);
      *rtnmsg = BAD_SETPOINT;
      return uBad;
    }

    int val = round(value);

    if (strncmp(ri1, BTN_POOL_HTR, strlen(BTN_POOL_HTR)) == 0) {
      //create_program_request(from, POOL_HTR_INCREMENT, val, 0);
      panel_device_request(_aqualink_data, POOL_HTR_INCREMENT, 0, val, from);
    } else if (strncmp(ri1, BTN_SPA_HTR, strlen(BTN_SPA_HTR)) == 0) {
      //create_program_request(from, SPA_HTR_INCREMENT, val, 0);
      panel_device_request(_aqualink_data, SPA_HTR_INCREMENT, 0, val, from);
    } else {
      LOG(NET_LOG,LOG_WARNING, "%s: ignoring %.*s setpoint add only valid for pool & spa\n", actionName[from], uri_length, URI);
      *rtnmsg = BAD_SETPOINT;
      return uBad;
    }
    rtn = uActioned;
  } else if (ri3 != NULL && (strncasecmp(ri2, "setpoint", 8) == 0) && (strncasecmp(ri3, "set", 3) == 0)) {
    int val =  convertTemp? round(degCtoF(value)) : round(value);
    if (strncmp(ri1, BTN_POOL_HTR, strlen(BTN_POOL_HTR)) == 0) {
     //create_program_request(from, POOL_HTR_SETOINT, val, 0);
      panel_device_request(_aqualink_data, POOL_HTR_SETOINT, 0, val, from);
    } else if (strncmp(ri1, BTN_SPA_HTR, strlen(BTN_SPA_HTR)) == 0) {
      //create_program_request(from, SPA_HTR_SETOINT, val, 0);
      panel_device_request(_aqualink_data, SPA_HTR_SETOINT, 0, val, from);
    } else if (strncmp(ri1, FREEZE_PROTECT, strlen(FREEZE_PROTECT)) == 0) {
      //create_program_request(from, FREEZE_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, FREEZE_SETPOINT, 0, val, from);
    } else if (strncmp(ri1, "SWG", 3) == 0) {  // If we get SWG percent as setpoint message it's from homebridge so use the convert
      //int val = round(degCtoF(value));
      //int val = convertTemp? round(degCtoF(value)) : round(value);
      //create_program_request(from, SWG_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, SWG_SETPOINT, 0, val, from);
    } else {
      // Not sure what the setpoint is, ignore.
      LOG(NET_LOG,LOG_WARNING, "%s: ignoring %.*s don't recognise button setpoint\n", actionName[from], uri_length, URI);
      *rtnmsg = BAD_SETPOINT;
      return uBad;
    }
    rtn = uActioned;
    /* Moved into create_program_request()
    if (from == NET_MQTT) // We can get multiple MQTT requests for
      time(&_aqualink_data->unactioned.requested);
    else
      _aqualink_data->unactioned.requested = 0;
      */
  // Action a SWG Percent message
  } else if ((ri3 != NULL && (strncmp(ri1, "SWG", 3) == 0) && (strncasecmp(ri2, "Percent", 7) == 0) && (strncasecmp(ri3, "set", 3) == 0))) {
    int val;
    if ( (strncmp(ri2, "Percent_f", 9) == 0)  ) {
      val = _aqualink_data->unactioned.value = round(degCtoF(value));
    } else {
      val = _aqualink_data->unactioned.value = round(value);
    }
    //create_program_request(from, SWG_SETPOINT, val, 0);
    panel_device_request(_aqualink_data, SWG_SETPOINT, 0, val, from);
    rtn = uActioned;
  // Action a SWG boost message
  } else if ((ri3 != NULL && (strncmp(ri1, "SWG", 3) == 0) && (strncasecmp(ri2, "Boost", 5) == 0) && (strncasecmp(ri3, "set", 3) == 0))) {
    //create_program_request(from, SWG_BOOST, round(value), 0);
    panel_device_request(_aqualink_data, SWG_BOOST, 0, round(value), from);
    if (_aqualink_data->swg_led_state == OFF)
      rtn = uBad; // Return bad so we repost a mqtt update
    else
      rtn = uActioned;
  // Action Light program.
  } else if ((ri3 != NULL && ((strncasecmp(ri2, "color", 5) == 0) || (strncasecmp(ri2, "program", 7) == 0)) && (strncasecmp(ri3, "set", 3) == 0))) {
    found = false;
    for (i=0; i < _aqualink_data->total_buttons; i++) {
      if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ||
          strncmp(ri1, _aqualink_data->aqbuttons[i].label, strlen(_aqualink_data->aqbuttons[i].label)) == 0)
      {
        //char buf[5];
        found = true;
        //sprintf(buf,"%.0f",value);
        //set_light_mode(buf, i);
        panel_device_request(_aqualink_data, LIGHT_MODE, i, value, from);
        break;
      }
    }
    if(!found) {
      *rtnmsg = NO_PLIGHT_DEVICE;
      LOG(NET_LOG,LOG_WARNING, "%s: Didn't find device that matched URI '%.*s'\n",actionName[from], uri_length, URI);
      rtn = uBad;
    }
  // Action a pump RPM/GPM message
  } else if ((ri3 != NULL && ((strncasecmp(ri2, "RPM", 3) == 0) || (strncasecmp(ri2, "GPM", 3) == 0) || (strncasecmp(ri2, "VSP", 3) == 0)) && (strncasecmp(ri3, "set", 3) == 0))) {
    found = false;
    // Is it a pump index or pump name
    if (strncmp(ri1, "Pump_", 5) == 0) { // Pump by number
      int pumpIndex = atoi(ri1+5); // Check for 0   
      for (i=0; i < _aqualink_data->num_pumps; i++) {
        if (_aqualink_data->pumps[i].pumpIndex == pumpIndex) {
          if ((strncasecmp(ri2, "VSP", 3) == 0)) {
            if (isIAQT_ENABLED) {
              //LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d to program %d\n",actionName[from], pumpIndex+1, round(value));
              //create_program_request(from, PUMP_VSPROGRAM, round(value), pumpIndex);
              LOG(NET_LOG,LOG_ERR, "Setting Pump VSP is not supported yet\n");
              *rtnmsg = NO_VSP_SUPPORT;
              return uBad;
            } else {
              LOG(NET_LOG,LOG_ERR, "Setting Pump VSP only supported if iAqualinkTouch protocol en enabled\n");
              *rtnmsg = NO_VSP_SUPPORT;
              return uBad;
            }
          } else {
            LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d %s to %d\n",actionName[from],pumpIndex+1, (strncasecmp(ri2, "GPM", 3) == 0)?"GPM":"RPM", round(value));
            //create_program_request(from, PUMP_RPM, round(value), pumpIndex);
            panel_device_request(_aqualink_data, PUMP_RPM, pumpIndex, round(value), from);
          }
          //_aqualink_data->unactioned.type = PUMP_RPM;
          //_aqualink_data->unactioned.value = round(value);
          //_aqualink_data->unactioned.id = pumpIndex;
          found=true;
          break;
        }
      }
    } else { // Pump by button name
      for (i=0; i < _aqualink_data->total_buttons ; i++) {
        if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ){
          int pi;
          for (pi=0; pi < _aqualink_data->num_pumps; pi++) {
            if (_aqualink_data->pumps[pi].button == &_aqualink_data->aqbuttons[i]) {
              if ((strncasecmp(ri2, "VSP", 3) == 0)) {
                if (isIAQT_ENABLED) {
                  //LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d to program %d\n",actionName[from], pi+1, round(value));
                  //create_program_request(from, PUMP_VSPROGRAM, round(value), _aqualink_data->pumps[pi].pumpIndex);
                  LOG(NET_LOG,LOG_ERR, "Setting Pump VSP is not supported yet\n");
                  *rtnmsg = NO_VSP_SUPPORT;
                  return uBad;
                } else {
                  LOG(NET_LOG,LOG_ERR, "Setting Pump VSP only supported if iAqualinkTouch protocol en enabled\n");
                  *rtnmsg = NO_VSP_SUPPORT;
                  return uBad;
                }
              } else {
                LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d %s to %d\n",actionName[from], pi+1, (strncasecmp(ri2, "GPM", 3) == 0)?"GPM":"RPM", round(value));
                //create_program_request(from, PUMP_RPM, round(value), _aqualink_data->pumps[pi].pumpIndex);
                panel_device_request(_aqualink_data, PUMP_RPM, _aqualink_data->pumps[pi].pumpIndex, round(value), from);
              }
              //_aqualink_data->unactioned.type = PUMP_RPM;
              //_aqualink_data->unactioned.value = round(value);
              //_aqualink_data->unactioned.id = _aqualink_data->pumps[pi].pumpIndex;
              found=true;
              break;
            }
          }
        }
      }
    }
    if(!found) {
      LOG(NET_LOG,LOG_WARNING, "%s: Didn't find pump from message %.*s\n",actionName[from], uri_length, URI);
      *rtnmsg = PUMP_NOT_FOUND;
      rtn = uBad;
    } else {
      rtn = uActioned;
    }
  } else if ((ri3 != NULL && (strncmp(ri1, "CHEM", 4) == 0) && (strncasecmp(ri3, "set", 3) == 0))) {
  //aqualinkd/CHEM/pH/set
  //aqualinkd/CHEM/ORP/set
    if ( strncasecmp(ri2, "ORP", 3) == 0 ) {
      _aqualink_data->orp = round(value);
      rtn = uActioned;
      LOG(NET_LOG,LOG_NOTICE, "%s: request to set ORP to %d\n",actionName[from],_aqualink_data->orp);
    } else if ( strncasecmp(ri2, "Ph", 2) == 0 ) {
      _aqualink_data->ph = value;
      rtn = uActioned;
      LOG(NET_LOG,LOG_NOTICE, "%s: request to set Ph to %.2f\n",actionName[from],_aqualink_data->ph);
    } else {
      LOG(NET_LOG,LOG_WARNING,"%s: ignoring, unknown URI %.*s\n",actionName[from],uri_length,URI);
      rtn = uBad;
    }
  // Action a Turn on / off message
  } else if ( (ri2 != NULL && (strncasecmp(ri2, "set", 3) == 0) && (strncasecmp(ri2, "setpoint", 8) != 0)) ||
              (ri2 != NULL && ri3 != NULL && (strncasecmp(ri2, "timer", 5) == 0) && (strncasecmp(ri3, "set", 3) == 0)) ) {
    // Must be a switch on / off
    rtn = uActioned;
    found = false;
    //bool istimer = false;
    action_type atype = ON_OFF;
    //int timer=0;
    if (strncasecmp(ri2, "timer", 5) == 0) {
      //istimer = true;
      atype = TIMER;
      //timer = value; // Save off timer
      //value = 1; // Make sure we turn device on if timer.
    } else if ( value > 1 || value < 0) {
      LOG(NET_LOG,LOG_WARNING, "%s: URI %s has invalid value %.2f\n",actionName[from], URI, value);
      *rtnmsg = INVALID_VALUE;
      rtn = uBad;
      return rtn;
    }

    for (i=0; i < _aqualink_data->total_buttons && found==false; i++) {
      // If Label = "Spa", "Spa_Heater" will turn on "Spa", so need to check '/' on label as next character
      if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ||
          (strncmp(ri1, _aqualink_data->aqbuttons[i].label, strlen(_aqualink_data->aqbuttons[i].label)) == 0 && ri1[strlen(_aqualink_data->aqbuttons[i].label)] == '/'))
      {
        found = true;
        //create_panel_request(from, i, value, istimer);
        panel_device_request(_aqualink_data, atype, i, value, from);
        //LOG(NET_LOG,LOG_INFO, "%s: MATCH %s to topic %.*s\n",from,_aqualink_data->aqbuttons[i].name,uri_length, URI);
      }
    }
    if(!found) {
      *rtnmsg = NO_DEVICE;
      LOG(NET_LOG,LOG_WARNING, "%s: Didn't find device that matched URI '%.*s'\n",actionName[from], uri_length, URI);
      rtn = uBad;
    }
  } else {
    // We don's care
    *rtnmsg = UNKNOWN_REQUEST;
    LOG(NET_LOG,LOG_WARNING, "%s: ignoring, unknown URI %.*s\n",actionName[from],uri_length,URI);
    rtn = uBad;
  }

  return rtn;
}


void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  char *rtnmsg;
#ifdef AQ_TM_DEBUG
  int tid;
#endif
  //unsigned int i;
  //LOG(NET_LOG,LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  // If message doesn't end in set or increment we don't care about it.
  if (strncmp(&msg->topic.p[msg->topic.len -4], "/set", 4) != 0 && strncmp(&msg->topic.p[msg->topic.len -10], "/increment", 10) != 0) {
    LOG(NET_LOG,LOG_DEBUG, "MQTT: Ignore %.*s %.*s\n",msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
    return;
  }
  LOG(NET_LOG,LOG_DEBUG, "MQTT: topic %.*s %.*s\n",msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);

  DEBUG_TIMER_START(&tid);
  //Need to do this in a better manor, but for present it's ok.
  static char tmp[20];
  strncpy(tmp, msg->payload.p, msg->payload.len);
  tmp[msg->payload.len] = '\0';

  //float value = atof(tmp);

  // Check value like on/off/heat/cool and convery to int.
  // HASSIO doesn't support `mode_command_template` so easier to code around their limotation here.
  char *end;
  float value = strtof(tmp, &end);
  if (tmp == end) { // Not a number
    // See if any test resembeling 1, of not leave at zero.
    if (rsm_strcmp(tmp, "on")==0 || rsm_strcmp(tmp, "heat")==0 || rsm_strcmp(tmp, "cool")==0)
      value = 1;

    LOG(NET_LOG,LOG_NOTICE, "MQTT: converted value from '%s' to '%.0f', from message '%.*s'\n",tmp,value,msg->topic.len, msg->topic.p);
  } 


  //int val = _aqualink_data->unactioned.value = (_aqualink_data->temp_units != CELSIUS && _aqconfig_.convert_mqtt_temp) ? round(degCtoF(value)) : round(value);
  bool convert = (_aqualink_data->temp_units != CELSIUS && _aqconfig_.convert_mqtt_temp)?true:false;
  int offset = strlen(_aqconfig_.mqtt_aq_topic)+1;
  if ( action_URI(NET_MQTT, &msg->topic.p[offset], msg->topic.len - offset, value, convert, &rtnmsg) == uBad ) {
    // Check if it was something that can't be changed, if so send back current state.  Homekit thermostat for SWG and Freezeprotect.
    if (  strncmp(&msg->topic.p[offset], FREEZE_PROTECT, strlen(FREEZE_PROTECT)) == 0) {
      if (_aqualink_data->frz_protect_set_point != TEMP_UNKNOWN ) {
        send_mqtt_setpoint_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_set_point);
        send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_ON);
      } else {
        send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_OFF);
      }
      send_mqtt_string_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_state==ON?MQTT_ON:MQTT_OFF);
    } else if (  strncmp(&msg->topic.p[offset], SWG_TOPIC, strlen(SWG_TOPIC)) == 0) {
      if (_aqualink_data->swg_led_state != LED_S_UNKNOWN) {
        send_mqtt_swg_state_msg(nc, SWG_TOPIC, _aqualink_data->swg_led_state);
        send_mqtt_int_msg(nc, SWG_BOOST_TOPIC, _aqualink_data->boost);
      }
    }
  }

  DEBUG_TIMER_STOP(tid, NET_LOG, "action_mqtt_message() completed, took ");
}




float pass_mg_body(struct mg_str *body) {
  LOG(NET_LOG,LOG_INFO, "Message body:'%.*s'\n", body->len, body->p);
  // Quick n dirty pass value from either of below.
  // value=1.5&arg2=val2
  // {"value":"1.5"}
  int i;
  char buf[10];
  
  // NSF Really need to come back and clean this up

  for (i=0; i < body->len; i++) {
    if ( body->p[i] == '=' || body->p[i] == ':' ) {
      while (!isdigit((unsigned char) body->p[i]) && body->p[i] != '-' && i < body->len) {i++;}
      if(i < body->len) {
        // Need to copy to buffer so we can terminate correctly.
        strncpy(buf, &body->p[i], body->len - i);
        buf[body->len - i] = '\0';
        //printf ("RETURN\n");
        //return atof(&body->p[i]);
        return atof(buf);
      }
    }
  }
  //printf ("RETURN UNKNOWN\n");
  return TEMP_UNKNOWN;
}



void action_web_request(struct mg_connection *nc, struct http_message *http_msg) {
  char *msg = NULL;
  // struct http_message *http_msg = (struct http_message *)ev_data;
#ifdef AQ_TM_DEBUG
  int tid;
  int tid2;
#endif

  //DEBUG_TIMER_START(&tid);
  if (getLogLevel(NET_LOG) >= LOG_INFO) { // Simply for log message, check we are at
                                   // this log level before running all this
                                   // junk
    char *uri = (char *)malloc(http_msg->uri.len + http_msg->query_string.len + 2);
    strncpy(uri, http_msg->uri.p, http_msg->uri.len + http_msg->query_string.len + 1);
    uri[http_msg->uri.len + http_msg->query_string.len + 1] = '\0';
    LOG(NET_LOG,LOG_INFO, "URI request: '%s'\n", uri);
    free(uri);
  }
  //DEBUG_TIMER_STOP(tid, NET_LOG, "action_web_request debug print crap took"); 

  //LOG(NET_LOG,LOG_INFO, "Message request:\n'%.*s'\n", http_msg->message.len, http_msg->message.p);

  // If we have a get request, pass it
  if (strncmp(http_msg->uri.p, "/api", 4 ) != 0) {
    if (strstr(http_msg->method.p, "GET") && http_msg->query_string.len > 0) {
      LOG(NET_LOG,LOG_ERR, "WEB: Old API stanza requested, ignoring client request\n");
    } else {
      DEBUG_TIMER_START(&tid);
      mg_serve_http(nc, http_msg, _http_server_opts);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_web_request() serve file took");
    }
  //} else if (strstr(http_msg->method.p, "PUT")) {
  } else {
    char buf[JSON_BUFFER_SIZE];
    float value = 0;
    DEBUG_TIMER_START(&tid);

    // If query string.
    if (http_msg->query_string.len > 1) {
      mg_get_http_var(&http_msg->query_string, "value", buf, sizeof(buf));
      value = atof(buf);
    } else if (http_msg->body.len > 1) {
      value = pass_mg_body(&http_msg->body);
    }
    
    int len = mg_url_decode(http_msg->uri.p, http_msg->uri.len, buf, 50, 0);
    
    if (strncmp(http_msg->uri.p, "/api/",4) == 0) {
      switch (action_URI(NET_API, &buf[5], len-5, value, false, &msg)) {
        case uActioned:
          mg_send_head(nc, 200, strlen(GET_RTN_OK), CONTENT_TEXT);
          mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
        break;
        case uDevices:
        {
          char message[JSON_BUFFER_SIZE];
          DEBUG_TIMER_START(&tid2);
          int size = build_device_JSON(_aqualink_data, message, JSON_BUFFER_SIZE, false);
          DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_device_JSON took");
          mg_send_head(nc, 200, size, CONTENT_JSON);
          mg_send(nc, message, size);
        }
        break;
        case uHomebridge:
        {
          char message[JSON_BUFFER_SIZE];
          int size = build_device_JSON(_aqualink_data, message, JSON_BUFFER_SIZE, true);
          mg_send_head(nc, 200, size, CONTENT_JSON);
          mg_send(nc, message, size);
        }
        break;
        case uStatus:
        {
          char message[JSON_BUFFER_SIZE];
          DEBUG_TIMER_START(&tid2);
          int size = build_aqualink_status_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
          DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_aqualink_status_JSON took");
          mg_send_head(nc, 200, size, CONTENT_JSON);
          mg_send(nc, message, size);
        }
        break;
        case uDynamicconf:
        {
          char message[JSON_BUFFER_SIZE];
          DEBUG_TIMER_START(&tid2);
          int size = build_webconfig_js(_aqualink_data, message, JSON_BUFFER_SIZE);
          DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_webconfig_js took");
          mg_send_head(nc, 200, size, CONTENT_JS);
          mg_send(nc, message, size); 
        }
        break;
        case uSchedules:
        {
          char message[JSON_BUFFER_SIZE];
          DEBUG_TIMER_START(&tid2);
          int size = build_schedules_js(message, JSON_BUFFER_SIZE);
          DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_schedules_js took");
          mg_send_head(nc, 200, size, CONTENT_JS);
          mg_send(nc, message, size); 
        }
        break;
        case uSetSchedules:
        {
          char message[JSON_BUFFER_SIZE];
          DEBUG_TIMER_START(&tid2);
          //int size = save_schedules_js(_aqualink_data, &http_msg->body, message, JSON_BUFFER_SIZE);
          int size = save_schedules_js((char *)&http_msg->body, http_msg->body.len, message, JSON_BUFFER_SIZE);
          DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() save_schedules_js took");
          mg_send_head(nc, 200, size, CONTENT_JS);
          mg_send(nc, message, size); 
        }
        break;
#ifndef AQ_MANAGER
        case uDebugStatus:
        {
          char message[JSON_BUFFER_SIZE];
          int size = snprintf(message,80,"{\"sLevel\":\"%s\", \"iLevel\":%d, \"logReady\":\"%s\"}\n",elevel2text(getLogLevel(NET_LOG)),getLogLevel(NET_LOG),islogFileReady()?"true":"false" );
          mg_send_head(nc, 200, size, CONTENT_JS);
          mg_send(nc, message, size);
        }
        break;
#else
        case uLogDownload:
          //int lines = 1000;
          #define DEFAULT_LOG_DOWNLOAD_LINES 100
          // If lines was passed in post use it, if not see if it's next path in URI is a number
          if (value == 0.0) {
            // /api/<downloadmsg>/<lines>
            char *pt = rsm_lastindexof(buf, "/", strlen(buf));
            value = atoi(pt+1);
          }
          LOG(NET_LOG, LOG_DEBUG, "Downloading log of max %d lines\n",value>0?(int)value:DEFAULT_LOG_DOWNLOAD_LINES);
          if (write_systemd_logmessages_2file("/dev/shm/aqualinkd.log", value>0?(int)value:DEFAULT_LOG_DOWNLOAD_LINES) ) {
            mg_http_serve_file(nc, http_msg, "/dev/shm/aqualinkd.log", mg_mk_str("text/plain"), mg_mk_str("Content-Disposition: attachment; filename=\"aqualinkd.log\""));
            remove("/dev/shm/aqualinkd.log");
          }
        break;
#endif
        case uBad:
        default:
          if (msg == NULL) {
            mg_send_head(nc, 400, strlen(GET_RTN_UNKNOWN), CONTENT_TEXT);
            mg_send(nc, GET_RTN_UNKNOWN, strlen(GET_RTN_UNKNOWN));
          } else {
            mg_send_head(nc, 400, strlen(msg), CONTENT_TEXT);
            mg_send(nc, msg, strlen(msg));
          }
        break;
      }
    } else {
      mg_send_head(nc, 200, strlen(GET_RTN_UNKNOWN), CONTENT_TEXT);
      mg_send(nc, GET_RTN_UNKNOWN, strlen(GET_RTN_UNKNOWN));
    }

    sprintf(buf, "action_web_request() request '%.*s' took",(int)http_msg->uri.len, http_msg->uri.p);

    DEBUG_TIMER_STOP(tid, NET_LOG, buf);
  }
}


void action_websocket_request(struct mg_connection *nc, struct websocket_message *wm) {
  char buffer[100];
  struct JSONkvptr jsonkv;
  int i;
  char *uri = NULL;
  char *value = NULL;
  char *msg = NULL;
#ifdef AQ_TM_DEBUG
  int tid;
#endif
#ifdef AQ_PDA
  // Any websocket request means UI is active, so don't let AqualinkD go to sleep if in PDA mode
  if (isPDA_PANEL)
    pda_reset_sleep();
#endif
   
  strncpy(buffer, (char *)wm->data, wm->size);
  buffer[wm->size] = '\0';

  parseJSONrequest(buffer, &jsonkv);

  for(i=0; i < 4; i++) {
    if (jsonkv.kv[i].key != NULL)
      LOG(NET_LOG,LOG_DEBUG, "WS: Message - Key '%s' Value '%s'\n",jsonkv.kv[i].key,jsonkv.kv[i].value);
    
    if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "uri", 3) == 0)
      uri = jsonkv.kv[i].value;
    else if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "value", 4) == 0)
      value = jsonkv.kv[i].value;
  }
  
  if (uri == NULL) {
    LOG(NET_LOG,LOG_ERR, "WEB: Old websocket stanza requested, ignoring client request\n");
    return;
  }

  switch ( action_URI(NET_WS, uri, strlen(uri), (value!=NULL?atof(value):TEMP_UNKNOWN), false, &msg)) {
    case uActioned:
      sprintf(buffer, "{\"message\":\"ok\"}");
      ws_send(nc, buffer);
    break;
    case uDevices:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      build_device_JSON(_aqualink_data, message, JSON_BUFFER_SIZE, false);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_device_JSON took");
      ws_send(nc, message);
    }
    break;
    case uStatus:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      build_aqualink_status_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_aqualink_status_JSON took");
      ws_send(nc, message);
    }
    break;
    case uSimulator:
    {
      LOG(NET_LOG,LOG_DEBUG, "Request to start Simulator\n");
      set_websocket_simulator(nc);
      //_aqualink_data->simulate_panel = true;
      // Clear simulator ID incase sim type changes
      //_aqualink_data->simulator_id = NUL;
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      build_aqualink_status_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_aqualink_status_JSON took");
      ws_send(nc, message);
    }
    break;
    case uAQmanager:
    {
      LOG(NET_LOG,LOG_DEBUG, "Started AqualinkD Manager\n");
      set_websocket_aqmanager(nc);
      _aqualink_data->aqManagerActive = true;
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      //build_aqualink_status_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
      build_aqualink_aqmanager_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_aqualink_status_JSON took");
      ws_send(nc, message);
    }
    break;
    case uNotAvailable:
    {
      sprintf(buffer, "{\"na_message\":\"not available in this version!\"}");
       ws_send(nc, buffer);
    }
    case uSchedules:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      build_schedules_js(message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_schedules_js took");
      ws_send(nc, message);
    }
    break;
    case uSetSchedules:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      save_schedules_js((char *)wm->data, wm->size, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() save_schedules_js took");
      ws_send(nc, message); 
    }
    case uBad:
    default:
      if (msg == NULL)
        sprintf(buffer, "{\"message\":\"Bad request\"}");
      else
        sprintf(buffer, "{\"message\":\"%s\"}",msg);
      ws_send(nc, buffer);
    break;
  }
}

void action_domoticz_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  int idx = -1;
  int nvalue = -1;
  int i;
  char svalue[DZ_SVALUE_LEN+1];

  if (parseJSONmqttrequest(msg->payload.p, msg->payload.len, &idx, &nvalue, svalue) && idx > 0) {
    for (i=0; i < _aqualink_data->total_buttons; i++) {
      if (_aqualink_data->aqbuttons[i].dz_idx == idx){
        LOG(NET_LOG,LOG_DEBUG, "MQTT: DZ: Received message IDX=%d nValue=%d sValue=%s\n", idx, nvalue, svalue);
        //NSF, should try to simplify this if statment, but not easy since AQ ON and DZ ON are different, and AQ has other states.
        if ( (_aqualink_data->aqbuttons[i].led->state == OFF && nvalue==DZ_OFF) ||
           (nvalue == DZ_ON && (_aqualink_data->aqbuttons[i].led->state == ON || 
                                _aqualink_data->aqbuttons[i].led->state == FLASH || 
                                _aqualink_data->aqbuttons[i].led->state == ENABLE))) {
          LOG(NET_LOG,LOG_INFO, "MQTT: DZ: received '%s' for '%s', already '%s', Ignoring\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name, (nvalue==DZ_OFF?"OFF":"ON"));
        } else {
          // NSF Below if needs to check that the button pressed is actually a light. Add this later
          if (_aqualink_data->active_thread.ptype == AQ_SET_LIGHTPROGRAM_MODE ) {
            LOG(NET_LOG,LOG_NOTICE, "MQTT: DZ: received '%s' for '%s', IGNORING as we are programming light mode\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name);
          } else {
            LOG(NET_LOG,LOG_INFO, "MQTT: DZ: received '%s' for '%s', turning '%s'\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name,(nvalue==DZ_OFF?"OFF":"ON"));
            //create_panel_request(NET_DZMQTT, i, (nvalue == DZ_OFF?0:1), false);
            panel_device_request(_aqualink_data, ON_OFF, i, (nvalue == DZ_OFF?0:1), NET_DZMQTT);
          }
        }
        break; // no need to continue in for loop, we found button.
      }
    }
  }

  // NSF Need to check idx against ours and decide if we are interested in it.
}





static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  struct http_message *http_msg;
  struct websocket_message *ws_msg;
  char aq_topic[30];
  #ifdef AQ_TM_DEBUG 
    int tid; 
  #endif
  //static double last_control_time;

  // LOG(NET_LOG,LOG_DEBUG, "Event\n");
  switch (ev) {
  case MG_EV_HTTP_REQUEST:
    //nc->user_data = WEB;
    http_msg = (struct http_message *)ev_data;
    DEBUG_TIMER_START(&tid); 
    action_web_request(nc, http_msg);
    DEBUG_TIMER_STOP(tid, NET_LOG, "WEB Request action_web_request() took"); 
    LOG(NET_LOG,LOG_DEBUG, "Served WEB request\n");
    break;
  
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    //nc->user_data = WS;
    _aqualink_data->open_websockets++;
    LOG(NET_LOG,LOG_DEBUG, "++ Websocket joined\n");
    break;
  
  case MG_EV_WEBSOCKET_FRAME: 
    ws_msg = (struct websocket_message *)ev_data;
    DEBUG_TIMER_START(&tid); 
    action_websocket_request(nc, ws_msg);
    DEBUG_TIMER_STOP(tid, NET_LOG, "Websocket Request action_websocket_request() took"); 
    break;
  
  case MG_EV_CLOSE: 
    if (is_websocket(nc)) {
      _aqualink_data->open_websockets--;
      LOG(NET_LOG,LOG_DEBUG, "-- Websocket left\n");
      if (is_websocket_simulator(nc)) {
        stop_simulator(_aqualink_data);
        LOG(NET_LOG,LOG_DEBUG, "Stoped Simulator Mode\n");
      } else if (is_websocket_aqmanager(nc)) {
        _aqualink_data->aqManagerActive = false;
        LOG(NET_LOG,LOG_DEBUG, "Stoped Aqualink Manager\n");
      }
    } else if (is_mqtt(nc)) {
      LOG(NET_LOG,LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_exit_flag = true;
    }
    break;
  
  case MG_EV_CONNECT: {
    //nc->user_data = MQTT;
    //nc->flags |= MG_F_USER_1; // NFS Need to readup on this
    set_mqtt(nc);
    _mqtt_exit_flag = false;
    //char *MQTT_id = "AQUALINK_MQTT_TEST_ID";
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user_name = _aqconfig_.mqtt_user;
    opts.password = _aqconfig_.mqtt_passwd;
    opts.keep_alive = 5;
    opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this

    snprintf(aq_topic, 24, "%s/%s", _aqconfig_.mqtt_aq_topic,MQTT_LWM_TOPIC);
    opts.will_topic = aq_topic;
    opts.will_message = MQTT_OFF;

    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc, _aqconfig_.mqtt_ID, opts);
    LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing mqtt with id of: %s\n", _aqconfig_.mqtt_ID);
    //last_control_time = mg_time();
  } break;

  case MG_EV_MQTT_CONNACK:
    {
      struct mg_mqtt_topic_expression topics[2];
      
      int qos=0;// can't be bothered with ack, so set to 0

      LOG(NET_LOG,LOG_DEBUG, "MQTT: Connection acknowledged\n");
      mqtt_msg = (struct mg_mqtt_message *)ev_data;
      if (mqtt_msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        LOG(NET_LOG,LOG_WARNING, "Got mqtt connection error: %d\n", mqtt_msg->connack_ret_code);
        _mqtt_exit_flag = true;
      }
      
      snprintf(aq_topic, 29, "%s/#", _aqconfig_.mqtt_aq_topic);
      if (_aqconfig_.mqtt_aq_topic != NULL && _aqconfig_.mqtt_dz_sub_topic != NULL) {
        topics[0].topic = aq_topic;
        topics[0].qos = qos; 
        topics[1].topic = _aqconfig_.mqtt_dz_sub_topic;
        topics[1].qos = qos;
        mg_mqtt_subscribe(nc, topics, 2, 42);
        LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
        LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing to '%s'\n", _aqconfig_.mqtt_dz_sub_topic);
      } 
      else if (_aqconfig_.mqtt_aq_topic != NULL) {
        topics[0].topic = aq_topic;
        topics[0].qos = qos;
        mg_mqtt_subscribe(nc, topics, 1, 42);
        LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
      } 
      else if (_aqconfig_.mqtt_dz_sub_topic != NULL) {
        topics[0].topic = _aqconfig_.mqtt_dz_sub_topic;;
        topics[0].qos = qos;
        mg_mqtt_subscribe(nc, topics, 1, 42);
        LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing to '%s'\n", _aqconfig_.mqtt_dz_sub_topic);
      }

      publish_mqtt_hassio_discover( _aqualink_data, nc);
    }
    break;
  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    LOG(NET_LOG,LOG_DEBUG, "MQTT: Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    LOG(NET_LOG,LOG_INFO, "MQTT: Subscription(s) acknowledged\n");
    snprintf(aq_topic, 24, "%s/%s", _aqconfig_.mqtt_aq_topic,MQTT_LWM_TOPIC);
    send_mqtt(nc, aq_topic ,MQTT_ON);
    break;
  case MG_EV_MQTT_PUBLISH: 
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    
    if (mqtt_msg->message_id != 0) {
      LOG(NET_LOG,LOG_DEBUG, "MQTT: received (msg_id: %d), looks like my own message, ignoring\n", mqtt_msg->message_id);
    }
// NSF Need to change strlen to a global so it's not executed every time we check a topic
    if (_aqconfig_.mqtt_aq_topic != NULL && strncmp(mqtt_msg->topic.p, _aqconfig_.mqtt_aq_topic, strlen(_aqconfig_.mqtt_aq_topic)) == 0) 
    {
      DEBUG_TIMER_START(&tid); 
      action_mqtt_message(nc, mqtt_msg);
      DEBUG_TIMER_STOP(tid, NET_LOG, "MQTT Request action_mqtt_message() took"); 
    }
    if (_aqconfig_.mqtt_dz_sub_topic != NULL && strncmp(mqtt_msg->topic.p, _aqconfig_.mqtt_dz_sub_topic, strlen(_aqconfig_.mqtt_dz_sub_topic)) == 0) {
      action_domoticz_mqtt_message(nc, mqtt_msg);
    }
    break;
  }
}

void reset_last_mqtt_status()
{
  int i;
  memset(&_last_mqtt_aqualinkdata, 0, sizeof(_last_mqtt_aqualinkdata));

  for (i=0; i < _aqualink_data->total_buttons; i++) {
    _last_mqtt_aqualinkdata.aqualinkleds[i].state = LED_S_UNKNOWN;
  }
  _last_mqtt_aqualinkdata.ar_swg_device_status = SWG_STATUS_UNKNOWN;
  _last_mqtt_aqualinkdata.swg_led_state = LED_S_UNKNOWN;
  _last_mqtt_aqualinkdata.air_temp = TEMP_REFRESH;
  _last_mqtt_aqualinkdata.pool_temp = TEMP_REFRESH;
  _last_mqtt_aqualinkdata.spa_temp = TEMP_REFRESH;
  //_last_mqtt_aqualinkdata.sw .ar_swg_device_status = SWG_STATUS_UNKNOWN;
  _last_mqtt_aqualinkdata.battery = -1;
  _last_mqtt_aqualinkdata.frz_protect_state = -1;
  _last_mqtt_aqualinkdata.service_mode_state = -1;
  _last_mqtt_aqualinkdata.pool_htr_set_point = TEMP_REFRESH;
  _last_mqtt_aqualinkdata.spa_htr_set_point = TEMP_REFRESH;
  _last_mqtt_aqualinkdata.ph = -1;
  _last_mqtt_aqualinkdata.orp = -1;
  _last_mqtt_aqualinkdata.boost = -1;
  _last_mqtt_aqualinkdata.swg_percent = -1;
  _last_mqtt_aqualinkdata.swg_ppm = -1;
  _last_mqtt_aqualinkdata.heater_err_status = NUL; // 0x00

  for (i=0; i < _aqualink_data->num_pumps; i++) {
    _last_mqtt_aqualinkdata.pumps[i].gpm = TEMP_UNKNOWN;
    _last_mqtt_aqualinkdata.pumps[i].rpm = TEMP_UNKNOWN;
    _last_mqtt_aqualinkdata.pumps[i].watts = TEMP_UNKNOWN;
    _last_mqtt_aqualinkdata.pumps[i].mode = TEMP_UNKNOWN;
    _last_mqtt_aqualinkdata.pumps[i].status = TEMP_UNKNOWN;
    _last_mqtt_aqualinkdata.pumps[i].pressureCurve = TEMP_UNKNOWN;
    //_last_mqtt_aqualinkdata.pumps[i].driveState = TEMP_UNKNOWN;
  }

}

void start_mqtt(struct mg_mgr *mgr) {
  LOG(NET_LOG,LOG_NOTICE, "Starting MQTT client to %s\n", _aqconfig_.mqtt_server);
  if ( _aqconfig_.mqtt_server == NULL || 
      ( _aqconfig_.mqtt_aq_topic == NULL && _aqconfig_.mqtt_dz_pub_topic == NULL && _aqconfig_.mqtt_dz_sub_topic == NULL) )
    return;

  if (mg_connect(mgr, _aqconfig_.mqtt_server, ev_handler) == NULL) {
      LOG(NET_LOG,LOG_ERR, "Failed to create MQTT listener to %s\n", _aqconfig_.mqtt_server);
  } else {
    //int i;
#ifdef AQ_MEMCMP
    memset(&_last_mqtt_aqualinkdata, 0, sizeof (struct aqualinkdata));
#endif
    reset_last_mqtt_status();
    _mqtt_exit_flag = false; // set here to stop multiple connects, if it fails truley fails it will get set to false.
  }
}

//bool start_web_server(struct mg_mgr *mgr, struct aqualinkdata *aqdata, char *port, char* web_root) {
//bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata, struct aqconfig *aqconfig) {
bool _start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata) {
  struct mg_connection *nc;
  _aqualink_data = aqdata;
  //_aqconfig_ = aqconfig;
 
  signal(SIGTERM, net_signal_handler);
  signal(SIGINT, net_signal_handler);
  signal(SIGRESTART, net_signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);
  
  mg_mgr_init(mgr, NULL);
  LOG(NET_LOG,LOG_NOTICE, "Starting web server on port %s\n", _aqconfig_.socket_port);
  nc = mg_bind(mgr, _aqconfig_.socket_port, ev_handler);
  if (nc == NULL) {
    LOG(NET_LOG,LOG_ERR, "Failed to create listener on port %s\n",_aqconfig_.socket_port);
    return false;
  }

  // Set up HTTP server parameters
  mg_set_protocol_http_websocket(nc);
  
  // Set default web options
  //struct mg_serve_http_opts opts;
      //memset(&opts, 0, sizeof(opts)); // Reset all options to defaults
      //opts.document_root = _web_root; // Serve files from the current directory
      //opts.extra_headers = "Cache-Control: public, max-age=604800, immutable";

  memset(&_http_server_opts, 0, sizeof(_http_server_opts)); // Reset all options to defaults
  _http_server_opts.document_root = _aqconfig_.web_directory;  // Serve current directory
  _http_server_opts.enable_directory_listing = "yes";
  //_http_server_opts.extra_headers = "Cache-Control: public, max-age=604800, immutable";
  _http_server_opts.extra_headers = "Cache-Control: public,max-age=31536000,immutable"; // Let's be as agressive on browser caching.
  
#ifndef MG_DISABLE_MQTT
  // Start MQTT
  start_mqtt(mgr);
#endif


  return true;
}


/**********************************************************************************************
 * Thread Net Services
 * 
*/

//volatile bool _broadcast = false; // This is redundent when most the fully threadded rather than option.

void *net_services_thread( void *ptr )
{
  struct aqualinkdata *aqdata = (struct aqualinkdata *) ptr;
  //struct mg_mgr mgr;
  if (!_start_net_services(&_mgr, aqdata)) {
    //LOG(NET_LOG,LOG_ERR, "Failed to start network services\n");
    // Not the best way to do this (have thread exit process), but forks for the moment.
    _keepNetServicesRunning = false;
    LOG(AQUA_LOG,LOG_ERR, "Can not start webserver on port %s.\n", _aqconfig_.socket_port);
    exit(EXIT_FAILURE);
    goto f_end;
  }

  while (_keepNetServicesRunning == true)
  {
    //poll_net_services(&_mgr, 10);
    // Shorten poll cycle when in simulator mode
    mg_mgr_poll(&_mgr, (_aqualink_data->simulator_active != SIM_NONE)?10:100);
    //mg_mgr_poll(&_mgr, 100);

    if (aqdata->updated == true /*|| _broadcast == true*/) {
      //LOG(NET_LOG,LOG_DEBUG, "********** Broadcast ************\n");
      _broadcast_aqualinkstate(_mgr.active_connections);
      aqdata->updated = false;
    }
#ifdef AQ_MANAGER
    if ( ! broadcast_systemd_logmessages(aqdata->aqManagerActive)) {
      LOG(AQUA_LOG,LOG_ERR, "Couldn't open systemd journal log\n");
    }
#endif
    if (aqdata->simulator_active != SIM_NONE && aqdata->simulator_packet_updated == true ) {
      _broadcast_simulator_message(_mgr.active_connections);
    } 
  }

f_end:
  LOG(NET_LOG,LOG_NOTICE, "Stopping network services thread\n");
  mg_mgr_free(&_mgr);

  pthread_exit(0);
}




#ifndef AQ_NO_THREAD_NETSERVICE


void broadcast_aqualinkstate() {
  _aqualink_data->updated = true;
}
void broadcast_aqualinkstate_error(char *msg) {
  _broadcast_aqualinkstate_error(_mgr.active_connections, msg);
}
void broadcast_simulator_message() {
  _aqualink_data->simulator_packet_updated = true;
}


void stop_net_services() {
  _keepNetServicesRunning = false;
  return;
}

bool start_net_services(struct aqualinkdata *aqdata) 
{
  // Not the best way to see if we are running, but works for now.
  if (_net_thread_id != 0 && _keepNetServicesRunning) {
    LOG(NET_LOG,LOG_NOTICE, "Network services thread is already running, not starting\n");
    return true;
  }

  _keepNetServicesRunning = true;

  LOG(NET_LOG,LOG_NOTICE, "Starting network services thread\n");

  if( pthread_create( &_net_thread_id , NULL ,  net_services_thread, (void*)aqdata) < 0) {
    LOG(NET_LOG, LOG_ERR, "could not create network thread\n");
    return false;
  }

  pthread_detach(_net_thread_id);

  return true;
}

#else // DON'T THREAD NET SERVICES

void stop_net_services() {
  if ( ! _aqconfig_.thread_netservices) {
    mg_mgr_free(&_mgr);
    return;
  }
}
void broadcast_aqualinkstate(/*struct mg_connection *nc*/)
{
  if ( ! _aqconfig_.thread_netservices) {
    _broadcast_aqualinkstate(_mgr.active_connections);
    _aqualink_data->updated = false;
    return;
  }
}
void broadcast_aqualinkstate_error(/*struct mg_connection *nc,*/ char *msg)
{
  if ( ! _aqconfig_.thread_netservices) {
    return _broadcast_aqualinkstate_error(_mgr.active_connections, msg);
  }
  LOG(NET_LOG,LOG_NOTICE, "Broadcast error to network\n");
}
void broadcast_simulator_message() {
  if ( ! _aqconfig_.thread_netservices) {
    return _broadcast_simulator_message();
  }
}
time_t poll_net_services(/*struct mg_mgr *mgr,*/ int timeout_ms) 
{
  if (timeout_ms < 0)
    timeout_ms = 0;

  if ( ! _aqconfig_.thread_netservices) {
    //return mg_mgr_poll(mgr, timeout_ms);
    return mg_mgr_poll(&_mgr, timeout_ms);
  }
  
  if (timeout_ms > 5)
    delay(5);
  else if (timeout_ms > 0)
    delay(timeout_ms);

  //LOG(NET_LOG,LOG_NOTICE, "Poll network services\n");

  return 0;
}
bool start_net_services(/*struct mg_mgr *mgr, */struct aqualinkdata *aqdata) 
{
  _keepNetServicesRunning = true;

  if ( ! _aqconfig_.thread_netservices) {
    //return _start_net_services(mgr, aqdata);
    return _start_net_services(&_mgr, aqdata);
  }
  
  LOG(NET_LOG,LOG_NOTICE, "Starting network services thread\n");

  if( pthread_create( &_net_thread_id , NULL ,  net_services_thread, (void*)aqdata) < 0) {
    LOG(NET_LOG, LOG_ERR, "could not create network thread\n");
    return false;
  }

  pthread_detach(_net_thread_id);

  return true;
}

#endif