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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <libgen.h>

#include <sys/ioctl.h>
//#include <sys/socket.h>
//#include <sys/time.h>
//#include <syslog.h>
//#include <unistd.h>
#include <netdb.h>
//#include <linux/if.h>
//#include <sys/types.h>
#include <unistd.h>
#include <net/if.h>

#define CONFIG_C
#include "aqualink.h"
#include "config.h"
#include "utils.h"
#include "aq_serial.h"

#define MAXCFGLINE 256


typedef enum cfg_type{cfg_ptr, cfg_str, cfg_int, cfg_bool, cfg_hex, cfg_float, cfg_loglevel} cfg_type;

struct cfg_line {
  cfg_type type;
  void **value;
  void *value_ptr;
  char *description;
  //char name[20];
  char *name;
  int size;
};

#define SERIAL_PORT_DESC "Serial Port for RS485"
#define LOG_LEVEL_DESC "The level for logging. [DEBUG_SERIAL, DEBUG, INFO, NOTICE, WARNING, ERROR]"
#define SOCKET_PORT_DESC "Port for web interface, usually 80"
#define WEB_DIRECTORY_DESC "Root web directory, usuall /var/www/aqualinkd"
#define DEVICE_ID_DESC "Main device ID for RS485"
#define ONETOUCH_ID_DESC "Device ID for extended RS485 commands (ie variable speed pump)"
#define DEAMONIZE_DESC "Run process as service, ie deamonize?"
#define LOG_FILE_DESC "Log to a file and not syslog, use log filename ie /var/log/aqualinkd.log"
#define MQTT_DZ_SUB_TOPIC_DESC "Domoticz MQTT subscription topic (usually domoticz/out, comment out if you don't use domoticz)"
#define MQTT_DZ_PUB_TOPIC_DESC "Domoticz MQTT publish topic (usually domoticz/in, comment out if you don't use domoticz)"
#define MQTT_AQ_TOPIC_DESC "AqualinkD MQTT topic (usually aqualinkd, comment out if you don't want to publish events to MQTT"
#define MQTT_ADDRESS_DESC "MQTT Server and port (machinename:port)"
#define MQTT_USER_DESC "MQTT Username (commant out if you don't use)"
#define MQTT_PASSWD_DESC "MQTT Password (commant out if you don't use)"
#define MQTT_ID_DESC "MQTT ID. Unique identifyer for MQTT subscription, (leave blank, AqualinkD will generate one)"

#define AIR_TEMP_DZIDX_DESC "dzidx_air_temp"
#define POOL_TEMP_DZIDX_DESC "dzidx_pool_water_temp"
#define SPA_TEMP_DZIDX_DESC "dzidx_spa_water_temp"
#define SWG_PERCENT_DZIDX_DESC "SWG_percent_dzidx"
#define SWG_PPM_DZIDX_DESC "SWG_PPM_dzidx"
#define SWG_STATUS_DZIDX_DESC "SWG_Status_dzidx"

#define LIGHT_PROGM_MODE_DESC "light_programming_mode"
#define LIGHT_PROGM_INIT_ON_DESC "light_programming_initial_on"
#define LIGHT_PROGM_INIT_OFF_DESC "light_programming_initial_off"
#define LIGHT_PROGM_SPA_DESC "Button for spa light if programable. (leave blank if not programable)"
#define LIGHT_PROGM_POOL_DESC "Button for pool light if programable. (leave blank if not programable)"

#define OVERRIDE_FRZ_PROTECT_DESC "override_freeze_protect"
#define PDA_MODE_DESC "pda_mode"
#define PDA_SLEEP_MODE_DESC "pda_sleep_mode"
#define CONVERT_MQTT_TEMP_2C_DESC "convert_mqtt_temp_to_c"
#define CONVERT_DZ_TEMP_2C_DESC "convert_dz_temp_to_c"
#define REPORT_ZERO_SPA_TEMP_DESC "report_zero_spa_temp"
#define REPORT_ZERO_POOL_TEMP_DESC "report_zero_pool_temp"
#define READ_ALL_DEVICES_DESC "read_all_devices"
#define READ_PENTAIR_PACKETS_DESC "read_pentair_packets"
#define USE_PANEL_AUX_LABELS_DESC "use_panel_aux_labels"
#define FORCE_SWG_DESC "force_swg"
#define DEBUG_RSPROTOCOL_DESC "debug_RSProtocol_packets"
#define DISPLAY_WARN_WEB "display_warnings_in_web"

