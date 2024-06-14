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
#include <errno.h>

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
#include "config.h"
#include "utils.h"
#include "aq_serial.h"
#include "aq_panel.h"
#include "aqualink.h"

#define MAXCFGLINE 256



char *generate_mqtt_id(char *buf, int len);
pump_detail *getpump(struct aqualinkdata *aqdata, int button);

struct tmpPanelInfo {
  int size;
  bool rs;
  bool combo;
  bool dual;
};

struct tmpPanelInfo *_tmpPanel;

/*
* initialize data to default values
*/
void init_parameters (struct aqconfig * parms)
{
  // Set default panel if it get's missed from config
  _tmpPanel = malloc(sizeof(struct tmpPanelInfo));
  _tmpPanel->size = 8;
  _tmpPanel->rs = true;
  _tmpPanel->combo = true;
  _tmpPanel->dual = false;

  clearDebugLogMask();

  //int i;
  //char *p;
  //parms->rs_panel_size = 8;
  parms->serial_port = DEFAULT_SERIALPORT;
  parms->log_level = DEFAULT_LOG_LEVEL;
  parms->socket_port = DEFAULT_WEBPORT;
  parms->web_directory = DEFAULT_WEBROOT;
  //parms->device_id = strtoul(DEFAULT_DEVICE_ID, &p, 16);
  parms->device_id = strtoul(DEFAULT_DEVICE_ID, NULL, 16);
  parms->rssa_device_id = NUL;
  parms->RSSD_LOG_filter = NUL;
  parms->paneltype_mask = 0;
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  parms->extended_device_id = NUL;
  parms->extended_device_id_programming = false;
#endif
  //sscanf(DEFAULT_DEVICE_ID, "0x%x", &parms->device_id);
  parms->override_freeze_protect = FALSE;

  parms->mqtt_dz_sub_topic = DEFAULT_MQTT_DZ_OUT;
  parms->mqtt_dz_pub_topic = DEFAULT_MQTT_DZ_IN;
  parms->mqtt_hass_discover_topic = DEFAULT_HASS_DISCOVER;
  parms->mqtt_aq_topic = DEFAULT_MQTT_AQ_TP;
  parms->mqtt_server = DEFAULT_MQTT_SERVER;
  parms->mqtt_user = DEFAULT_MQTT_USER;
  parms->mqtt_passwd = DEFAULT_MQTT_PASSWD;

  parms->dzidx_air_temp = TEMP_UNKNOWN;
  parms->dzidx_pool_water_temp = TEMP_UNKNOWN;
  parms->dzidx_spa_water_temp = TEMP_UNKNOWN;
  parms->dzidx_swg_percent = TEMP_UNKNOWN;
  parms->dzidx_swg_ppm = TEMP_UNKNOWN;
  //parms->dzidx_pool_thermostat = TEMP_UNKNOWN; // removed until domoticz has a better virtual thermostat
  //parms->dzidx_spa_thermostat = TEMP_UNKNOWN; // removed until domoticz has a better virtual thermostat
  parms->light_programming_mode = 0;
  parms->light_programming_initial_on = 15;
  parms->light_programming_initial_off = 12;
  //parms->light_programming_button_pool = TEMP_UNKNOWN;
  //parms->light_programming_button_spa = TEMP_UNKNOWN;
  parms->deamonize = true;
#ifndef AQ_MANAGER
  parms->log_file = '\0';
#endif
#ifdef AQ_PDA
  parms->pda_sleep_mode = false;
#endif
  //parms->onetouch_mode = false;
  parms->convert_mqtt_temp = true;
  parms->convert_dz_temp = true;
  parms->report_zero_pool_temp = false;
  parms->report_zero_spa_temp = false;
  //parms->read_all_devices = true;
  //parms->read_pentair_packets = false;
  parms->read_RS485_devmask = 0;
  parms->use_panel_aux_labels = false;
  
  parms->force_swg = false;
  parms->force_ps_setpoints = false;
  parms->force_frzprotect_setpoints = false;
  parms->force_chem_feeder = false;
  //parms->swg_pool_and_spa = false;
  parms->swg_zero_ignore = DEFAULT_SWG_ZERO_IGNORE_COUNT;
  parms->display_warnings_web = false;

  parms->log_protocol_packets = false; // Read & Write as packets write to file
  parms->log_raw_bytes = false; // bytes read and write to file
  
  parms->sync_panel_time = true;

#ifdef AQ_NO_THREAD_NETSERVICE
  parms->rs_poll_speed = DEFAULT_POLL_SPEED;
  parms->thread_netservices = true;
#endif

  parms->enable_scheduler = true;
  parms->ftdi_low_latency = true;
  parms->frame_delay = 0;
  parms->device_pre_state = true;
  
  generate_mqtt_id(parms->mqtt_ID, MQTT_ID_LEN);
}

