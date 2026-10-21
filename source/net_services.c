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
#include "aq_mqtt.h"
#include "devices_jandy.h"
#include "web_config.h"
#include "debug_timer.h"
#include "serialadapter.h"
#include "aq_timer.h"
#include "aq_scheduler.h"
#include "rs_msg_utils.h"
#include "simulator.h"
#include "mqtt_discovery.h"
#include "version.h"
#include "color_lights.h"
#include "net_interface.h"
#include "aq_systemutils.h"

#ifdef AQ_PDA
#include "pda.h"
#endif

struct mg_connection *mg_next(struct mg_mgr *s, struct mg_connection *conn) {
  return conn == NULL ? s->conns : conn->next;
}


#define FAST_SUFFIX_3_CI(str, len, SUFFIX) ( \
    (len) >= 3 && \
    tolower((unsigned char)(str)[(len)-3]) == tolower((unsigned char)(SUFFIX)[0]) && \
    tolower((unsigned char)(str)[(len)-2]) == tolower((unsigned char)(SUFFIX)[1]) && \
    tolower((unsigned char)(str)[(len)-1]) == tolower((unsigned char)(SUFFIX)[2]) \
)

#define FAST_SUFFIX_4_CI(str, len, SUFFIX) ( \
    (len) >= 4 && \
    tolower((unsigned char)(str)[(len)-4]) == tolower((unsigned char)(SUFFIX)[0]) && \
    tolower((unsigned char)(str)[(len)-3]) == tolower((unsigned char)(SUFFIX)[1]) && \
    tolower((unsigned char)(str)[(len)-2]) == tolower((unsigned char)(SUFFIX)[2]) && \
    tolower((unsigned char)(str)[(len)-1]) == tolower((unsigned char)(SUFFIX)[3]) \
)


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


void start_mqtt(struct mg_mgr *mgr);
static struct aqualinkdata _last_mqtt_aqualinkdata;
static aqled _last_mqtt_chiller_led;
void mqtt_broadcast_aqualinkstate(struct mg_connection *nc);


void reset_last_mqtt_status();
bool uri_strcmp(const char *uri, const char *string);

//static const char *s_http_port = "8080";
//static struct mg_serve_http_opts _http_server_opts;
static struct mg_http_serve_opts _http_server_opts;
static struct mg_http_serve_opts _http_server_opts_nocache;

static void net_signal_handler(int sig_num) {
  intHandler(sig_num); // Force signal handler to aqualinkd.c
}


static int is_websocket(const struct mg_connection *nc) {
  //return nc->flags & MG_F_IS_WEBSOCKET && !(nc->flags & MG_F_USER_2); // WS only, not WS simulator
  //return nc->flags & MG_F_IS_WEBSOCKET;
  return nc->is_websocket;
}
static void set_websocket_simulator(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_WS_SIM; 
}
static int is_websocket_simulator(const struct mg_connection *nc) {
  return nc->aq_flags & AQ_MG_CON_WS_SIM;
}
static void set_websocket_aqmanager(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_WS_AQM; 
}
static int is_websocket_aqmanager(const struct mg_connection *nc) {
  return nc->aq_flags & AQ_MG_CON_WS_AQM;
}
static int is_mqtt(const struct mg_connection *nc) {
  //return nc->aq_flags & AQ_MG_CON_MQTT;
  return nc->aq_flags & (AQ_MG_CON_MQTT | AQ_MG_CON_MQTT_CONNECTING);
}
/*
static void set_mqtt(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_MQTT; 
}*/
static int is_mqttconnecting(const struct mg_connection *nc) {
  return nc->aq_flags & AQ_MG_CON_MQTT_CONNECTING;
}
static void set_mqttconnecting(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_MQTT_CONNECTING; 
}
static void set_mqttconnected(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_MQTT;
  nc->aq_flags &= ~AQ_MG_CON_MQTT_CONNECTING;
}

static void ws_send(struct mg_connection *nc, char *msg)
{
  int size = strlen(msg);
  
  mg_ws_send(nc, msg, size, WEBSOCKET_OP_TEXT);
  
  //LOG(NET_LOG,LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

void _broadcast_aqualinkstate_error(struct mg_connection *nc, const char *msg) 
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

#define WS_LOG_LENGTH 400
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
  sd_journal *journal; // should be static??????
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
    return NULL;
  }
  snprintf(filter, 50, "SYSLOG_IDENTIFIER=%s",_aqualink_data->self );
  if (sd_journal_add_match(journal, filter, 0) < 0)
  {
    LOGSystemError(errno, NET_LOG, "Failed to set journal syslog filter");
    sd_journal_close(journal);
    return NULL;
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

void find_aqualinkd_startupmsg(sd_journal *journal, int fallbacklines)
{
  static bool once=false;
  const void *log;
  size_t len;

  if (fallbacklines == 0) {
    return;
  }

  // Only going to do this one time, incase re reset while reading.
  if (once) {
    // Simply go back number of lines since we have already gone back to startup message
    sd_journal_previous_skip(journal, fallbacklines);
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
      free(cursor);  // sd_journal_get_cursor() allocates, caller must free
      cursor = NULL;
      return true;
    }
  } 
  // aqManager is active
  if (!active) {
      if ( (journal = open_journal()) == NULL) {
  //printf("Open faied\n");
        build_logmsg_JSON(msg, LOG_ERR, "Failed to open journal", WS_LOG_LENGTH,22);
        ws_send_logmsg(_mgr.conns, msg);
        return false;
      }
  //printf("Open good %d\n",journal);
      if (sd_journal_seek_tail(journal) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to seek to journal end", WS_LOG_LENGTH,29);
        ws_send_logmsg(_mgr.conns, msg);
        sd_journal_close(journal);
        return false;
      }
      //if we have cusror go to it, otherwise jump back and try to find startup message
      if (cursor != NULL) {
        sd_journal_seek_cursor(journal, cursor);
        sd_journal_next(journal);
      } else {
        find_aqualinkd_startupmsg(journal, (reOpenStaleConnection?0:10) );
      }

      active = true;
  }

  const void *log;
  size_t len;
  const void *pri;
  size_t plen;
  int rtn;

  while ( (rtn = sd_journal_next(journal)) > 0)
  {
    if (sd_journal_get_data(journal, "MESSAGE", &log, &len) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to get journal message", WS_LOG_LENGTH,29);
        ws_send_logmsg(_mgr.conns, msg);
    } else if (sd_journal_get_data(journal, "PRIORITY", &pri, &plen) < 0) {
        build_logmsg_JSON(msg, LOG_ERR, "Failed to seek to journal message priority", WS_LOG_LENGTH,42);
        ws_send_logmsg(_mgr.conns, msg);
    } else {
        build_logmsg_JSON(msg, atoi((const char *)pri+9), (const char *)log+8, WS_LOG_LENGTH,(int)len-8);
        ws_send_logmsg(_mgr.conns, msg);
        cnt=0;
        // sd_journal_get_cursor() returns a newly malloc'd string on every call. Free the
        // previous one, otherwise every log line sent to aqmanager leaks ~150 bytes.
        char *new_cursor = NULL;
        if (sd_journal_get_cursor(journal, &new_cursor) >= 0) {
          free(cursor);
          cursor = new_cursor;
        }
      }
    }
  }
  if (rtn < 0) {
    build_logmsg_JSON(msg, LOG_ERR, "Failed to seek to next journal message", WS_LOG_LENGTH,42);
    ws_send_logmsg(_mgr.conns, msg);
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
      cnt = 0;  // Reset this so we don't keep hitting this when we don't print the message above.
      
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
   if ( (journal = open_journal()) == NULL) {
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
  
  if (_mqtt_exit_flag == true) {
    mqtt_count++;
    //if (mqtt_count >= 10) {
    if (mqtt_count >= 1) { // changed to 1 when cleaned up all the MQTT spaming.  may want to delete count completely in the future
      start_mqtt(nc->mgr);
      mqtt_count = 0;
    }
  }

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    //if (is_websocket(c) && !is_websocket_simulator(c)) // No need to broadcast status messages to simulator.
    if (is_websocket(c)) // All button simulator needs status messages
      ws_send(c, data);
    else if (is_mqtt(c))
      mqtt_broadcast_aqualinkstate(c);

  }

  DEBUG_TIMER_STOP(tid, NET_LOG, "broadcast_aqualinkstate() completed, took ");

  return;
}