#define NONE TEMP_UNKNOWN

// Need to be global
void cfg_init ();
void readCfgFile(struct aqualinkdata *aqdata, char *cfgFile);
bool writeCfgFile(struct aqualinkdata *aqdata);

struct cfg_line *_cfg_lines;

char *generate_mqtt_id(char *buf, int len);
void setup_cfg_param(struct cfg_line *cfg, char *name, void *ptr, cfg_type type, char *des);
void set_cfg_param(struct cfg_line *cfg, char *name, void *ptr, void *def, cfg_type type, char *des);
bool setConfigButtonValue(struct aqconfig *cfg_prms, struct aqualinkdata *aqdata, char *param, char *value);
char *errorlevel2text(int level);

pump_detail *getpump(struct aqualinkdata *aqdata, int button);


void cfg_init () {

  set_cfg_param(&_cfg_lines[__COUNTER__], "serial_port",                  &_aqconfig_.serial_port, (void *)DEFAULT_SERIALPORT, cfg_ptr, SERIAL_PORT_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "log_level",                    &_aqconfig_.log_level,   (void *)DEFAULT_LOG_LEVEL,  cfg_loglevel, LOG_LEVEL_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "socket_port",                  &_aqconfig_.socket_port, (void *)DEFAULT_WEBPORT,    cfg_ptr, SOCKET_PORT_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "web_directory",                &_aqconfig_.web_directory,(void*)DEFAULT_WEBROOT,    cfg_ptr, WEB_DIRECTORY_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "device_id",                    &_aqconfig_.device_id,    (void*)strtoul(DEFAULT_DEVICE_ID, NULL, 16), cfg_hex, DEVICE_ID_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "extended_device_id",           &_aqconfig_.onetouch_device_id, (void*)0x00,         cfg_hex, ONETOUCH_ID_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "deamonize",                    &_aqconfig_.deamonize,    (void *)true,              cfg_bool, DEAMONIZE_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "log_file",                     &_aqconfig_.log_file,     NULL,                      cfg_ptr, LOG_FILE_DESC);

  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_aq_topic",                &_aqconfig_.mqtt_aq_topic,(void*)DEFAULT_MQTT_AQ_TP, cfg_ptr, MQTT_AQ_TOPIC_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_address",                 &_aqconfig_.mqtt_server,  NULL,                      cfg_ptr, MQTT_ADDRESS_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_user",                    &_aqconfig_.mqtt_user,    NULL,                      cfg_ptr, MQTT_USER_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_passwd",                  &_aqconfig_.mqtt_passwd,  NULL,                      cfg_ptr, MQTT_PASSWD_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_ID",                      &_aqconfig_.mqtt_ID,      NULL,                      cfg_str, MQTT_ID_DESC);

  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_dz_sub_topic",            &_aqconfig_.mqtt_dz_sub_topic,  NULL,                cfg_ptr, MQTT_DZ_SUB_TOPIC_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "mqtt_dz_pub_topic",            &_aqconfig_.mqtt_dz_pub_topic,  NULL,                cfg_ptr, MQTT_DZ_PUB_TOPIC_DESC);
  
  set_cfg_param(&_cfg_lines[__COUNTER__], "air_temp_dzidx",               &_aqconfig_.dzidx_air_temp,        NULL,        cfg_int, AIR_TEMP_DZIDX_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "pool_water_temp_dzidx",        &_aqconfig_.dzidx_pool_water_temp, NULL,        cfg_int, POOL_TEMP_DZIDX_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "spa_water_temp_dzidx",         &_aqconfig_.dzidx_spa_water_temp,  NULL,        cfg_int, SPA_TEMP_DZIDX_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "SWG_percent_dzidx",            &_aqconfig_.dzidx_swg_percent,     NULL,        cfg_int, SWG_PERCENT_DZIDX_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "SWG_PPM_dzidx",                &_aqconfig_.dzidx_swg_ppm,         NULL,        cfg_int, SWG_PPM_DZIDX_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "SWG_Status_dzidx",             &_aqconfig_.dzidx_swg_status,      NULL,        cfg_int, SWG_STATUS_DZIDX_DESC);
  
  set_cfg_param(&_cfg_lines[__COUNTER__], "light_programming_mode",       &_aqconfig_.light_programming_mode,        (void*)0, cfg_float, LIGHT_PROGM_MODE_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "light_programming_initial_on", &_aqconfig_.light_programming_initial_on,  (void*)15, cfg_int, LIGHT_PROGM_INIT_ON_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "light_programming_initial_off",&_aqconfig_.light_programming_initial_off, (void*)12, cfg_int, LIGHT_PROGM_INIT_OFF_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "light_programming_button_pool",&_aqconfig_.light_programming_button_pool, NULL, cfg_int, LIGHT_PROGM_POOL_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "light_programming_button_spa", &_aqconfig_.light_programming_button_spa,  NULL, cfg_int, LIGHT_PROGM_SPA_DESC);
 
  set_cfg_param(&_cfg_lines[__COUNTER__], "override_freeze_protect",      &_aqconfig_.override_freeze_protect,  (void*)false, cfg_bool, OVERRIDE_FRZ_PROTECT_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "pda_mode",                     &_aqconfig_.pda_mode,                 (void*)false, cfg_bool, PDA_MODE_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "pda_sleep_mode",               &_aqconfig_.pda_sleep_mode,           (void*)false, cfg_bool, PDA_SLEEP_MODE_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "convert_mqtt_temp_to_c",       &_aqconfig_.convert_mqtt_temp,        (void*)true,  cfg_bool, CONVERT_MQTT_TEMP_2C_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "convert_dz_temp_to_c",         &_aqconfig_.convert_dz_temp,          (void*)true,  cfg_bool, CONVERT_DZ_TEMP_2C_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "report_zero_spa_temp",         &_aqconfig_.report_zero_spa_temp,     (void*)false, cfg_bool, REPORT_ZERO_SPA_TEMP_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "report_zero_pool_temp",        &_aqconfig_.report_zero_pool_temp,    (void*)false, cfg_bool, REPORT_ZERO_POOL_TEMP_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "read_all_devices",             &_aqconfig_.read_all_devices,         (void*)true,  cfg_bool, READ_ALL_DEVICES_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "read_pentair_packets",         &_aqconfig_.read_pentair_packets,     (void*)true,  cfg_bool, READ_PENTAIR_PACKETS_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "use_panel_aux_labels",         &_aqconfig_.use_panel_aux_labels,     (void*)false, cfg_bool, USE_PANEL_AUX_LABELS_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "force_SWG",                    &_aqconfig_.force_swg,                (void*)false, cfg_bool, FORCE_SWG_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "debug_RSProtocol_packets",     &_aqconfig_.debug_RSProtocol_packets, (void*)false, cfg_bool, DEBUG_RSPROTOCOL_DESC);
  set_cfg_param(&_cfg_lines[__COUNTER__], "display_warnings_in_web",      &_aqconfig_.display_warnings_web,     (void*)true,  cfg_bool, DISPLAY_WARN_WEB);
}