char *cleanalloc(char*str)
{
  if (str == NULL)
    return str;

  char *result;
  str = cleanwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
}
/*
char *cleanallocindex(char*str, int index)
{
  char *result;
  int i;
  int found = 1;
  int loc1=0;
  int loc2=strlen(str);
  
  for(i=0;i<loc2;i++) {
    if ( str[i] == ';' ) {
      found++;
      if (found == index)
        loc1 = i;
      else if (found == (index+1))
        loc2 = i;
    }
  }
  
  if (found < index)
    return NULL;

  // Trim leading & trailing spaces
  loc1++;
  while(isspace(str[loc1])) loc1++;
  loc2--;
  while(isspace(str[loc2])) loc2--;
  
  // Allocate and copy
  result = (char*)malloc(loc2-loc1+2*sizeof(char));
  strncpy ( result, &str[loc1], loc2-loc1+1 );
  result[loc2-loc1+1] = '\0';

  return result;
}
*/
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
/*
void readCfg_OLD (struct aqconfig *config_parameters, struct aqualinkdata *aqdata, char *cfgFile)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  //const char delim[2] = ";";
  //char *buf;
  //int line = 0;
  //int tokenindex = 0;
  char *b_ptr;

  if( (fp = fopen(cfgFile, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        char *indx;
        // Eat leading whitespace
        while(isspace(*b_ptr)) b_ptr++;
        if ( b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          indx = strchr(b_ptr, '=');  
          if ( indx != NULL) 
          {
            if (strncasecmp (b_ptr, "socket_port", 11) == 0) {
              //_aqconfig_.socket_port = cleanint(indx+1);
              _aqconfig_.socket_port = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "serial_port", 11) == 0) {
              _aqconfig_.serial_port = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_level", 9) == 0) {
              _aqconfig_.log_level = text2elevel(cleanalloc(indx+1));
              // should fee mem here
            } else if (strncasecmp (b_ptr, "device_id", 9) == 0) {
              _aqconfig_.device_id = strtoul(cleanalloc(indx+1), NULL, 16);
              // should fee mem here
            } else if (strncasecmp (b_ptr, "web_directory", 13) == 0) {
              _aqconfig_.web_directory = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_file", 8) == 0) {
              _aqconfig_.log_file = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_address", 12) == 0) {
              _aqconfig_.mqtt_server = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_dz_sub_topic", 17) == 0) {
              _aqconfig_.mqtt_dz_sub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_dz_pub_topic", 17) == 0) {
              _aqconfig_.mqtt_dz_pub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_aq_topic", 13) == 0) {
              _aqconfig_.mqtt_aq_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_user", 9) == 0) {
              _aqconfig_.mqtt_user = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_passwd", 11) == 0) {
              _aqconfig_.mqtt_passwd = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "air_temp_dzidx", 14) == 0) {
              _aqconfig_.dzidx_air_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "pool_water_temp_dzidx", 21) == 0) {
              _aqconfig_.dzidx_pool_water_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "spa_water_temp_dzidx", 20) == 0) {
              _aqconfig_.dzidx_spa_water_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "light_programming_mode", 21) == 0) {
              _aqconfig_.light_programming_mode = atof(cleanalloc(indx+1)); // should free this
            } else if (strncasecmp (b_ptr, "light_programming_initial_on", 28) == 0) {
              _aqconfig_.light_programming_initial_on = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "light_programming_initial_off", 29) == 0) {
              _aqconfig_.light_programming_initial_off = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "light_programming_button", 21) == 0) {
              _aqconfig_.light_programming_button = strtoul(indx+1, NULL, 10) - 1;
            } else if (strncasecmp (b_ptr, "SWG_percent_dzidx", 17) == 0) {
              _aqconfig_.dzidx_swg_percent = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "SWG_PPM_dzidx", 13) == 0) {
              _aqconfig_.dzidx_swg_ppm = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "SWG_Status_dzidx", 14) == 0) {
              _aqconfig_.dzidx_swg_status = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "override_freeze_protect", 23) == 0) {
              _aqconfig_.override_freeze_protect = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "pda_mode", 8) == 0) {
              _aqconfig_.pda_mode = text2bool(indx+1);
              set_pda_mode(_aqconfig_.pda_mode);
            } else if (strncasecmp (b_ptr, "convert_mqtt_temp_to_c", 22) == 0) {
              _aqconfig_.convert_mqtt_temp = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "convert_dz_temp_to_c", 21) == 0) {
              _aqconfig_.convert_dz_temp = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "flash_mqtt_buttons", 18) == 0) {
              _aqconfig_.flash_mqtt_buttons = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "report_zero_pool_temp", 21) == 0) {
              _aqconfig_.report_zero_pool_temp = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "report_zero_spa_temp", 20) == 0) {
              _aqconfig_.report_zero_spa_temp = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "report_zero_pool_temp", 21) == 0) {
              _aqconfig_.report_zero_pool_temp = text2bool(indx+1);
            } else if (strncasecmp (b_ptr, "button_", 7) == 0) {
              int num = strtoul(b_ptr+7, NULL, 10) - 1;
              //logMessage (LOG_DEBUG, "Button %d\n", strtoul(b_ptr+7, NULL, 10));
              if (strncasecmp (b_ptr+9, "_label", 6) == 0) {
                //logMessage (LOG_DEBUG, "     Label %s\n", cleanalloc(indx+1));
                aqdata->aqbuttons[num].label = cleanalloc(indx+1);
              } else if (strncasecmp (b_ptr+9, "_dzidx", 6) == 0) {
                //logMessage (LOG_DEBUG, "     dzidx %d\n", strtoul(indx+1, NULL, 10));
                aqdata->aqbuttons[num].dz_idx = strtoul(indx+1, NULL, 10);
              } else if (strncasecmp (b_ptr+9, "_PDA_label", 10) == 0) {
                //logMessage (LOG_DEBUG, "     dzidx %d\n", strtoul(indx+1, NULL, 10));
                aqdata->aqbuttons[num].pda_label = cleanalloc(indx+1);
              }
            }
          } 
          //line++;
        }
      }
    }
    fclose(fp);
  } else {
    
    displayLastSystemError(cfgFile);
    exit (EXIT_FAILURE);
  }
}
*/