void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message)
{
  //static uint16_t msg_id = 0;

  if (toppic == NULL)
    return;

  //if (msg_id >= 65535){msg_id=1;}else{msg_id++;}

  //mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_QOS(0), message, strlen(message));
  //mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_RETAIN | MG_MQTT_QOS(1), message, strlen(message));
  
  struct mg_mqtt_opts pub_opts = {.topic = mg_str(toppic),
                                .message = mg_str(message),
                                .qos = 1,
                                .retain = true};
  uint16_t msg_id = mg_mqtt_pub(nc, &pub_opts);

  LOG(NET_LOG,LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
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
  send_mqtt_led_state_msg(nc, dev_name, state, MQTT_COOL, MQTT_OFF);
}

void send_mqtt_heater_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  send_mqtt_led_state_msg(nc, dev_name, state, MQTT_ON, MQTT_OFF);
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

void send_mqtt_setpoint_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[11];
  // Use "not CELS" over "equal FAHR" so we default to FAHR for unknown units
  //sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(degC, "%.2f", (_aqualink_data->temp_units!=CELSIUS && _aqconfig_.convert_mqtt_temp)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s/setpoint", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
void send_mqtt_numeric_msg(struct mg_connection *nc, char *dev_name, int value)
{
  static char mqtt_pub_topic[250];
  static char msg[11];
  
  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[11];

  sprintf(msg, "%.2f", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_int_msg(struct mg_connection *nc, char *dev_name, int value) {
  send_mqtt_numeric_msg(nc, dev_name, value);
}

void send_mqtt_string_msg(struct mg_connection *nc, const char *dev_name, const char *msg) {
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

#define MQTT_TIMED_UDATE 300 //(in seconds)

void mqtt_broadcast_aqualinkstate(struct mg_connection *nc)
{
  int i;
  const char *status;
  int pumpStatus;
  static struct timespec last_update_timestamp = {0, 0};


  // We get called about every second, so check time every MQTT_TIMED_UDATE / 2
  if (_aqconfig_.mqtt_timed_update) {
    static int cnt=0;
    if (cnt >= (MQTT_TIMED_UDATE/2)) {
      static time_t last_full_update = 0;
      time_t now = time(0); // get time now
      if ( (int)difftime(now, last_full_update) > MQTT_TIMED_UDATE ) {
        reset_last_mqtt_status();
        memcpy(&last_full_update, &now, sizeof(time_t));
      }
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

  if (ENABLE_CHILLER) {
    if (_aqualink_data->chiller_set_point != TEMP_UNKNOWN && _aqualink_data->chiller_set_point != _last_mqtt_aqualinkdata.chiller_set_point) {
      _last_mqtt_aqualinkdata.chiller_set_point = _aqualink_data->chiller_set_point;
      send_mqtt_setpoint_msg(nc, CHILLER, _aqualink_data->chiller_set_point);
    }

    // Chiller is only on when in_alt_mode = true and led != off 
    if ( _aqualink_data->chiller_button != NULL && ((altlabel_detail *) _aqualink_data->chiller_button->special_mask_ptr)->in_alt_mode == false ) {
      // Chiller is off (in heat pump mode)
      if (OFF != _last_mqtt_chiller_led.state) {
        _last_mqtt_chiller_led.state = OFF;
        send_mqtt_led_state_msg(nc, CHILLER, OFF, MQTT_COOL, MQTT_OFF);
      }
    } else if (_aqualink_data->chiller_button != NULL && ((altlabel_detail *) _aqualink_data->chiller_button->special_mask_ptr)->in_alt_mode == true ) {
      // post actual LED state, in chiller mode
      if (_aqualink_data->chiller_button->led->state != _last_mqtt_chiller_led.state) {
        _last_mqtt_chiller_led.state = _aqualink_data->chiller_button->led->state;
        send_mqtt_led_state_msg(nc, CHILLER, _aqualink_data->chiller_button->led->state, MQTT_COOL, MQTT_OFF);
      }
    }
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
    }
    if (_aqualink_data->swg_ppm != TEMP_UNKNOWN && (_aqualink_data->swg_ppm != _last_mqtt_aqualinkdata.swg_ppm)) {
      _last_mqtt_aqualinkdata.swg_ppm = _aqualink_data->swg_ppm;
      send_mqtt_numeric_msg(nc, SWG_PPM_TOPIC, _aqualink_data->swg_ppm);
      send_mqtt_float_msg(nc, SWG_PPM_F_TOPIC, roundf(degFtoC(_aqualink_data->swg_ppm)));
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
      send_mqtt_int_msg(nc, SWG_EXTENDED_TOPIC, (int)_aqualink_data->ar_swg_device_status);
      send_mqtt_string_msg(nc, SWG_STATUS_MSG_TOPIC, get_swg_status_msg(_aqualink_data) );
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
    } else if ((_aqualink_data->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
      //send_mqtt_timer_duration_msg(nc, _aqualink_data->aqbuttons[i].name, &_aqualink_data->aqbuttons[i]);
      // send_mqtt_timer_state_msg will call send_mqtt_timer_duration_msg so no need to do it here.
      // Have to use send_mqtt_timer_state_msg due to a timer being set on a device that's already on, (ir no state change so above code does't get hit)
      
      struct timespec current_time;
      clock_gettime(CLOCK_MONOTONIC, &current_time);

      // Calculate the time difference in nanoseconds
      long long time_difference_ns = (current_time.tv_sec - last_update_timestamp.tv_sec) * 1000000000LL + (current_time.tv_nsec - last_update_timestamp.tv_nsec);

      // Check if 10 seconds (10 * 10^9 nanoseconds) have passed
      if (time_difference_ns >= 10000000000LL || last_update_timestamp.tv_sec == 0) {
        send_mqtt_timer_state_msg(nc, _aqualink_data->aqbuttons[i].name, &_aqualink_data->aqbuttons[i]);
        last_update_timestamp = current_time;
      }
    }
  }

  
  // Loop over Pumps
  for (i=0; i < _aqualink_data->num_pumps; i++) {
    int rpm_watts_changed=0;
    //_aqualink_data->pumps[i].rpm = TEMP_UNKNOWN;
    //_aqualink_data->pumps[i].gph = TEMP_UNKNOWN;
    //_aqualink_data->pumps[i].watts = TEMP_UNKNOWN;

    if (_aqualink_data->pumps[i].rpm != TEMP_UNKNOWN && _aqualink_data->pumps[i].rpm != _last_mqtt_aqualinkdata.pumps[i].rpm) {
      _last_mqtt_aqualinkdata.pumps[i].rpm = _aqualink_data->pumps[i].rpm;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_RPM_TOPIC, _aqualink_data->pumps[i].rpm);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_RPM_TOPIC, _aqualink_data->pumps[i].rpm);
      if (_aqualink_data->pumps[i].pumpType == EPUMP || _aqualink_data->pumps[i].pumpType == VSPUMP) {
        send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_SPEED_TOPIC, getPumpSpeedAsPercent(&_aqualink_data->pumps[i]));
      }
      rpm_watts_changed++;
    }
    if (_aqualink_data->pumps[i].gpm != TEMP_UNKNOWN && _aqualink_data->pumps[i].gpm != _last_mqtt_aqualinkdata.pumps[i].gpm) {
      _last_mqtt_aqualinkdata.pumps[i].gpm = _aqualink_data->pumps[i].gpm;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_GPH_TOPIC, _aqualink_data->pumps[i].gph);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_GPM_TOPIC, _aqualink_data->pumps[i].gpm);
      if (_aqualink_data->pumps[i].pumpType == VFPUMP) {
        send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_SPEED_TOPIC, getPumpSpeedAsPercent(&_aqualink_data->pumps[i]));
      }
    }
    if (_aqualink_data->pumps[i].watts != TEMP_UNKNOWN && _aqualink_data->pumps[i].watts != _last_mqtt_aqualinkdata.pumps[i].watts) {
      _last_mqtt_aqualinkdata.pumps[i].watts = _aqualink_data->pumps[i].watts;
      //send_mqtt_aux_msg(nc, PUMP_TOPIC, i+1, PUMP_WATTS_TOPIC, _aqualink_data->pumps[i].watts);
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_WATTS_TOPIC, _aqualink_data->pumps[i].watts);
      rpm_watts_changed++;
    }
    if (_aqualink_data->pumps[i].mode != TEMP_UNKNOWN && _aqualink_data->pumps[i].mode != _last_mqtt_aqualinkdata.pumps[i].mode) {
      _last_mqtt_aqualinkdata.pumps[i].mode = _aqualink_data->pumps[i].mode;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_MODE_TOPIC, _aqualink_data->pumps[i].mode);
    }
    if (_aqualink_data->pumps[i].pressureCurve != TEMP_UNKNOWN && _aqualink_data->pumps[i].pressureCurve != _last_mqtt_aqualinkdata.pumps[i].pressureCurve) {
      _last_mqtt_aqualinkdata.pumps[i].pressureCurve = _aqualink_data->pumps[i].pressureCurve;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_PPC_TOPIC, _aqualink_data->pumps[i].pressureCurve);
    }
    if (_aqualink_data->pumps[i].efficiency.isConfigured && rpm_watts_changed > 0) {
      float efficency = calculate_flow_efficiency_pct(&_aqualink_data->pumps[i]);
      if (efficency >= 0 ) {
        send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_EFFICIENCY_TOPIC, efficency);
      }
    }
    pumpStatus = getPumpStatus(i, _aqualink_data);
    if (pumpStatus != TEMP_UNKNOWN && 
        pumpStatus != _last_mqtt_aqualinkdata.pumps[i].status) {
      _last_mqtt_aqualinkdata.pumps[i].status = pumpStatus;
      send_mqtt_aux_msg(nc, _aqualink_data->pumps[i].button->name, PUMP_STATUS_TOPIC, pumpStatus);
    }
  }

  // Loop over programmable lights
  for (i=0; i < _aqualink_data->num_lights; i++) {
    //LOG(NET_LOG,LOG_NOTICE, "Light %10s | %d | lmode=%.2d cmode=%.2d | name=%s\n",_aqualink_data->lights[i].button->label,_aqualink_data->lights[i].button->led->state,_aqualink_data->lights[i].lastValue,_aqualink_data->lights[i].currentValue,get_currentlight_mode_name(_aqualink_data->lights[i], RSSADAPTER));
    char topic[50];
    if ( _aqualink_data->lights[i].currentValue != TEMP_UNKNOWN && _aqualink_data->lights[i].currentValue != _last_mqtt_aqualinkdata.lights[i].currentValue ) {
      _last_mqtt_aqualinkdata.lights[i].currentValue = _aqualink_data->lights[i].currentValue;
      send_mqtt_aux_msg(nc, _aqualink_data->lights[i].button->name, LIGHT_PROGRAM_TOPIC, _aqualink_data->lights[i].currentValue);

      sprintf(topic, "%s%s/name", _aqualink_data->lights[i].button->name, LIGHT_PROGRAM_TOPIC);
      if (_aqualink_data->lights[i].lightType == LC_DIMMER2) {
        char message[30];
        sprintf(message, "%d%%", _aqualink_data->lights[i].currentValue);
        send_mqtt_string_msg(nc, topic, message);
      } else {
        //send_mqtt_string_msg(nc, topic, light_mode_name(_aqualink_data->lights[i].lightType, _aqualink_data->lights[i].currentValue, RSSADAPTER));
        send_mqtt_string_msg(nc, topic, get_currentlight_mode_name(_aqualink_data->lights[i], RSSADAPTER));
      }
      /*
      if (_aqualink_data->lights[i].lightType == LC_DIMMER) {
        sprintf(topic, "%s%s", _aqualink_data->lights[i].button->name, LIGHT_DIMMER_VALUE_TOPIC);
        send_mqtt_int_msg(nc, topic, _aqualink_data->lights[i].currentValue * 25);
      } else*/ if (_aqualink_data->lights[i].lightType == LC_DIMMER2) {
        sprintf(topic, "%s%s", _aqualink_data->lights[i].button->name, LIGHT_DIMMER_VALUE_TOPIC);
        send_mqtt_int_msg(nc, topic, _aqualink_data->lights[i].currentValue);
      }
    }

    if (_aqualink_data->lights[i].lightType == LC_JANDYINFINATE) {
      if (_aqualink_data->lights[i].brightness != _last_mqtt_aqualinkdata.lights[i].brightness) {
        _last_mqtt_aqualinkdata.lights[i].brightness = _aqualink_data->lights[i].brightness;
        sprintf(topic, "%s%s", _aqualink_data->lights[i].button->name, LIGHT_DIMMER_VALUE_TOPIC);
        send_mqtt_int_msg(nc, topic, _aqualink_data->lights[i].brightness);
      }
      if (_aqualink_data->lights[i].red   != _last_mqtt_aqualinkdata.lights[i].red   ||
          _aqualink_data->lights[i].green != _last_mqtt_aqualinkdata.lights[i].green ||
          _aqualink_data->lights[i].blue  != _last_mqtt_aqualinkdata.lights[i].blue) {
        _last_mqtt_aqualinkdata.lights[i].red   = _aqualink_data->lights[i].red;
        _last_mqtt_aqualinkdata.lights[i].green = _aqualink_data->lights[i].green;
        _last_mqtt_aqualinkdata.lights[i].blue  = _aqualink_data->lights[i].blue;
        char rgb_msg[12];
        sprintf(rgb_msg, "%d,%d,%d", _aqualink_data->lights[i].red,
                                     _aqualink_data->lights[i].green,
                                     _aqualink_data->lights[i].blue);
        sprintf(topic, "%s%s", _aqualink_data->lights[i].button->name, LIGHT_RGB_TOPIC);
        send_mqtt_string_msg(nc, topic, rgb_msg);
      }
    }
  }

  // Loop over sensors
  for (i=0; i < _aqualink_data->num_sensors; i++) {
    if ( _aqualink_data->sensors[i].value != TEMP_UNKNOWN && _last_mqtt_aqualinkdata.sensors[i].value != _aqualink_data->sensors[i].value) {
      char topic[50];
      sprintf(topic, "%s%s", FULL_SENSOR_TOPIC, _aqualink_data->sensors[i].ID);
      send_mqtt_float_msg(nc, topic, _aqualink_data->sensors[i].value);
      _last_mqtt_aqualinkdata.sensors[i].value = _aqualink_data->sensors[i].value;
    }
  }
}