#define CFG_LEN __COUNTER__

// NSF, need to fix why passing 0 for def doesn't work, but every other number does
// NULL expands to (void *)0, not sure how to overcome that.
void set_cfg_param(struct cfg_line *cfg, char *name, void *ptr, void *def, cfg_type type, char *des) {

  cfg->type = type;
  cfg->name = name;
  cfg->description = des;   

  switch (type) {
    
    case cfg_ptr:  
      cfg->value = ptr;
    break;

    case cfg_str:
    case cfg_bool:
    case cfg_int:
    case cfg_float:
    case cfg_loglevel:
    case cfg_hex:
      cfg->value_ptr = ptr;
      cfg->value = cfg->value_ptr;
    break;
  }

  *cfg->value = def;  
}


void readCfgFile(struct aqualinkdata *aqdata, char *cfgFile)
{
  FILE *fp;
  char bufr[MAXCFGLINE];
  char *b_ptr;
  int i;
  bool matched = false;

  _aqconfig_.config_file = cleanalloc(cfgFile);

  if ((fp = fopen(cfgFile, "r")) != NULL)
  {
    while (!feof(fp))
    {
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        char *indx;
        // Eat leading whitespace
        while (isspace(*b_ptr))
          b_ptr++;

        if (b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          matched = false;
          indx = strchr(b_ptr, '=');
          if (indx != NULL)
          {
            // key = b_ptr
            // value = indx+1
            for (i = 0; i < CFG_LEN; i++)
            {
              if (strncmp(b_ptr, _cfg_lines[i].name, strlen(_cfg_lines[i].name)) == 0)
              {
                matched = true;
                switch (_cfg_lines[i].type)
                {
                case cfg_ptr:
                  *_cfg_lines[i].value = cleanalloc(indx + 1);
                  break;
                case cfg_str:
                  sprintf((char *)_cfg_lines[i].value, stripwhitespace(indx + 1));
                  break;
                case cfg_bool:
                  *((bool *)_cfg_lines[i].value) = text2bool(indx + 1);
                  break;
                case cfg_int:
                  *((int *)_cfg_lines[i].value) = strtoul(indx + 1, NULL, 10);
                  break;
                case cfg_float:
                  *((float *)_cfg_lines[i].value) = atof(stripwhitespace(indx + 1));
                  break;
                case cfg_loglevel:
                  *((unsigned long *)_cfg_lines[i].value) = text2elevel(stripwhitespace(indx + 1));
                  break;
                case cfg_hex:
                  *((unsigned char *)_cfg_lines[i].value) = strtoul(cleanalloc(indx + 1), NULL, 16);
                  break;
                }
              }
            }
            if (!matched)
            {
              if ( ! setConfigButtonValue(&_aqconfig_, aqdata, b_ptr, indx+1))
                logMessage(LOG_ERR, "Unknown config parameter '%.*s'\n",strlen(b_ptr)-1, b_ptr);
            }
          }
        }
      }
    }
    fclose(fp);
  }
  else
  {
    /* error processing, couldn't open file */
    logMessage(LOG_ERR, "Error reading config file '%s'\n", cfgFile);
    displayLastSystemError(cfgFile);
    exit(EXIT_FAILURE);
  }
}