bool setConfigValue(struct aqualinkdata *aqdata, char *param, char *value) {
  bool rtn = false;

  if (strncasecmp(param, "debug_log_mask", 14) == 0) {
    addDebugLogMask(strtoul(value, NULL, 10));
    rtn=true;
  } else if (strncasecmp(param, "socket_port", 11) == 0) {
    _aqconfig_.socket_port = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "serial_port", 11) == 0) {
    _aqconfig_.serial_port = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "log_level", 9) == 0) {
    _aqconfig_.log_level = text2elevel(cleanalloc(value));
    rtn=true;
  } else if (strncasecmp(param, "device_id", 9) == 0) {
    _aqconfig_.device_id = strtoul(cleanalloc(value), NULL, 16);
    rtn=true;
  } else if (strncasecmp(param, "rssa_device_id", 14) == 0) {
    _aqconfig_.rssa_device_id = strtoul(cleanalloc(value), NULL, 16);
    rtn=true;
  } else if (strncasecmp(param, "RSSD_LOG_filter", 15) == 0) {
    _aqconfig_.RSSD_LOG_filter = strtoul(cleanalloc(value), NULL, 16);
    rtn=true;
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  } else if (strncasecmp (param, "extended_device_id_programming", 30) == 0) {
    // Has to be before the below.
    _aqconfig_.extended_device_id_programming = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "extended_device_id", 9) == 0) {
    _aqconfig_.extended_device_id = strtoul(cleanalloc(value), NULL, 16);
    //_config_parameters.onetouch_device_id != 0x00
    rtn=true;
#endif
  } else if (strncasecmp(param, "panel_type_size", 15) == 0) {
    _tmpPanel->size = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_combo", 16) == 0) {
    _tmpPanel->combo = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_dual", 15) == 0) {
    _tmpPanel->dual = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_pda", 14) == 0) {
    _tmpPanel->rs = !text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_rs", 13) == 0) {
    _tmpPanel->rs = text2bool(value);
    rtn=true;
   } else if (strncasecmp(param, "panel_type", 10) == 0) { // This must be last so it doesn't get picked up by other settings
    setPanelByName(aqdata, cleanwhitespace(value));
    rtn=true;
  } else if (strncasecmp(param, "rs_panel_size", 13) == 0) {
    LOG(AQUA_LOG,LOG_WARNING, "Config error, 'rs_panel_size' no longer supported, please use 'panel_type'\n");
    _tmpPanel->size = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "web_directory", 13) == 0) {
    _aqconfig_.web_directory = cleanalloc(value);
    rtn=true;
#ifndef AQ_MANAGER
  } else if (strncasecmp(param, "log_file", 8) == 0) {
    _aqconfig_.log_file = cleanalloc(value);
    rtn=true;
#endif
  } else if (strncasecmp(param, "mqtt_address", 12) == 0) {
    _aqconfig_.mqtt_server = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "mqtt_dz_sub_topic", 17) == 0) {
    _aqconfig_.mqtt_dz_sub_topic = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "mqtt_dz_pub_topic", 17) == 0) {
    _aqconfig_.mqtt_dz_pub_topic = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "mqtt_hassio_discover_topic", 26) == 0) {
    _aqconfig_.mqtt_hass_discover_topic = cleanalloc(value);
    /*  It might also make sence to also set these to true. Since aqualinkd does not know the state at the time discover topics are published.
      _aqconfig_.force_swg;
      _aqconfig_.force_ps_setpoints;
      _aqconfig_.force_frzprotect_setpoints;
      force_chem_feeder
    */
    rtn=true;
  } else if (strncasecmp(param, "mqtt_aq_topic", 13) == 0) {
    _aqconfig_.mqtt_aq_topic = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "mqtt_user", 9) == 0) {
    _aqconfig_.mqtt_user = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "mqtt_passwd", 11) == 0) {
    _aqconfig_.mqtt_passwd = cleanalloc(value);
    rtn=true;
  } else if (strncasecmp(param, "air_temp_dzidx", 14) == 0) {
    _aqconfig_.dzidx_air_temp = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "pool_water_temp_dzidx", 21) == 0) {
    _aqconfig_.dzidx_pool_water_temp = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "spa_water_temp_dzidx", 20) == 0) {
    _aqconfig_.dzidx_spa_water_temp = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "light_programming_mode", 21) == 0) {
    _aqconfig_.light_programming_mode = atof(cleanalloc(value)); // should free this
    rtn=true;
  } else if (strncasecmp(param, "light_programming_initial_on", 28) == 0) {
    _aqconfig_.light_programming_initial_on = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "light_programming_initial_off", 29) == 0) {
    _aqconfig_.light_programming_initial_off = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "light_programming_button_spa", 28) == 0) {
    LOG(AQUA_LOG,LOG_ERR, "Config error, 'light_programming_button_spa' no longer supported\n");
    //_aqconfig_.light_programming_button_spa = strtoul(value, NULL, 10) - 1;
    rtn=true;
  } else if (strncasecmp(param, "light_programming_button", 24) == 0 ||
             strncasecmp(param, "light_programming_button_pool", 29) == 0) {
    LOG(AQUA_LOG,LOG_ERR, "Config error, 'light_programming_button' & 'light_programming_button_pool' are no longer supported\n");
    //_aqconfig_.light_programming_button_pool = strtoul(value, NULL, 10) - 1;
    rtn=true;
  } else if (strncasecmp(param, "SWG_percent_dzidx", 17) == 0) {
    _aqconfig_.dzidx_swg_percent = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "SWG_PPM_dzidx", 13) == 0) {
    _aqconfig_.dzidx_swg_ppm = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "SWG_Status_dzidx", 14) == 0) {
    _aqconfig_.dzidx_swg_status = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "override_freeze_protect", 23) == 0) {
    _aqconfig_.override_freeze_protect = text2bool(value);
    rtn=true;
#ifdef AQ_PDA
  } else if (strncasecmp(param, "pda_mode", 8) == 0) {
    LOG(AQUA_LOG,LOG_WARNING, "Config error, 'pda_mode' is no longer supported, please use rs_panel_type\n");
    //_aqconfig_.pda_mode = text2bool(value);
    //set_pda_mode(_aqconfig_.pda_mode);
    _tmpPanel->rs = !text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "pda_sleep_mode", 8) == 0) {
    _aqconfig_.pda_sleep_mode = text2bool(value);
    rtn=true;
#endif
  } else if (strncasecmp(param, "convert_mqtt_temp_to_c", 22) == 0) {
    _aqconfig_.convert_mqtt_temp = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "convert_dz_temp_to_c", 20) == 0) {
    _aqconfig_.convert_dz_temp = text2bool(value);
    rtn=true;
    /*
  } else if (strncasecmp(param, "flash_mqtt_buttons", 18) == 0) {
    _aqconfig_.flash_mqtt_buttons = text2bool(value);
    rtn=true;*/
  } else if (strncasecmp(param, "report_zero_spa_temp", 20) == 0) {
    _aqconfig_.report_zero_spa_temp = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "report_zero_pool_temp", 21) == 0) {
    _aqconfig_.report_zero_pool_temp = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "read_all_devices", 16) == 0) {
    LOG(AQUA_LOG,LOG_WARNING, "Config error, 'read_all_devices' is no longer supported, please using one or all of 'read_RS485_swg','read_RS485_ePump','read_RS485_vsfPump'\n");
    if (text2bool(value)) {
      _aqconfig_.read_RS485_devmask |= READ_RS485_SWG;
      _aqconfig_.read_RS485_devmask |= READ_RS485_JAN_PUMP;
    } else {
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_SWG;
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_JAN_PUMP;
    }
    rtn=true;
  } else if (strncasecmp (param, "read_pentair_packets", 17) == 0) {
    LOG(AQUA_LOG,LOG_WARNING, "Config error, 'read_all_devices' is no longer supported, please using 'read_pentair_pump'\n");
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_PEN_PUMP;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_PEN_PUMP;
    rtn=true;
  } else if (strncasecmp (param, "read_RS485_swg", 14) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_SWG;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_SWG;
    rtn=true;
  } else if (strncasecmp (param, "read_RS485_ePump", 16) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_JAN_PUMP;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_JAN_PUMP;
    rtn=true;
  } else if (strncasecmp (param, "read_RS485_vsfPump", 16) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_PEN_PUMP;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_PEN_PUMP;
    rtn=true;
  } else if (strncasecmp (param, "read_RS485_JXi", 14) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_JAN_JXI;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_JAN_JXI;
    rtn=true;
   } else if (strncasecmp (param, "read_RS485_LX", 13) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_JAN_LX;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_JAN_LX;
    rtn=true;
  } else if (strncasecmp (param, "read_RS485_Chem", 14) == 0) {
    if (text2bool(value))
      _aqconfig_.read_RS485_devmask |= READ_RS485_JAN_CHEM;
    else
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_JAN_CHEM;
    rtn=true;
  } else if (strncasecmp (param, "use_panel_aux_labels", 20) == 0) {
    _aqconfig_.use_panel_aux_labels = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "force_SWG", 9) == 0) {
    _aqconfig_.force_swg = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "force_ps_setpoints", 18) == 0) {
    _aqconfig_.force_ps_setpoints = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "force_frzprotect_setpoints", 26) == 0) {
    _aqconfig_.force_frzprotect_setpoints = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "force_chem_feeder", 17) == 0) {
    _aqconfig_.force_chem_feeder = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "debug_RSProtocol_bytes", 22) == 0) {
    _aqconfig_.log_raw_bytes = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "debug_RSProtocol_packets", 24) == 0) {
    _aqconfig_.log_protocol_packets = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "swg_zero_ignore_count", 21) == 0) {
    _aqconfig_.swg_zero_ignore = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp (param, "display_warnings_in_web", 23) == 0) {
    _aqconfig_.display_warnings_web = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "mqtt_timed_update", 17) == 0) {
    _aqconfig_.mqtt_timed_update = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "keep_paneltime_synced", 21) == 0) {
    _aqconfig_.sync_panel_time = text2bool(value);
    rtn=true;
#ifdef AQ_NO_THREAD_NETSERVICE
  } else if (strncasecmp (param, "network_poll_speed", 18) == 0) {
    LOG(AQUA_LOG,LOG_WARNING, "Config error, 'network_poll_speed' is no longer supported, using value for 'rs_poll_speed'\n");
    _aqconfig_.rs_poll_speed = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp (param, "rs_poll_speed", 13) == 0) {
    _aqconfig_.rs_poll_speed = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp (param, "thread_netservices", 18) == 0) {
    _aqconfig_.thread_netservices = text2bool(value);
    rtn=true;
#endif
  } else if (strncasecmp (param, "enable_scheduler", 16) == 0) {
    _aqconfig_.enable_scheduler = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "ftdi_low_latency", 16) == 0) {
    _aqconfig_.ftdi_low_latency = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "rs485_frame_delay", 17) == 0) {
    _aqconfig_.frame_delay = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp (param, "device_pre_state", 16) == 0) {
    _aqconfig_.device_pre_state = text2bool(value);
    rtn=true;
  }

  else if (strncasecmp(param, "button_", 7) == 0) {
    // Check we have inichalized panel information, if not use any settings we may have
    if (_aqconfig_.paneltype_mask == 0)
      setPanel(aqdata, _tmpPanel->rs, _tmpPanel->size, _tmpPanel->combo, _tmpPanel->dual);

    int num = strtoul(param + 7, NULL, 10) - 1;
    if (num > TOTAL_BUTTONS) {
      LOG(AQUA_LOG,LOG_ERR, "Config error, button_%d is out of range\n",num+1);
      rtn=false;
    } else if (strncasecmp(param + 9, "_label", 6) == 0) {
      aqdata->aqbuttons[num].label = cleanalloc(value);
      rtn=true;
    } else if (strncasecmp(param + 9, "_dzidx", 6) == 0) {
      aqdata->aqbuttons[num].dz_idx = strtoul(value, NULL, 10);
      rtn=true;
#ifdef AQ_PDA
    } else if (strncasecmp(param + 9, "_PDA_label", 10) == 0) {
      LOG(AQUA_LOG,LOG_WARNING, "Config error, 'button_%d_PDA_label' is no longer supported, please use 'button_%d_label'\n",num,num);
      //aqdata->aqbuttons[num].pda_label = cleanalloc(value);
      aqdata->aqbuttons[num].label = cleanalloc(value);
      rtn=true;
#endif
    } else if (strncasecmp(param + 9, "_lightMode", 10) == 0) {
      if (aqdata->num_lights < MAX_LIGHTS) {
        int type = strtoul(value, NULL, 10);
        if (type < LC_PROGRAMABLE || type > NUMBER_LIGHT_COLOR_TYPES) {
          LOG(AQUA_LOG,LOG_ERR, "Config error, unknown light mode '%s'\n",type);
        } else {
          aqdata->lights[aqdata->num_lights].button = &aqdata->aqbuttons[num];
          aqdata->lights[aqdata->num_lights].lightType = type;
          aqdata->num_lights++;
          aqdata->aqbuttons[num].special_mask |= PROGRAM_LIGHT;
        }
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, (colored|programmable) Lights limited to %d, ignoring %s'\n",MAX_LIGHTS,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpID", 7) == 0) {
      pump_detail *pump = getpump(aqdata, num);
      if (pump != NULL) {
        pump->pumpID = strtoul(cleanalloc(value), NULL, 16);
        if ( (int)pump->pumpID <= PENTAIR_DEC_PUMP_MAX) {
          pump->prclType = PENTAIR;
        } else {
          pump->prclType = JANDY;
          //pump->pumpType = EPUMP; // For testing let the interface set this
        }
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpIndex", 10) == 0) { //button_01_pumpIndex=1
      pump_detail *pump = getpump(aqdata, num);
      if (pump != NULL) {
        pump->pumpIndex = strtoul(value, NULL, 10);
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpType", 9) == 0) {
      // This is not documented, as it's prefered for AqualinkD to find the pump type.
      pump_detail *pump = getpump(aqdata, num);
      if (pump != NULL) {
        if ( stristr(value, "Pentair VS") != 0)
          pump->pumpType = VSPUMP;
        else if ( stristr(value, "Pentair VF") != 0)
          pump->pumpType = VFPUMP;
        else if ( stristr(value, "Jandy ePump") != 0)
          pump->pumpType = EPUMP;
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS,param);
      }
      rtn=true;
    }
      /*
    } else if (strncasecmp(param + 9, "_pumpID", 7) == 0) {
      //aqdata->aqbuttons[num].pda_label = cleanalloc(value);
      //96 to 111 = Pentair, 120 to 123 = Jandy
      if (pi < MAX_PUMPS) {
        aqdata->pumps[pi].button = &aqdata->aqbuttons[num];
        aqdata->pumps[pi].pumpID = strtoul(cleanalloc(value), NULL, 16);
        aqdata->pumps[pi].pumpIndex = pi+1;
        //aqdata->pumps[pi].buttonID = num;
        if (aqdata->pumps[pi].pumpID < 119)
          aqdata->pumps[pi].ptype = PENTAIR;
        else
          aqdata->pumps[pi].ptype = JANDY;
        pi++;
        
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring %s'\n",MAX_PUMPS,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pumpIndex", 10) == 0) { //button_01_pumpIndex=1 
    }*/
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
    aqdata->aqbuttons[button].special_mask |= VS_PUMP;
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


void init_config()
{
  init_parameters(&_aqconfig_);
}

//void readCfg (struct aqconfig *config_parameters, struct aqualinkdata *aqdata, char *cfgFile)
void read_config (struct aqualinkdata *aqdata, char *cfgFile)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  //const char delim[2] = ";";
  //char *buf;
  //int line = 0;
  //int tokenindex = 0;
  char *b_ptr;

 

  _aqconfig_.config_file = cleanalloc(cfgFile);

  if( (fp = fopen(cfgFile, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        char *indx;
        // Eat leading whitespace
        while(isspace(*b_ptr)) b_ptr++;
        if ( b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          indx = strchr(b_ptr, '=');  
          if ( indx != NULL) 
          {
            if ( ! setConfigValue(aqdata, b_ptr, indx+1))
              LOG(AQUA_LOG,LOG_ERR, "Unknown config parameter '%.*s'\n",strlen(b_ptr)-1, b_ptr);
          } 
        }
      }
    }
    fclose(fp);
  } else {
    /* error processing, couldn't open file */
    LOG(AQUA_LOG,LOG_ERR, "Error reading config file '%s'\n",cfgFile);
    errno = EBADF;
    displayLastSystemError("Error reading config file");
    exit (EXIT_FAILURE);
  }

  free(_tmpPanel);
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

/*
bool remount_root_ro(bool readonly) {
  // NSF Check if config is RO_ROOT set
  if (readonly) {} // Dummy to stop compile warnings.

  if (readonly) {
    LOG(AQUA_LOG,LOG_INFO, "reMounting root RO\n");
    mount (NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    return true;
  } else {
    struct statvfs fsinfo;
    statvfs("/", &fsinfo);
    if ((fsinfo.f_flag & ST_RDONLY) == 0) // We are readwrite, ignore
      return false;

    LOG(AQUA_LOG,LOG_INFO, "reMounting root RW\n");
    mount (NULL, "/", NULL, MS_REMOUNT, NULL);
    return true;
  }
  
 return true;
}
*/
void writeCharValue (FILE *fp, char *msg, char *value)
{
  if (value == NULL)
    fprintf(fp, "#%s = \n",msg);
  else
    fprintf(fp, "%s = %s\n", msg, value);
}
void writeIntValue (FILE *fp, char *msg, int value)
{
  if (value <= 0)
    fprintf(fp, "#%s = \n",msg);
  else
    fprintf(fp, "%s = %d\n", msg, value);
}


bool writeCfg (struct aqualinkdata *aqdata)
{ 
  LOG(AQUA_LOG,LOG_ERR, "writeCfg() not implimented\n");
  /*
  FILE *fp;
  int i;
  bool fs = remount_root_ro(false);

  fp = fopen(_aqconfig_.config_file, "w");
  if (fp == NULL) {
    LOG(AQUA_LOG,LOG_ERR, "Open config file failed '%s'\n", _aqconfig_.config_file);
    remount_root_ro(true);
    //fprintf(stdout, "Open file failed 'sprinkler.cron'\n");
    return false;
  }
  fprintf(fp, "#***** AqualinkD configuration *****\n");

  fprintf(fp, "socket_port = %s\n", _aqconfig_.socket_port);
  fprintf(fp, "serial_port = %s\n", _aqconfig_.serial_port);
  fprintf(fp, "device_id = 0x%02hhx\n", _aqconfig_.device_id);
  fprintf(fp, "read_all_devices = %s", bool2text(_aqconfig_.read_all_devices));
  writeCharValue(fp, "log_level", errorlevel2text(_aqconfig_.log_level));
  writeCharValue(fp, "web_directory", _aqconfig_.web_directory);
  writeCharValue(fp, "log_file", _aqconfig_.log_file);
  fprintf(fp, "pda_mode = %s\n", bool2text(_aqconfig_.pda_mode)); 

  fprintf(fp, "\n#** MQTT Configuration **\n");
  writeCharValue(fp, "mqtt_address", _aqconfig_.mqtt_server);
  writeCharValue(fp, "mqtt_dz_sub_topic", _aqconfig_.mqtt_dz_sub_topic);
  writeCharValue(fp, "mqtt_dz_pub_topic", _aqconfig_.mqtt_dz_pub_topic);
  writeCharValue(fp, "mqtt_aq_topic", _aqconfig_.mqtt_aq_topic);
  writeCharValue(fp, "mqtt_user", _aqconfig_.mqtt_user);
  writeCharValue(fp, "mqtt_passwd", _aqconfig_.mqtt_passwd);

  fprintf(fp, "\n#** General **\n");
  fprintf(fp, "convert_mqtt_temp_to_c = %s\n", bool2text(_aqconfig_.convert_mqtt_temp));
  fprintf(fp, "override_freeze_protect = %s\n", bool2text(_aqconfig_.override_freeze_protect));        
  //fprintf(fp, "flash_mqtt_buttons = %s\n", bool2text(_aqconfig_.flash_mqtt_buttons)); 
  fprintf(fp, "report_zero_spa_temp = %s\n", bool2text(_aqconfig_.report_zero_spa_temp));
  fprintf(fp, "report_zero_pool_temp = %s\n", bool2text(_aqconfig_.report_zero_pool_temp));

  fprintf(fp, "\n#** Programmable light **\n");
  //if (_aqconfig_.light_programming_button_pool <= 0) {
  //  fprintf(fp, "#light_programming_button_pool = %d\n", _aqconfig_.light_programming_button_pool); 
  //  fprintf(fp, "#light_programming_mode = %f\n", _aqconfig_.light_programming_mode);  
   // fprintf(fp, "#light_programming_initial_on = %d\n", _aqconfig_.light_programming_initial_on);         
  //  fprintf(fp, "#light_programming_initial_off = %d\n", _aqconfig_.light_programming_initial_off);
  //} else {
    fprintf(fp, "light_programming_button_pool = %d\n", _aqconfig_.light_programming_button_pool); 
    fprintf(fp, "light_programming_button_spa = %d\n", _aqconfig_.light_programming_button_spa); 
    fprintf(fp, "light_programming_mode = %f\n", _aqconfig_.light_programming_mode);  
    fprintf(fp, "light_programming_initial_on = %d\n", _aqconfig_.light_programming_initial_on);         
    fprintf(fp, "light_programming_initial_off = %d\n", _aqconfig_.light_programming_initial_off);
  //}

  fprintf(fp, "\n#** Domoticz **\n");
  fprintf(fp, "convert_dz_temp_to_c = %s\n", bool2text(_aqconfig_.convert_dz_temp));
  writeIntValue(fp, "air_temp_dzidx", _aqconfig_.dzidx_air_temp); 
  writeIntValue(fp, "pool_water_temp_dzidx", _aqconfig_.dzidx_pool_water_temp);     
  writeIntValue(fp, "spa_water_temp_dzidx", _aqconfig_.dzidx_spa_water_temp); 
  writeIntValue(fp, "SWG_percent_dzidx", _aqconfig_.dzidx_swg_percent); 
  writeIntValue(fp, "SWG_PPM_dzidx", _aqconfig_.dzidx_swg_ppm); 
  writeIntValue(fp, "SWG_Status_dzidx", _aqconfig_.dzidx_swg_status);    

  fprintf(fp, "\n#** Buttons **\n");
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    fprintf(fp, "button_%.2d_label = %s\n", i+1, aqdata->aqbuttons[i].label);
    if (aqdata->aqbuttons[i].dz_idx > 0)
      fprintf(fp, "button_%.2d_dzidx = %d\n", i+1, aqdata->aqbuttons[i].dz_idx);
    if (aqdata->aqbuttons[i].pda_label != NULL)
      fprintf(fp, "button_%.2d_PDA_label = %s\n", i+1, aqdata->aqbuttons[i].pda_label);
  }
  fclose(fp);
  remount_root_ro(fs);
  */
  return true;
}