typedef enum {uActioned, uBad, uDevices, uStatus, uHomebridge, uDynamicconf, uDebugStatus, uDebugDownload, uSimulator, uSchedules, uSetSchedules, uAQmanager, uLogDownload, uNotAvailable, uConfig, uSaveConfig, uConfigDownload, uSaveWebConfig} uriAtype;
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
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_PDA_DEVICE_ON_OFF, button, (isON? ON : OFF), AQP_NULL, _aqualink_data);
#else
  int i;
  char msg[PTHREAD_ARG];

  for (i=0; i < _aqualink_data->total_buttons; i++) {
    if (_aqualink_data->aqbuttons[i].code == button->code) {
      sprintf(msg, "%-5d%-5d", i, (isON? ON : OFF));
      aq_programmer(AQ_PDA_DEVICE_ON_OFF, msg, _aqualink_data);
      break;
    }
  }
#endif
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
  } else if (strncmp(ri1, "config/download", 10) == 0) {
    return uConfigDownload;
  } else if (strncmp(ri1, "config/set", 10) == 0) {
    return uSaveConfig;
  } else if (strncmp(ri1, "webconfig/set", 13) == 0) {
    return uSaveWebConfig;
  } else if (strncmp(ri1, "config", 6) == 0) {
    return uConfig;
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
    if ( round(value) == RSSD_LOG ) {
      // Check for filter on RSSD LOG
      if (ri2 != NULL) {
        unsigned int n;
        // ri will be /addlogmask/0x01 0x02 0x03 0x04/
        for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
          int index=i*5;
          if (ri2[index]=='0' && ri2[index+1]=='x') {
            sscanf(&ri2[index], "0x%2x", &n);
            _aqconfig_.RSSD_LOG_filter[i] = n;
          //_aqconfig_.RSSD_LOG_filter_OLD = strtoul(cleanalloc(ri2), NULL, 16);
            LOG(NET_LOG,LOG_NOTICE, "Adding RSSD LOG filter 0x%02hhx", _aqconfig_.RSSD_LOG_filter[i]);
          }
        }
      }
    }
    addDebugLogMask(round(value));
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "removelogmask", 13) == 0 && from == NET_WS) { // Only valid from websocket.
    removeDebugLogMask(round(value));
    if ( round(value) == RSSD_LOG ) {
      for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
        _aqconfig_.RSSD_LOG_filter[i] = NUL;
      }
      //_aqconfig_.RSSD_LOG_filter_OLD = NUL;
      //LOG(NET_LOG,LOG_NOTICE, "Removed RSSD LOG filter");
    }
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "logfile", 7) == 0) {
    if (ri2 != NULL && strncmp(ri2, "download", 8) == 0) {
      LOG(NET_LOG,LOG_INFO, "Received download log request!\n");
      return uLogDownload;
    } 
    return uAQmanager; // Want to resent updated status
  } else if (strncmp(ri1, "restart", 7) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received restart request!\n");
    raise(SIGRESTART);
    return uActioned;
  } else if (strncmp(ri1, "installrelease", 14) == 0 && from == NET_WS) { // Only valid from websocket.
    if (ri2 != NULL) {
      size_t version_length = (size_t)(URI + uri_length - ri2);

      LOG(NET_LOG,LOG_NOTICE, "Received install release request, %s\n",ri2);
      _aqualink_data->upgrade_version = malloc(version_length + 1);
      if (_aqualink_data->upgrade_version == NULL) {
        LOG(NET_LOG,LOG_ERR, "Could not allocate upgrade version\n");
        return uBad;
      }
      memcpy(_aqualink_data->upgrade_version, ri2, version_length);
      _aqualink_data->upgrade_version[version_length] = '\0';
    } else {
      LOG(NET_LOG,LOG_NOTICE, "Received install release request, but no version named, using latest!\n");
      _aqualink_data->upgrade_version = "latest";
    }
    raise(SIGRUPGRADE);
    return uActioned;
 /*   
  } else if (strncmp(ri1, "upgrade", 7) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received upgrade request!\n");
    setMASK(_aqualink_data->updatetype, UPDATERELEASE);
    raise(SIGRUPGRADE);
    return uActioned; 
  } else if (strncmp(ri1, "installdevrelease", 17) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received install dev release request!\n");
    setMASK(_aqualink_data->updatetype, INSTALLDEVRELEASE);
    raise(SIGRUPGRADE);
    return uActioned; 
    */
  } else if (strncmp(ri1, "seriallogger", 12) == 0 && from == NET_WS) { // Only valid from websocket.
    LOG(NET_LOG,LOG_NOTICE, "Received request to run rs485mon!\n");
    //LOG(NET_LOG,LOG_NOTICE, "Received request ri1=%s, ri2=%s, ri3=%s value=%f\n",ri1,ri2,ri3,value);
    _aqualink_data->slogger_packets = round(value);
    if (ri2 != NULL) {
      size_t ids_length = ri3 != NULL
                            ? (size_t)(ri3 - ri2 - 1)
                            : (size_t)(URI + uri_length - ri2);
      ids_length = AQ_MIN(ids_length, sizeof(_aqualink_data->slogger_ids) - 1);
      memcpy(_aqualink_data->slogger_ids, ri2, ids_length);
      _aqualink_data->slogger_ids[ids_length] = '\0'; // 0x01 0x02 0x03 0x04
    } else {
      _aqualink_data->slogger_ids[0] = '\0';
    }
    if (ri3 != NULL && strncmp(ri3, "true", 4) == 0) {
      _aqualink_data->slogger_debug = true;
    } else {
      _aqualink_data->slogger_debug = false;
    }
    //LOG(NET_LOG,LOG_NOTICE, "Received request to run rs485mon (%d,%s,%s)!\n",
    //                        _aqualink_data->slogger_packets,
    //                        _aqualink_data->slogger_ids[0]!='\0'?_aqualink_data->slogger_ids:" ", 
    //                        _aqualink_data->slogger_debug?"debug":"" ); 
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
    if(isONET_ENABLED)
      queueGetProgramData(ONETOUCH, _aqualink_data);
    if(isIAQT_ENABLED)
      queueGetProgramData(IAQTOUCH, _aqualink_data);
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
     //create_program_request(from, POOL_HTR_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, POOL_HTR_SETPOINT, 0, val, from);
    } else if (strncmp(ri1, BTN_SPA_HTR, strlen(BTN_SPA_HTR)) == 0) {
      //create_program_request(from, SPA_HTR_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, SPA_HTR_SETPOINT, 0, val, from);
    } else if (strncmp(ri1, FREEZE_PROTECT, strlen(FREEZE_PROTECT)) == 0) {
      //create_program_request(from, FREEZE_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, FREEZE_SETPOINT, 0, val, from);
    } else if (strncmp(ri1, CHILLER, strlen(CHILLER)) == 0) {
      //create_program_request(from, FREEZE_SETPOINT, val, 0);
      panel_device_request(_aqualink_data, CHILLER_SETPOINT, 0, val, from);
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
    } else {
      rtn = uActioned;
    }
  } else if ((ri3 != NULL && (strncasecmp(ri2, "brightness", 10) == 0) && (strncasecmp(ri3, "set", 3) == 0))) {
    found = false;
    for (i=0; i < _aqualink_data->total_buttons; i++) {
      if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ||
          strncmp(ri1, _aqualink_data->aqbuttons[i].label, strlen(_aqualink_data->aqbuttons[i].label)) == 0)
      {
        found = true;
        panel_device_request(_aqualink_data, LIGHT_BRIGHTNESS, i, value, from);
        break;
      }
    }
    if(!found) {
      *rtnmsg = NO_PLIGHT_DEVICE;
      LOG(NET_LOG,LOG_WARNING, "%s: Didn't find device that matched URI '%.*s'\n",actionName[from], uri_length, URI);
      rtn = uBad;
    } else {
      rtn = uActioned;
    }
  } else if ((ri3 != NULL && (strncasecmp(ri2, "rgb", 3) == 0) && (strncasecmp(ri3, "set", 3) == 0))) {
    found = false;
    for (i=0; i < _aqualink_data->total_buttons; i++) {
      if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ||
          strncmp(ri1, _aqualink_data->aqbuttons[i].label, strlen(_aqualink_data->aqbuttons[i].label)) == 0)
      {
        found = true;
        panel_device_request(_aqualink_data, LIGHT_RGB, i, (int)value, from);
        break;
      }
    }
    if(!found) {
      *rtnmsg = NO_PLIGHT_DEVICE;
      LOG(NET_LOG,LOG_WARNING, "%s: Didn't find device that matched URI '%.*s'\n",actionName[from], uri_length, URI);
      rtn = uBad;
    } else {
      rtn = uActioned;
    }
  // Action a pump RPM/GPM message
  } else if ((ri3 != NULL && ((strncasecmp(ri2, "RPM", 3) == 0) || (strncasecmp(ri2, "GPM", 3) == 0) || (strncasecmp(ri2, "Speed", 5) == 0) || (strncasecmp(ri2, "VSP", 3) == 0)) && (strncasecmp(ri3, "set", 3) == 0))) {
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
            if (strncasecmp(ri2, "Speed", 5) == 0) {
              int val = convertPumpPercentToSpeed(&_aqualink_data->pumps[i], round(value));
              LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d Speed to %d%%, using %s of %d\n",actionName[from],pumpIndex+1, round(value), (_aqualink_data->pumps[i].pumpType==VFPUMP?"GPM":"RPM" ) ,val);
              panel_device_request(_aqualink_data, PUMP_RPM, pumpIndex, val, from); 
            } else {
              LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d %s to %d\n",actionName[from],pumpIndex+1, (strncasecmp(ri2, "GPM", 3) == 0)?"GPM":"RPM", round(value));
            //create_program_request(from, PUMP_RPM, round(value), pumpIndex);
              panel_device_request(_aqualink_data, PUMP_RPM, pumpIndex, round(value), from);
            }
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
        //if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ){
        if ( uri_strcmp(ri1, _aqualink_data->aqbuttons[i].name) || 
             ( isVBUTTON_ALTLABEL(_aqualink_data->aqbuttons[i].special_mask) && uri_strcmp(ri1, ((altlabel_detail *)_aqualink_data->aqbuttons[i].special_mask_ptr)->altlabel)) ) {
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
                if (strncasecmp(ri2, "Speed", 5) == 0) {
                  int val = convertPumpPercentToSpeed(&_aqualink_data->pumps[pi], round(value));
                  LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d Speed to %d%%, using %s of %d\n",actionName[from],_aqualink_data->pumps[pi].pumpIndex, round(value), (_aqualink_data->pumps[i].pumpType==VFPUMP?"GPM":"RPM" ) ,val);
                  panel_device_request(_aqualink_data, PUMP_RPM, _aqualink_data->pumps[pi].pumpIndex, val, from); 
                } else {
                  LOG(NET_LOG,LOG_NOTICE, "%s: request to change pump %d %s to %d\n",actionName[from], pi+1, (strncasecmp(ri2, "GPM", 3) == 0)?"GPM":"RPM", round(value));
                //create_program_request(from, PUMP_RPM, round(value), _aqualink_data->pumps[pi].pumpIndex);
                  panel_device_request(_aqualink_data, PUMP_RPM, _aqualink_data->pumps[pi].pumpIndex, round(value), from);
                }
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
      SET_IF_CHANGED(_aqualink_data->orp, round(value), _aqualink_data->is_dirty);
      rtn = uActioned;
      LOG(NET_LOG,LOG_INFO, "%s: request to set ORP to %d\n",actionName[from],_aqualink_data->orp);
    } else if ( strncasecmp(ri2, "Ph", 2) == 0 ) {
      SET_IF_CHANGED(_aqualink_data->ph, value, _aqualink_data->is_dirty);
      rtn = uActioned;
      LOG(NET_LOG,LOG_INFO, "%s: request to set Ph to %.2f\n",actionName[from],_aqualink_data->ph);
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
    if (strncasecmp(ri2, "timer_sec", 9) == 0) {
      atype = TIMER_SEC;
    } else if (strncasecmp(ri2, "timer", 5) == 0) {
      atype = TIMER; 
    }else if ( value > 1 || value < 0) {
      LOG(NET_LOG,LOG_WARNING, "%s: URI %s has invalid value %.2f\n",actionName[from], URI, value);
      *rtnmsg = INVALID_VALUE;
      rtn = uBad;
      return rtn;
    }

    for (i=0; i < _aqualink_data->total_buttons && found==false; i++) {
      // If Label = "Spa", "Spa_Heater" will turn on "Spa", so need to check '/' on label as next character
      //if (strncmp(ri1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ||
      //   (strncmp(ri1, _aqualink_data->aqbuttons[i].label, strlen(_aqualink_data->aqbuttons[i].label)) == 0 && ri1[strlen(_aqualink_data->aqbuttons[i].label)] == '/'))
      //if ( uri_strcmp(ri1, _aqualink_data->aqbuttons[i].name) || uri_strcmp(ri1, _aqualink_data->aqbuttons[i].label) )

      if ( uri_strcmp(ri1, _aqualink_data->aqbuttons[i].name) || uri_strcmp(ri1, _aqualink_data->aqbuttons[i].label) || 
         ( isVBUTTON_ALTLABEL(_aqualink_data->aqbuttons[i].special_mask) && uri_strcmp(ri1, ((altlabel_detail *)_aqualink_data->aqbuttons[i].special_mask_ptr)->altlabel)) )
      {
        found = true;
        //create_panel_request(from, i, value, istimer);
        LOG(NET_LOG,LOG_INFO, "%d: MATCH %s to topic %.*s\n",from,_aqualink_data->aqbuttons[i].name,uri_length, URI);
        LOG(NET_LOG,LOG_INFO, "ri1=%s, length=%d char at len=%c\n",ri1,strlen(_aqualink_data->aqbuttons[i].label),ri1[strlen(_aqualink_data->aqbuttons[i].label)] );
        panel_device_request(_aqualink_data, atype, i, value, from);
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

/*
  Quicker and more accurate for us than normal strncmp, since we check for the trailing / at right position
  check Spa against uri /Spa/set /Spa_mode/set / Spa_heater/set
*/
bool uri_strcmp(const char *uri, const char *string) {
  int i;
  int len = strlen(string);

  // Check the trailing / on length first.
  if (uri[len] != '/') {
    return false;
  }

  // Now check all characters
  for (i=0; i < len; i++) {
    if ( uri[i] != string[i] ){
      return false;
    } 
  }

  return true;
}

void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  char *rtnmsg;
#ifdef AQ_TM_DEBUG
  int tid;
#endif
  //unsigned int i;
  //LOG(NET_LOG,LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.buf, atof(msg->data.buf));
  // If message doesn't end in set or increment we don't care about it.
  if (strncmp(&msg->topic.buf[msg->topic.len -4], "/set", 4) != 0 && strncmp(&msg->topic.buf[msg->topic.len -10], "/increment", 10) != 0) {
    LOG(NET_LOG,LOG_DEBUG, "MQTT: Ignore %.*s %.*s\n",msg->topic.len, msg->topic.buf, msg->data.len, msg->data.buf);
    return;
  }
  LOG(NET_LOG,LOG_DEBUG, "MQTT: topic %.*s %.*s\n",msg->topic.len, msg->topic.buf, msg->data.len, msg->data.buf);

  DEBUG_TIMER_START(&tid);
  //Need to do this in a better manor, but for present it's ok.
  static char tmp[20];
  strncpy(tmp, msg->data.buf, msg->data.len);
  tmp[msg->data.len] = '\0';

  //float value = atof(tmp);

  // Check value like on/off/heat/cool and convery to int.
  // HASSIO doesn't support `mode_command_template` so easier to code around their limotation here.
  char *end;
  float value = strtof(tmp, &end);
  if (tmp == end) { // Not a number
    // See if any test resembeling 1, of not leave at zero.
    if (rsm_strcmp(tmp, "on")==0 || rsm_strcmp(tmp, "heat")==0 || rsm_strcmp(tmp, "cool")==0)
      value = 1;

    LOG(NET_LOG,LOG_NOTICE, "MQTT: converted value from '%s' to '%.0f', from message '%.*s'\n",tmp,value,msg->topic.len, msg->topic.buf);
  } 


  //int val = _aqualink_data->unactioned.value = (_aqualink_data->temp_units != CELSIUS && _aqconfig_.convert_mqtt_temp) ? round(degCtoF(value)) : round(value);

  // For RGB topics, re-parse the raw string as "r,g,b" and pack into value as a single int.
  if (msg->topic.len >= 7 && strncmp(&msg->topic.buf[msg->topic.len - 7], "rgb/set", 7) == 0) {
    int r = 0, g = 0, b = 0;
    if (sscanf(tmp, "%d,%d,%d", &r, &g, &b) == 3) {
      value = (float)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
    } else {
      LOG(NET_LOG, LOG_ERR, "Invalid RGB payload '%s', expected 'r,g,b'\n", tmp);
    }
  }

  bool convert = (_aqualink_data->temp_units != CELSIUS && _aqconfig_.convert_mqtt_temp)?true:false;
  int offset = strlen(_aqconfig_.mqtt_aq_topic)+1;
  if ( action_URI(NET_MQTT, &msg->topic.buf[offset], msg->topic.len - offset, value, convert, &rtnmsg) == uBad ) {
    // Check if it was something that can't be changed, if so send back current state.  Homekit thermostat for SWG and Freezeprotect.
    if (  strncmp(&msg->topic.buf[offset], FREEZE_PROTECT, strlen(FREEZE_PROTECT)) == 0) {
      if (_aqualink_data->frz_protect_set_point != TEMP_UNKNOWN ) {
        send_mqtt_setpoint_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_set_point);
        send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_ON);
      } else {
        send_mqtt_string_msg(nc, FREEZE_PROTECT_ENABELED, MQTT_OFF);
      }
      send_mqtt_string_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_state==ON?MQTT_ON:MQTT_OFF);
    } else if (  strncmp(&msg->topic.buf[offset], SWG_TOPIC, strlen(SWG_TOPIC)) == 0) {
      if (_aqualink_data->swg_led_state != LED_S_UNKNOWN) {
        send_mqtt_swg_state_msg(nc, SWG_TOPIC, _aqualink_data->swg_led_state);
        send_mqtt_int_msg(nc, SWG_BOOST_TOPIC, _aqualink_data->boost);
      }
    }
  }

  DEBUG_TIMER_STOP(tid, NET_LOG, "action_mqtt_message() completed, took ");
}