bool writeCfgFile(struct aqualinkdata *aqdata)
{
  FILE *fp;
  char bufr[MAXCFGLINE*5];
  int i = 0;
  int pi=0;
  int indx = 0;

  _aqconfig_.config_file = "crap.cfg";

  //fclose(fp);

  if ((fp = fopen(_aqconfig_.config_file, "w")) == NULL)
  {
    /* error processing, couldn't open file */
    logMessage(LOG_ERR, "Error writing config file '%s'\n", _aqconfig_.config_file);
    displayLastSystemError(_aqconfig_.config_file);
    return false;
  }
  
  for (i = 0; i < CFG_LEN; i++)
  {
    if (_cfg_lines[i].name == NULL)
    {
      continue;
    }
    
    indx = sprintf(bufr, "#%s\n%s = ", _cfg_lines[i].description, _cfg_lines[i].name);  
      
    switch (_cfg_lines[i].type)
    {
      case cfg_bool:
        indx += sprintf(&bufr[indx], "%s\n\n", *((bool *)_cfg_lines[i].value) == true ? "Yes" : "No");  
      break;
      case cfg_str:
        indx += sprintf(&bufr[indx], "%s\n\n", (char *)_cfg_lines[i].value);
      break;
      case cfg_ptr:
        indx += sprintf(&bufr[indx], "%s\n\n",(char *)*_cfg_lines[i].value!=NULL?*_cfg_lines[i].value:"");
      break;
      case cfg_int:
        indx += sprintf(&bufr[indx], "%d\n\n",*((int *)_cfg_lines[i].value));
      break;
      case cfg_hex:
        indx += sprintf(&bufr[indx], "0x%02hhx\n\n",*((unsigned char *)_cfg_lines[i].value));
      break;
      case cfg_float:
        indx += sprintf(&bufr[indx], "%.2f\n\n",*((float *)_cfg_lines[i].value));
      break;
      case cfg_loglevel:
        indx += sprintf(&bufr[indx], "%s\n\n",errorlevel2text(*((int *)_cfg_lines[i].value)));
      break;
    }
    //printf("%s",bufr);
    fwrite(bufr,sizeof(char), strlen(bufr), fp);
  }

  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    indx = sprintf(bufr, "button_%.2d_label = %s\n", i+1, aqdata->aqbuttons[i].label);
    indx += sprintf(&bufr[indx], "button_%.2d_dzidx = %d\n", i+1, aqdata->aqbuttons[i].dz_idx);
    if (aqdata->aqbuttons[i].pda_label != NULL)
      indx += sprintf(&bufr[indx], "button_%.2d_PDA_label = %s\n", i+1, aqdata->aqbuttons[i].pda_label);
  
    for(pi=0; pi < MAX_PUMPS; pi++) {
      if ( aqdata->pumps[pi].button == &aqdata->aqbuttons[i] ) {
        indx += sprintf(&bufr[indx],"button_%.2d_pumpID = 0x%02hhx\n", i+1, aqdata->pumps[pi].pumpID);
        indx += sprintf(&bufr[indx],"button_%.2d_pumpIndex = %d\n", i+1, aqdata->pumps[pi].pumpIndex);
      }
    }

    //button_04_pumpID=0x79
    //button_04_pumpIndex=2
    
    indx += sprintf(&bufr[indx],"\n");

    //printf("%s",bufr);
    fwrite(bufr,sizeof(char), strlen(bufr), fp);
  }

  fclose(fp);
  
  return true;
}