float pass_mg_body(struct mg_str *body) {
  LOG(NET_LOG,LOG_INFO, "Message body:'%.*s'\n", body->len, body->buf);
  // Quick n dirty pass value from either of below.
  // value=1.5&arg2=val2
  // {"value":"1.5"}
  int i;
  char buf[10];
  
  int len = sizeof(buf);
  if (body->len < len) {
    len = body->len;
  }

  // NSF Really need to come back and clean this up

  for (i=0; i < len; i++) {
    if ( body->buf[i] == '=' || body->buf[i] == ':' ) {
      while (!isdigit((unsigned char) body->buf[i]) && body->buf[i] != '-' && i < len) {i++;}
      if(i < len) {
        // Need to copy to buffer so we can terminate correctly.
        strncpy(buf, &body->buf[i], len - i);
        buf[len - i] = '\0';
        return atof(buf);
      }
    }
  }
  
  return TEMP_UNKNOWN;
}

void log_http_request(int level, char *message, struct mg_http_message *http_msg) {
  char *uri = (char *)malloc(http_msg->uri.len + http_msg->query.len + 2);
  
  strncpy(uri, http_msg->uri.buf, http_msg->uri.len + http_msg->query.len + 1);
  uri[http_msg->uri.len + http_msg->query.len + 1] = '\0';

  LOG(NET_LOG,level, "%s: '%s', length %d\n", message, uri, http_msg->uri.len);
  
  free(uri);
}

void serve_file(struct mg_connection *nc, struct mg_http_message *http_msg)
{
  // Anything with .json should not be cached.
  if (FAST_SUFFIX_4_CI(http_msg->uri.buf, http_msg->uri.len, "json"))
  {
    if (_aqconfig_.web_config != NULL && strncmp(http_msg->uri.buf, "/config.json", 12) == 0)
    {
      mg_http_serve_file(nc, http_msg, _aqconfig_.web_config, &_http_server_opts_nocache);
      LOG(NET_LOG, LOG_NOTICE, "Using %s for web config\n", _aqconfig_.web_config);
    }
    else
    {
      mg_http_serve_dir(nc, http_msg, &_http_server_opts_nocache);
    }
  }
  else // can cache anything here.
  {
    if (http_msg->uri.len <= 12 && strncmp(http_msg->uri.buf, "/aqmanager", 10) == 0) {
      char buf[256];
      snprintf(buf, 256, "%s/aqmanager.html", _aqconfig_.web_directory);
      mg_http_serve_file(nc, http_msg, buf, &_http_server_opts);
    } else {
      mg_http_serve_dir(nc, http_msg, &_http_server_opts);
    }
  }
}

void action_web_request(struct mg_connection *nc, struct mg_http_message *http_msg)
{
  char *msg = NULL;
  // struct http_message *http_msg = (struct http_message *)ev_data;
#ifdef AQ_TM_DEBUG
  int tid;
  int tid2;
#endif

  // DEBUG_TIMER_START(&tid);
  if (getLogLevel(NET_LOG) >= LOG_INFO)
  { // Simply for log message, check we are at
    log_http_request(LOG_INFO, "URI request", http_msg);
  }
  // DEBUG_TIMER_STOP(tid, NET_LOG, "action_web_request debug print crap took");

  // LOG(NET_LOG,LOG_INFO, "Message request:\n'%.*s'\n", http_msg->message.len, http_msg->message.p);

  // If we have a get request (ie file), pass it
  if (strncmp(http_msg->uri.buf, "/api", 4) != 0)
  {
    DEBUG_TIMER_START(&tid);
    serve_file(nc, http_msg);
    DEBUG_TIMER_STOP(tid, NET_LOG, "action_web_request() serve file took");
    return;
  }

  char buf[JSON_BUFFER_SIZE];
  float value = 0;
  DEBUG_TIMER_START(&tid);

  // If query string.
  if (http_msg->query.len > 1)
  {
    // mg_get_http_var(&http_msg->query, "value", buf, sizeof(buf)); // Old mosquitto
    mg_http_get_var(&http_msg->query, "value", buf, sizeof(buf));
    value = atof(buf);
  }
  else if (http_msg->body.len > 1)
  {
    value = pass_mg_body(&http_msg->body);
  }

  int len = mg_url_decode(http_msg->uri.buf, http_msg->uri.len, buf, 50, 0);

  if (strncmp(http_msg->uri.buf, "/api/", 4) == 0)
  {
    switch (action_URI(NET_API, &buf[5], len - 5, value, false, &msg))
    {
    case uActioned:
      mg_http_reply(nc, 200, CONTENT_TEXT, GET_RTN_OK);
      break;
    case uDevices:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      build_device_JSON(_aqualink_data, message, JSON_BUFFER_SIZE, false);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_device_JSON took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uHomebridge:
    {
      char message[JSON_BUFFER_SIZE];
      build_device_JSON(_aqualink_data, message, JSON_BUFFER_SIZE, true);
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uStatus:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      build_aqualink_status_JSON(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_aqualink_status_JSON took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uDynamicconf:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      /*
      build_dynamic_webconfig_js(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_webconfig_js took");
      mg_http_reply(nc, 200, CONTENT_JS, message);
      */
      build_dynamic_webconfig_json(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_webconfig_json took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uSchedules:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      build_schedules_js(message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_schedules_js took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uSetSchedules:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      save_schedules_js(http_msg->body.buf, http_msg->body.len, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() save_schedules_js took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
    case uConfig:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      build_aqualink_config_JSON(message, JSON_BUFFER_SIZE, _aqualink_data);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_aqualink_config_JSON took");
      mg_http_reply(nc, 200, CONTENT_JSON, message);
    }
    break;
#ifndef AQ_MANAGER
    case uDebugStatus:
    {
      char message[JSON_BUFFER_SIZE];
      snprintf(message, 80, "{\"sLevel\":\"%s\", \"iLevel\":%d, \"logReady\":\"%s\"}\n", elevel2text(getLogLevel(NET_LOG)), getLogLevel(NET_LOG), islogFileReady() ? "true" : "false");
      mg_http_reply(nc, 200, CONTENT_JS, message);
    }
    break;
#else
    case uLogDownload:
// int lines = 1000;
#define DEFAULT_LOG_DOWNLOAD_LINES 100
      // If lines was passed in post use it, if not see if it's next path in URI is a number
      if (value == 0.0)
      {
        // /api/<downloadmsg>/<lines>
        char *pt = rsm_lastindexof(buf, "/", strlen(buf));
        value = atoi(pt + 1);
      }
      LOG(NET_LOG, LOG_DEBUG, "Downloading log of max %d lines\n", value > 0 ? (int)value : DEFAULT_LOG_DOWNLOAD_LINES);
      if (write_systemd_logmessages_2file("/dev/shm/aqualinkd.log", value > 0 ? (int)value : DEFAULT_LOG_DOWNLOAD_LINES))
      {
        mg_http_serve_file(nc, http_msg, "/dev/shm/aqualinkd.log", &_http_server_opts_nocache);
        remove("/dev/shm/aqualinkd.log");
      }
      break;

    case uConfigDownload:
      LOG(NET_LOG, LOG_DEBUG, "Downloading config\n");
      mg_http_serve_file(nc, http_msg, _aqconfig_.config_file, &_http_server_opts_nocache);
      break;
#endif
    case uBad:
    default:
      if (msg == NULL)
      {
        mg_http_reply(nc, 400, CONTENT_TEXT, GET_RTN_UNKNOWN);
      }
      else
      {
        mg_http_reply(nc, 400, CONTENT_TEXT, msg);
      }
      break;
    }
  }
  else
  {
    mg_http_reply(nc, 400, CONTENT_TEXT, GET_RTN_UNKNOWN);
  }

  sprintf(buf, "action_web_request() request '%.*s' took", (int)http_msg->uri.len, http_msg->uri.buf);

  DEBUG_TIMER_STOP(tid, NET_LOG, buf);
}

void action_websocket_request(struct mg_connection *nc, struct mg_ws_message *wm) {
  char buffer[100];
  struct JSONkvptr jsonkv;
  int i;
  char *uri = NULL;
  char *value = NULL;
  //char *id = NULL;
  //char *text_value = NULL;
  char *msg = NULL;
#ifdef AQ_TM_DEBUG
  int tid;
#endif
#ifdef AQ_PDA
  // Any websocket request means UI is active, so don't let AqualinkD go to sleep if in PDA mode
  if (isPDA_PANEL)
    pda_reset_sleep();
#endif
   
  strncpy(buffer, (char *)wm->data.buf, AQ_MIN(wm->data.len, 99));
  buffer[wm->data.len] = '\0';

  parseJSONrequest(buffer, &jsonkv);

  for(i=0; i < 4; i++) {
    if (jsonkv.kv[i].key != NULL)
      LOG(NET_LOG,LOG_DEBUG, "WS: Message - Key '%s' Value '%s'\n",jsonkv.kv[i].key,jsonkv.kv[i].value);
    
    if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "uri", 3) == 0)
      uri = jsonkv.kv[i].value;
    else if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "value", 5) == 0)
      value = jsonkv.kv[i].value;
    //else if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "button", 6) == 0)
    //  id = jsonkv.kv[i].value;
    //else if (jsonkv.kv[i].key != NULL && strncmp(jsonkv.kv[i].key, "text_value", 10) == 0)
    //  text_value = jsonkv.kv[i].value;
  }
  
  if (uri == NULL) {
    LOG(NET_LOG,LOG_ERR, "WEB: Old websocket stanza requested, ignoring client request\n");
    return;
  }

  // NSF in future change action_URI to accept button and text_value
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
    case uDynamicconf:
    {
      char message[JSON_BUFFER_SIZE];
      DEBUG_TIMER_START(&tid2);
      build_dynamic_webconfig_json(_aqualink_data, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid2, NET_LOG, "action_web_request() build_webconfig_json took");
      ws_send(nc, message);
    }
    break;
    case uSimulator:
    {
      LOG(NET_LOG,LOG_DEBUG, "Request to start Simulator\n");
      set_websocket_simulator(nc);
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
      save_schedules_js((char *)wm->data.buf, wm->data.len, message, JSON_BUFFER_SIZE);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() save_schedules_js took");
      ws_send(nc, message); 
    }
    break;
    case uConfig:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      build_aqualink_config_JSON(message, JSON_BUFFER_SIZE, _aqualink_data);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() build_aqualink_config_JSON took");
      ws_send(nc, message);
    }
    break;
    case uSaveConfig:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      save_config_js((char *)wm->data.buf, wm->data.len, message, JSON_BUFFER_SIZE, _aqualink_data);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() save_config_js took");
      ws_send(nc, message);
    }
    break;
    case uSaveWebConfig:
    {
      DEBUG_TIMER_START(&tid);
      char message[JSON_BUFFER_SIZE];
      save_web_config_json((char *)wm->data.buf, wm->data.len, message, JSON_BUFFER_SIZE, _aqualink_data);
      DEBUG_TIMER_STOP(tid, NET_LOG, "action_websocket_request() save_web_config_json took");
      ws_send(nc, message);
    }
    break;
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