void print_cfg(struct aqualinkdata *aqdata)
{
  int i,j;

  logMessage(LOG_NOTICE, "+----------------------------   Configuration   ----------------------------+\n");
  for (i = 0; i < CFG_LEN; i++)
  {
    if (_cfg_lines[i].name == NULL)
    {
      continue;
    }
    switch (_cfg_lines[i].type)
    {
      case cfg_bool:
        logMessage(LOG_NOTICE, "Config %30s = %s\n", _cfg_lines[i].name, *((bool *)_cfg_lines[i].value) == true ? "Yes" : "No");  
      break;
      case cfg_str:
        logMessage(LOG_NOTICE, "Config %30s = %s\n",_cfg_lines[i].name,(char *)_cfg_lines[i].value);
      break;
      case cfg_ptr:
        logMessage(LOG_NOTICE, "Config %30s = %s\n",_cfg_lines[i].name,(char *)*_cfg_lines[i].value!=NULL?*_cfg_lines[i].value:"");
      break;
      case cfg_int:
        logMessage(LOG_NOTICE, "Config %30s = %d\n",_cfg_lines[i].name,*((int *)_cfg_lines[i].value));
      break;
      case cfg_hex:
        logMessage(LOG_NOTICE, "Config %30s = 0x%02hhx\n",_cfg_lines[i].name,*((unsigned char *)_cfg_lines[i].value));
      break;
      case cfg_float:
        logMessage(LOG_NOTICE, "Config %30s = %.2f\n",_cfg_lines[i].name,*((float *)_cfg_lines[i].value));
      break;
      case cfg_loglevel:
        //printf("Config %30s = %lu\n",_cfg_lines[i].name,*((unsigned long *)_cfg_lines[i].value));
       logMessage(LOG_NOTICE, "Config %30s = %s\n",_cfg_lines[i].name,elevel2text((int)*_cfg_lines[i].value));
      break;
    }
  }

  logMessage(LOG_NOTICE, "+------------------------------ Button Labels & Pumps -------------------------------+\n");
  for (i = 0; i < TOTAL_BUTTONS; i++)
  {
    char vsp[] = "None";
    int alid = 0;
    for (j = 0; j < aqdata->num_pumps; j++) {
      //if (_aqualink_data.pumps[j].buttonID == i) {
      if (aqdata->pumps[j].button == &aqdata->aqbuttons[i]) {
        sprintf(vsp,"0x%02hhx",aqdata->pumps[j].pumpID);
        alid = aqdata->pumps[j].pumpIndex;
        //printf("Pump %d %d %d\n",_aqualink_data.pumps[j].pumpID, _aqualink_data.pumps[j].buttonID, _aqualink_data.pumps[j].ptype);
      }
    }
    if (!_aqconfig_.pda_mode) {
      logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | VSP ID %-4s | AL ID %-1d | dzidx %-3d | %s\n", 
                           aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label, vsp, alid, aqdata->aqbuttons[i].dz_idx,
                          (i>0 && (i==_aqconfig_.light_programming_button_pool || i==_aqconfig_.light_programming_button_spa)?"Programable":"")  );
    } else {
      logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | VSP ID %-4s | AL ID %-1d | PDAlabel %-15s | dzidx %d\n", 
                           aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label, vsp, alid,
                          aqdata->aqbuttons[i].pda_label, aqdata->aqbuttons[i].dz_idx  );
    }
    //logMessage(LOG_NOTICE, "Button %d\n", i+1, _aqualink_data.aqbuttons[i].label , _aqualink_data.aqbuttons[i].dz_idx);
  }
}