static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  struct mg_http_message *http_msg;
  struct mg_ws_message *ws_msg;
  char aq_topic[30];
  #ifdef AQ_TM_DEBUG 
    int tid; 
  #endif
  //static double last_control_time;

  // LOG(NET_LOG,LOG_DEBUG, "Event\n");
  switch (ev) {
  //case MG_EV_HTTP_REQUEST:
  case MG_EV_HTTP_MSG:
    http_msg = (struct mg_http_message *)ev_data;

    //if ( strstr(http_msg->head.buf, "Upgrade: websocket")  ) {
    if ( mg_http_get_header(http_msg, "Sec-WebSocket-Key") != NULL) {
      LOG(NET_LOG,LOG_DEBUG, "Enable websockets\n");
      mg_ws_upgrade(nc, http_msg, NULL);
      break;
    }

    DEBUG_TIMER_START(&tid); 
    action_web_request(nc, http_msg);
    DEBUG_TIMER_STOP(tid, NET_LOG, "WEB Request action_web_request() took"); 
    LOG(NET_LOG,LOG_DEBUG, "Served WEB request\n");
    break;
  
  case MG_EV_WS_OPEN:
    _aqualink_data->open_websockets++;
    LOG(NET_LOG,LOG_DEBUG, "++ Websocket joined\n");
    break;
  
  case MG_EV_WS_MSG:
    ws_msg = (struct mg_ws_message *)ev_data;
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
    } else if (is_mqtt(nc) || is_mqttconnecting(nc) ) {
      LOG(NET_LOG,LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_exit_flag = true;
    }

    break;
  
  case MG_EV_ACCEPT: 
    if (is_mqtt(nc)) {
      return;
    }
    // Only want HTTPS & WS connections
#if MG_TLS > 0
    if (nc->is_tls) {
      static char *crt;
      static char *key; 
      static char *ca;
      
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));

      if (crt == NULL || key == NULL) {
        LOG(NET_LOG,LOG_NOTICE, "HTTPS: loading certs from : %s\n", _aqconfig_.cert_dir);
        crt = read_pem_file(false, "%s/crt.pem",_aqconfig_.cert_dir);
        key = read_pem_file(false, "%s/key.pem",_aqconfig_.cert_dir);
        ca = read_pem_file(true, "%s/ca.pem",_aqconfig_.cert_dir); // If this doesn't exist we don't care. If it exists, 2 way auth
      }
      opts.ca = mg_str(ca);    // Most cases this will be null, only get's set for 2 way auth (ie load cert and authority onto client)
      opts.cert = mg_str(crt);
      opts.key = mg_str(key);      
      mg_tls_init(nc, &opts);
    }