void print_json_cfg(struct aqualinkdata *aqdata)
{
  char bufr[MAXCFGLINE*100];
  int i = 0;
  int pi=0;
  int indx = 0;
  
  indx += sprintf(&bufr[indx], "{\"config\":[");

  for (i = 0; i < CFG_LEN; i++)
  {
    if (_cfg_lines[i].name == NULL)
    {
      continue;
    }
      
    indx += sprintf(&bufr[indx], "{\"name\":\"%s\",\"desc\":\"%s\",", _cfg_lines[i].name,_cfg_lines[i].description);  
      
    switch (_cfg_lines[i].type)
    {
      case cfg_bool:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%s\"},", "bool", *((bool *)_cfg_lines[i].value) == true ? "Yes" : "No");  
      break;
      case cfg_str:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%s\"},", "string", (char *)_cfg_lines[i].value);
      break;
      case cfg_ptr:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%s\"},", "ptr", (char *)*_cfg_lines[i].value!=NULL?*_cfg_lines[i].value:"");
      break;
      case cfg_int:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%d\"},", "int", *((int *)_cfg_lines[i].value));
      break;
      case cfg_hex:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"0x%02hhx\"},","hex",*((unsigned char *)_cfg_lines[i].value));
      break;
      case cfg_float:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%.2f\"},","float",*((float *)_cfg_lines[i].value));
      break;
      case cfg_loglevel:
        indx += sprintf(&bufr[indx], "\"type\":\"%s\",\"value\":\"%s\"},","loglevel",errorlevel2text(*((int *)_cfg_lines[i].value)));
      break;
      default:
        indx += sprintf(&bufr[indx], "},");
      break;
    }
  }

  if (bufr[indx-1] == ',')
    indx--;
  indx += sprintf(&bufr[indx], "], \"buttons\":[");

  for (i = 0; i < TOTAL_BUTTONS; i++)
  {
    int j;
    unsigned char vsp = NULL;
    int alid = 0;
    for (j = 0; j < aqdata->num_pumps; j++) {
      if (aqdata->pumps[j].button == &aqdata->aqbuttons[i]) {
        vsp = aqdata->pumps[j].pumpID;
        alid = aqdata->pumps[j].pumpIndex;
      }
    }
    if (!_aqconfig_.pda_mode) {
      indx += sprintf(&bufr[indx], "{\"button\":\"%d\",\"label\":\"%s\",\"dzidx\":\"%d\"",i,aqdata->aqbuttons[i].label,aqdata->aqbuttons[i].dz_idx);
      if (vsp != NULL)
        indx += sprintf(&bufr[indx], ",\"pumpID\":\"0x%02hhx\"",vsp);
      if (alid > 0)
        indx += sprintf(&bufr[indx], ",\"pumpIndex\":\"%d\"",alid);

      indx += sprintf(&bufr[indx], "},");
    } else {
      logMessage(LOG_NOTICE, "Config BTN %-13s = label %-15s | VSP ID %-4s | AL ID %-1d | PDAlabel %-15s | dzidx %d\n", 
                           aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label, vsp, alid,
                          aqdata->aqbuttons[i].pda_label, aqdata->aqbuttons[i].dz_idx  );
    }
  }


  if (bufr[indx-1] == ',')
    indx--;
  indx += sprintf(&bufr[indx], "]}\n");

  printf("%s\n",bufr);
}