#endif
    break;

  case MG_EV_CONNECT: {
    set_mqttconnected(nc);
    //set_mqtt(nc);
    _mqtt_exit_flag = false;
    LOG(NET_LOG,LOG_DEBUG, "MQTT: Connected to : %s\n", _aqconfig_.mqtt_server);
#if MG_TLS > 0
    if (nc->is_tls) {
      static char *crt;
      static char *key;
      static char *ca;
      
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));

      if (crt == NULL || key == NULL) {
        LOG(NET_LOG,LOG_NOTICE, "MQTTS: loading certs from : %s\n", _aqconfig_.mqtt_cert_dir);
        crt = read_pem_file(false, "%s/crt.pem",_aqconfig_.cert_dir);
        key = read_pem_file(false, "%s/key.pem",_aqconfig_.cert_dir);
        ca = read_pem_file(true, "%s/ca.pem",_aqconfig_.cert_dir);
      }
      opts.cert = mg_str(crt);
      opts.key = mg_str(key);
      opts.ca = mg_str(ca);
      mg_tls_init(nc, &opts);
    }
#endif
  } break;

  case MG_EV_MQTT_OPEN:
    {
      //struct mg_mqtt_opts sub_opts
      static uint8_t qos=0;// PUT IN FUNCTION HEADDER can't be bothered with ack, so set to 0

      LOG(NET_LOG,LOG_DEBUG, "MQTT: Connection open %lu\n", nc->id);

      snprintf(aq_topic, 29, "%s/#", _aqconfig_.mqtt_aq_topic);
      //mqtt_subscribe(nc, aq_topic);
      struct mg_mqtt_opts sub_opts;
      memset(&sub_opts, 0, sizeof(sub_opts));
      sub_opts.topic = mg_str(aq_topic);
      sub_opts.qos = qos;
      LOG(NET_LOG,LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
      mg_mqtt_sub(nc, &sub_opts);
      
      LOG(NET_LOG,LOG_INFO, "MQTT: sending Alive message (last will message)\n");
      snprintf(aq_topic, 24, "%s/%s", _aqconfig_.mqtt_aq_topic,MQTT_LWM_TOPIC);
      send_mqtt(nc, aq_topic ,MQTT_ON);

      publish_mqtt_discovery( _aqualink_data, nc);
    }
    break;

  case MG_EV_MQTT_CMD:
    //LOG(NET_LOG,LOG_NOTICE, "MQTT: MG_EV_MQTT_CMD command, add code / need to replocate MG_EV_MQTT_PUBACK MG_EV_MQTT_SUBACK\n");
    break;
  //case MG_EV_MQTT_PUBLISH:
  case MG_EV_MQTT_MSG:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    
    // We are only subscribed to aqualink topic, (so not checking that).
    // Just check we have "set" as string end
    if ( FAST_SUFFIX_3_CI(mqtt_msg->topic.buf, mqtt_msg->topic.len, "set"))
    {
        DEBUG_TIMER_START(&tid); 
        action_mqtt_message(nc, mqtt_msg);
        DEBUG_TIMER_STOP(tid, NET_LOG, "MQTT Request action_mqtt_message() took"); 
    } else {
      LOG(NET_LOG,LOG_DEBUG, "MQTT: received (msg_id: %d), %.*s ignoring\n", mqtt_msg->id, mqtt_msg->topic.len, mqtt_msg->topic.buf);
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
    //if (isVBUTTON_CHILLER(_aqualink_data->aqbuttons[i].special_mask)){
    //  _chiller_ledindex = i;
    //}
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
  _last_mqtt_aqualinkdata.chiller_set_point = TEMP_REFRESH;
  //_last_mqtt_aqualinkdata.chiller_state = LED_S_UNKNOWN;
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

  for (i=0; i < _aqualink_data->num_lights; i++) {
     _last_mqtt_aqualinkdata.lights[i].currentValue = TEMP_UNKNOWN;
  }

  for (i=0; i < _aqualink_data->num_sensors; i++) {
    _last_mqtt_aqualinkdata.sensors[i].value = TEMP_UNKNOWN;
  }

  _last_mqtt_chiller_led.state = LED_S_UNKNOWN;

}

void start_mqtt(struct mg_mgr *mgr) {
  
  //generate_mqtt_id(_aqconfig_.mqtt_ID, MQTT_ID_LEN);

  //LOG(NET_LOG,LOG_WARNING, "NOT Starting MQTT client, need to check code\n");
  if ( _aqconfig_.mqtt_server == NULL || _aqconfig_.mqtt_aq_topic == NULL ) 
    return;

  char aq_topic[30];
  char *mqtt_ID = generate_mqtt_id();
  LOG(NET_LOG,LOG_NOTICE, "Starting MQTT client to %s, id %s\n", _aqconfig_.mqtt_server, mqtt_ID);

  snprintf(aq_topic, 24, "%s/%s", _aqconfig_.mqtt_aq_topic,MQTT_LWM_TOPIC);

  struct mg_mqtt_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user = mg_str(_aqconfig_.mqtt_user);
    opts.pass = mg_str(_aqconfig_.mqtt_passwd);
    //opts.client_id = mg_str(_aqconfig_.mqtt_ID);
    opts.client_id = mg_str(mqtt_ID);

    //opts.keepalive = 5; // This seems to kill connection for some reason, and not sent heartbeat
    opts.clean = true;
    //opts.version = 4; // Maybe 5
    opts.message = mg_str(MQTT_OFF); // will_message
    opts.topic = mg_str(aq_topic); // will_topic
    

  struct mg_connection *nc = mg_mqtt_connect(mgr, _aqconfig_.mqtt_server, &opts, ev_handler, NULL);
  if ( nc == NULL ) {
    LOG(NET_LOG,LOG_ERR, "Failed to create MQTT listener to %s\n", _aqconfig_.mqtt_server);
  } else {
    set_mqttconnecting(nc);
    reset_last_mqtt_status();
    _mqtt_exit_flag = false; // set here to stop multiple connects, if it fails truley fails it will get set to false.
  }

}
static void mg_logger(char ch, void *param) {
  
  static char buf[256];
  static size_t len;
  buf[len++] = ch;
  if (ch == '\n' || len >= sizeof(buf)) {
    //syslog(LOG_INFO, "%.*s", (int) len, buf); // Send logs
    LOG(NET_LOG, LOG_INFO, buf);
    len = 0;
    memset(buf, 0, sizeof(buf));
  }
}





bool _start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata) {
  struct mg_connection *nc;
  _aqualink_data = aqdata;
  //_aqconfig_ = aqconfig;
 
  signal(SIGTERM, net_signal_handler);
  signal(SIGINT, net_signal_handler);
  signal(SIGRESTART, net_signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);
  
  mg_log_set(_aqconfig_.mg_log_level);
  mg_log_set_fn(mg_logger, NULL);

  const char *nameserver = get_ip_address_of_nameserver();
  mg_mgr_init(mgr);

  if (nameserver != NULL)
    mgr->dns4.url = nameserver;


  //char url[256];
  //if ( strcasestr(_aqconfig_.listen_address, "http") != NULL ) {
  //  sprintf(url, "%s",_aqconfig_.listen_address);
  //} else {
  //  sprintf(url, "http://0.0.0.0:%s",_aqconfig_.listen_address);
  //}
  LOG(NET_LOG,LOG_NOTICE, "Starting web server on %s\n", _aqconfig_.listen_address);
  //nc = mg_bind(mgr, _aqconfig_.listen_address, ev_handler);
  nc = mg_http_listen(mgr, _aqconfig_.listen_address, ev_handler, mgr);
  if (nc == NULL) {
    LOG(NET_LOG,LOG_ERR, "Failed to create listener on port %s\n",_aqconfig_.listen_address);
    return false;
  }

  // Set default web options
  _http_server_opts.root_dir = _aqconfig_.web_directory;
  _http_server_opts.extra_headers = CACHE; 
  _http_server_opts.ssi_pattern = NULL;

  _http_server_opts_nocache.root_dir = _aqconfig_.web_directory;
  _http_server_opts_nocache.extra_headers = NO_CACHE;
  _http_server_opts_nocache.ssi_pattern = NULL;
  // Start MQTT
  start_mqtt(mgr);



  return true;
}


/**********************************************************************************************
 * Thread Net Services
 * 
*/

//volatile bool _broadcast = false; // This is redundent when most the fully threadded rather than option.

#define JOURNAL_FAIL_RETRY 5

void *net_services_thread( void *ptr )
{
  struct aqualinkdata *aqdata = (struct aqualinkdata *) ptr;
  int journald_fail = 0;
#ifdef DEBUG_SET_IF_CHANGED
  uint noupdate=0;
#endif

  if (!_start_net_services(&_mgr, aqdata)) {
    //LOG(NET_LOG,LOG_ERR, "Failed to start network services\n");
    // Not the best way to do this (have thread exit process), but forks for the moment.
    _keepNetServicesRunning = false;
    LOG(AQUA_LOG,LOG_ERR, "Can not start webserver on port %s.\n", _aqconfig_.listen_address);
    exit(EXIT_FAILURE);
    goto f_end;
  }

  while (_keepNetServicesRunning == true)
  {
    mg_mgr_poll(&_mgr, (_aqualink_data->simulator_active != SIM_NONE)?10:100);

    if (aqdata->is_dirty == true /*|| _broadcast == true*/) {
      _broadcast_aqualinkstate(_mgr.conns);
      CLEAR_DIRTY(aqdata->is_dirty);
#ifdef DEBUG_SET_IF_CHANGED
      printf("NO updates for %d loops\n",noupdate), noupdate=0;
    } else {
      noupdate += (noupdate < UINT_MAX);
#endif
    }
#ifdef AQ_MANAGER
// NSF, need to stop and disable after 5 tries, this just keeps looping. 
// USe something like below to notify user
// build_logmsg_JSON(msg, LOG_ERR, "Failed to open journal, giving up!", WS_LOG_LENGTH,29);
// set global variable so _broadcast_systemd_logmessages() also doesn't keep erroring.

    //if ( ! broadcast_systemd_logmessages(aqdata->aqManagerActive) && journald_fail < JOURNAL_FAIL_RETRY) {
    if ( journald_fail < JOURNAL_FAIL_RETRY && ! broadcast_systemd_logmessages(aqdata->aqManagerActive) ) {
      journald_fail++;
      LOG(AQUA_LOG,LOG_ERR, "Couldn't open systemd journal log\n");
    } else if (journald_fail == JOURNAL_FAIL_RETRY) {
      char msg[WS_LOG_LENGTH];
      build_logmsg_JSON(msg, LOG_ERR, "Giving up on journal, don't expect to see logs", WS_LOG_LENGTH,46);
      ws_send_logmsg(_mgr.conns, msg);
      journald_fail = JOURNAL_FAIL_RETRY+1;
    }
    // Reset failures when manager is not active.
    if (!aqdata->aqManagerActive) {journald_fail=0;}
/*
    if ( ! broadcast_systemd_logmessages(aqdata->aqManagerActive)) {
      LOG(AQUA_LOG,LOG_ERR, "Couldn't open systemd journal log\n");
    }
*/    
#endif
    if (aqdata->simulator_active != SIM_NONE && aqdata->simulator_packet_updated == true ) {
      _broadcast_simulator_message(_mgr.conns);
    } 
  }

f_end:
  LOG(NET_LOG,LOG_NOTICE, "Stopping network services thread\n");
  mg_mgr_free(&_mgr);

  pthread_exit(0);
}

/*
void broadcast_aqualinkstate() {
  _aqualink_data->updated = true;
}
*/
void broadcast_aqualinkstate_error(const char *msg) {
  _broadcast_aqualinkstate_error(_mgr.conns, msg);
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