void init_config(struct aqualinkdata *aqdata, char *cfgFile) {
  //struct aqconfig cfg_prms;
  //char name[50];
  //struct aqualinkdata aqdata;

  //memset(aqdata, 0, sizeof(struct aqualinkdata));
  _cfg_lines = (struct cfg_line*)malloc(sizeof(struct cfg_line) * CFG_LEN );

  initButtons(aqdata);
  //sprintf(name, "./release/aqualinkd.test.conf");

  cfg_init();
  print_cfg(aqdata);
  //printf("cfg MQTT CP Address=%p value='%s'\n",&cfg_prms.mqtt_ID,cfg_prms.mqtt_ID);
  //printf("\n");

  readCfgFile(aqdata, cfgFile);
  
  // (re)set any params 
  if (strlen(_aqconfig_.mqtt_ID) <= 0)
    generate_mqtt_id(_aqconfig_.mqtt_ID, MQTT_ID_LEN);


  if (_aqconfig_.device_id >= 0x60 && _aqconfig_.device_id <= 0x63) {
    _aqconfig_.pda_mode = true;
  }

  print_cfg(aqdata);

  print_json_cfg(aqdata);

  //writeCfgFile(cfg_prms, aqdata);

  /* 
  printf("Force SWG = %s\n",cfg_prms->force_swg==true?"Yes":"No");
  printf("Read All Devices = %s\n",cfg_prms->read_all_devices==true?"Yes":"No");
  printf("Zero Spa Temp = %s\n",cfg_prms->report_zero_spa_temp==true?"Yes":"No");
  printf("Read Pentair = %s\n",cfg_prms->read_pentair_packets==true?"Yes":"No");
*/
  //print_cfg();
}



int main(int argc, char *argv[]) {
  //struct aqconfig cfg_prms;
  
  char name[50];
  
  struct aqualinkdata aqdata;
  memset(&aqdata, 0, sizeof(aqdata));

  sprintf(name, "./release/aqualinkd.test.conf");
  
  setLoggingPrms(10 , false, NULL, NULL);
  
  init_config(&aqdata, name);

  writeCfgFile(&aqdata);
}


char *cleanalloc(char*str)
{
  char *result;
  str = cleanwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
}

// Find the first network interface with valid MAC and put mac address into buffer upto length
bool mac(char *buf, int len)
{
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct if_nameindex *if_nidxs, *intf;

  if_nidxs = if_nameindex();
  if (if_nidxs != NULL)
  {
    for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++)
    {
      strcpy(s.ifr_name, intf->if_name);
      if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
      {
        int i;
        if ( s.ifr_addr.sa_data[0] == 0 &&
             s.ifr_addr.sa_data[1] == 0 &&
             s.ifr_addr.sa_data[2] == 0 &&
             s.ifr_addr.sa_data[3] == 0 &&
             s.ifr_addr.sa_data[4] == 0 &&
             s.ifr_addr.sa_data[5] == 0 ) {
          continue;
        }
        for (i = 0; i < 6 && i * 2 < len; ++i)
        {
          sprintf(&buf[i * 2], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
        }
        return true;
      }
    }
  }

  return false;
}

char *generate_mqtt_id(char *buf, int len) {
  extern char *__progname; // glibc populates this
  int i;
  strncpy(buf, basename(__progname), len);
  i = strlen(buf);

  if (i < len) {
    buf[i++] = '_';
    // If we can't get MAC to pad mqtt id then use PID
    if (!mac(&buf[i], len - i)) {
      sprintf(&buf[i], "%.*d", (len-i), getpid());
    }
  }

  buf[len] = '\0';

  return buf;
}

bool setConfigButtonValue(struct aqconfig *cfg_prms, struct aqualinkdata *aqdata, char *param, char *value) {
  bool rtn = false;
  static int pi=0;

   if (strncasecmp(param, "button_", 7) == 0) {
    int num = strtoul(param + 7, NULL, 10) - 1;
    if (strncasecmp(param + 9, "_label", 6) == 0) {
      aqdata->aqbuttons[num].label = cleanalloc(value);
      rtn=true;
    } else if (strncasecmp(param + 9, "_dzidx", 6) == 0) {
      aqdata->aqbuttons[num].dz_idx = strtoul(value, NULL, 10);
      rtn=true;
    } else if (strncasecmp(param + 9, "_PDA_label", 10) == 0) {
      aqdata->aqbuttons[num].pda_label = cleanalloc(value);
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpID", 7) == 0) {
      pump_detail *pump = getpump(aqdata, num);
      if (pump != NULL) {
        pump->pumpID = strtoul(cleanalloc(value), NULL, 16);
        if (pump->pumpID < 119) {
          pump->ptype = PENTAIR;
        } else {
          pump->ptype = JANDY;
          //pump->pumpType = EPUMP; // For testing let the interface set this
        }
      } else {
        logMessage(LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS-1,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpIndex", 10) == 0) { //button_01_pumpIndex=1
      pump_detail *pump = getpump(aqdata, num);
      if (pump != NULL) {
        pump->pumpIndex = strtoul(value, NULL, 10);
      } else {
        logMessage(LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS-1,param);
      }
      rtn=true;
    }
  }

  return rtn;
}


pump_detail *getpump(struct aqualinkdata *aqdata, int button)
{
  //static int _pumpindex = 0;
  //aqdata->num_pumps
  int pi;

  // Does it exist
  for (pi=0; pi < aqdata->num_pumps; pi++) {
    if (aqdata->pumps[pi].button == &aqdata->aqbuttons[button]) {
      //printf ("Found pump %d\n",button);
      return &aqdata->pumps[pi];
    }
  }

  // Create new entry
  if (aqdata->num_pumps < MAX_PUMPS) {
    //printf ("Creating pump %d\n",button);
    aqdata->pumps[aqdata->num_pumps].button = &aqdata->aqbuttons[button];
    aqdata->pumps[aqdata->num_pumps].pumpType = PT_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].rpm = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].watts = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].gpm = TEMP_UNKNOWN;
    aqdata->num_pumps++;
    return &aqdata->pumps[aqdata->num_pumps-1];
  }

  return NULL;
}





//DEBUG_DERIAL, DEBUG, INFO, NOTICE, WARNING, ERROR
char *errorlevel2text(int level) 
{
  switch(level) {
    case LOG_DEBUG_SERIAL:
      return "DEBUG_SERIAL";
      break;
    case LOG_DEBUG:
      return "DEBUG";
      break;
    case LOG_INFO:
      return "INFO";
      break;
    case LOG_NOTICE:
      return "NOTICE";
      break;
    case LOG_WARNING:
      return "WARNING";
      break;
    case LOG_ERR:
     default:
      return "ERROR";
      break;
  }
  
  return "";
}

bool remount_root_ro(bool readonly) {
  // NSF Check if config is RO_ROOT set
  if (readonly) {} // Dummy to stop compile warnings.
/*
  if (readonly) {
    logMessage(LOG_INFO, "reMounting root RO\n");
    mount (NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    return true;
  } else {
    struct statvfs fsinfo;
    statvfs("/", &fsinfo);
    if ((fsinfo.f_flag & ST_RDONLY) == 0) // We are readwrite, ignore
      return false;

    logMessage(LOG_INFO, "reMounting root RW\n");
    mount (NULL, "/", NULL, MS_REMOUNT, NULL);
    return true;
  }
*/  
 return true;
}
