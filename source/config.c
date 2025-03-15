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
#include <regex.h>

#define CONFIG_C
#include "config.h"
#include "utils.h"
#include "aq_serial.h"
#include "aq_panel.h"
#include "aqualink.h"
#include "iaqualink.h"
#include "color_lights.h"
#include "aq_scheduler.h"
#include "aq_panel.h"
#include "rs_msg_utils.h"
#include "aq_filesystem.h"

#define MAXCFGLINE 256



char *generate_mqtt_id(char *buf, int len);
pump_detail *getpump(struct aqualinkdata *aqdata, int button);
bool populatePumpData(struct aqualinkdata *aqdata, char *pumpcfg ,aqkey *button, char *value);
pump_detail *getPumpFromButtonID(struct aqualinkdata *aqdata, aqkey *button);
aqkey *getVirtualButton(struct aqualinkdata *aqdata, int num);
struct aqualinkdata *_aqdata = NULL;



struct tmpPanelInfo {
  int size;
  bool rs;
  bool combo;
  bool dual;
};
struct tmpPanelInfo _defaultPanel;

/*
* initialize data to default values
*/

/* odd ball

   
   ftdi_low_latency
   rs485_frame_delay

   display_warnings_in_web
   sync_panel_time
   overide_freeze_protect
*/

void set_cfg_parm_to_default(cfgParam *parm) {

  //printf("Setting default %s\n",parm->name);
      switch(parm->value_type) {
        case CFG_STRING:
          *(char **)parm->value_ptr = (char *)parm->default_value;
        break;
        case CFG_INT:
          *(int *)parm->value_ptr =  *(int *) parm->default_value;
        break;
        case CFG_BOOL:
          *(bool *)parm->value_ptr = *(bool *) parm->default_value;
        break;
        case CFG_HEX:
          *(unsigned char *)parm->value_ptr = *(unsigned char *)parm->default_value; 
        break;
        case CFG_FLOAT:
          *(float *)parm->value_ptr = *(float *)parm->default_value;
        break;
        case CFG_BITMASK:
          if ( *(bool *)parm->default_value == true) {
            *(uint8_t *)parm->value_ptr |= parm->mask;
          } else {
            *(uint8_t *)parm->value_ptr &= ~parm->mask;
          }
        break;
        case CFG_SPECIAL:
          if (strncasecmp(parm->name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
            *(int *)parm->value_ptr = *(int *)parm->default_value;
          } else if (strncasecmp(parm->name, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
            //setPanelByName(aqdata, cleanwhitespace(value)); 
          } else {
            LOG(AQUA_LOG,LOG_ERR, "ADD CONFIG DEFAULT FOR %s\n",parm->name);
          }
        break;
      }

}

void set_config_defaults()
{
  for (int i=0; i <= _numCfgParams; i++) {
    
    set_cfg_parm_to_default(&_cfgParams[i]);
  }

}

/*
#define cfgint "const int"
#define cfgbool "const bool"
#define cfgchar "const char *"
#define cfghex  "const unsigned char"
*/

const int           _dcfg_unknownInt = TEMP_UNKNOWN;
const unsigned char _dcfg_unknownHex = NUL; // 0x00 don't use
const unsigned char _dcfg_findIDHex = 0xFF;

const bool          _dcfg_false = false;
const bool          _dcfg_true = true;
const int           _dcfg_loglevel = DEFAULT_LOG_LEVEL;

const char         *_dcfg_web_port = DEFAULT_WEBPORT;
//const char         *_dcfg_web_port = "80";
const char         *_dcfg_web_root = DEFAULT_WEBROOT;
const char         *_dcfg_serial_port = DEFAULT_SERIALPORT;

const char         *_dcfg_mqtt_ha_discover = DEFAULT_HASS_DISCOVER;
const char         *_dcfg_mqtt_aq_tp = DEFAULT_MQTT_AQ_TP;

const char         *_dcfg_null = NULL;

const int           _dcfg_zero = 0;
const int           _dcfg_light_programming_mode = 0;
const int           _dcfg_light_programming_initial_on = 15;
const int           _dcfg_light_programming_initial_off = 12;


void init_parameters (struct aqconfig * parms)
{
  //#ifdef CONFIG_DEV_TEST
  _numCfgParams = 0;

  const int unknownInt = TEMP_UNKNOWN;
 
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.socket_port;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_socket_port;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_web_port;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.serial_port;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_serial_port;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_serial_port;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.log_level;
  _cfgParams[_numCfgParams].value_type = CFG_SPECIAL; // Set with _aqconfig_.log_level = text2elevel(cleanalloc(value));
  _cfgParams[_numCfgParams].name = CFG_N_log_level;
  _cfgParams[_numCfgParams].valid_values = CFG_V_log_level;
  _cfgParams[_numCfgParams].default_value = (void *) &_dcfg_loglevel;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.paneltype_mask;
  _cfgParams[_numCfgParams].value_type = CFG_SPECIAL;
  _cfgParams[_numCfgParams].name = CFG_N_panel_type;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.device_id;
  _cfgParams[_numCfgParams].value_type = CFG_HEX;
  _cfgParams[_numCfgParams].name = CFG_N_device_id;
  _cfgParams[_numCfgParams].valid_values = CFG_V_device_id;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_findIDHex;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.rssa_device_id;
  _cfgParams[_numCfgParams].value_type = CFG_HEX;
  _cfgParams[_numCfgParams].name = CFG_N_rssa_device_id;
  _cfgParams[_numCfgParams].valid_values = CFG_V_rssa_device_id;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_unknownHex;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.extended_device_id_programming;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL; 
  _cfgParams[_numCfgParams].name = CFG_N_extended_device_id_programming;
  _cfgParams[_numCfgParams].valid_values = CFG_V_BOOL;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.extended_device_id;
  _cfgParams[_numCfgParams].value_type = CFG_HEX; 
  _cfgParams[_numCfgParams].name = CFG_N_extended_device_id;
  _cfgParams[_numCfgParams].valid_values = CFG_V_extended_device_id;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_unknownHex;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.enable_iaqualink;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL; 
  _cfgParams[_numCfgParams].name = CFG_N_enable_iaqualink;
  _cfgParams[_numCfgParams].valid_values = CFG_V_BOOL;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.web_directory;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_web_directory;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = NULL;
  
#ifndef AQ_MANAGER
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.log_file;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_log_file;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = NULL;
#endif
  
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_server;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_server;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_null;

   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_user;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_user;
  _cfgParams[_numCfgParams].default_value = (void *)NULL;

   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_passwd;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_passwd;
  _cfgParams[_numCfgParams].default_value = (void *)NULL;

   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_aq_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_aq_topic;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_mqtt_aq_tp;

   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_hass_discover_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_hass_discover_topic;
  _cfgParams[_numCfgParams].default_value = (void *)_dcfg_mqtt_ha_discover;


   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_hass_discover_use_mac;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_hass_discover_use_mac;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_timed_update;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_timed_update;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.convert_mqtt_temp;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_convert_mqtt_temp;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_dz_sub_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_dz_sub_topic;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = NULL;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_dz_pub_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_dz_pub_topic;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = NULL;
/*
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.mqtt_dz_pub_topic;
  _cfgParams[_numCfgParams].value_type = CFG_STRING;
  _cfgParams[_numCfgParams].name = CFG_N_mqtt_dz_pub_topic;
  _cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].default_value = NULL;
*/
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_air_temp;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_air_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_pool_water_temp;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_pool_water_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_spa_water_temp;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_spa_water_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_swg_percent;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_swg_percent;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_swg_ppm;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_swg_ppm;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.dzidx_swg_status;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_dzidx_swg_status;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.convert_dz_temp;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_convert_dz_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&unknownInt;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.light_programming_mode;
  _cfgParams[_numCfgParams].value_type = CFG_FLOAT;
  _cfgParams[_numCfgParams].name = CFG_N_light_programming_mode;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_light_programming_mode;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.light_programming_initial_on;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_light_programming_initial_on;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_light_programming_initial_on;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.light_programming_initial_off;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_light_programming_initial_off;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_light_programming_initial_off;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_swg;
  _cfgParams[_numCfgParams].mask = READ_RS485_SWG;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_ePump;
  _cfgParams[_numCfgParams].mask = READ_RS485_JAN_PUMP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

   _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_vsfPump;
  _cfgParams[_numCfgParams].mask = READ_RS485_PEN_PUMP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_JXi;
  _cfgParams[_numCfgParams].mask = READ_RS485_JAN_JXI;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_LX;
  _cfgParams[_numCfgParams].mask = READ_RS485_JAN_LX;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_Chem;
  _cfgParams[_numCfgParams].mask = READ_RS485_JAN_CHEM;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_iAqualink;
  _cfgParams[_numCfgParams].mask = READ_RS485_IAQUALNK;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.read_RS485_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_read_RS485_HeatPump;
  _cfgParams[_numCfgParams].mask = READ_RS485_HEATPUMP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  // ADD FORCE OPTIONS HERE
  
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.force_device_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_force_swg;
  _cfgParams[_numCfgParams].mask = FORCE_SWG_SP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.force_device_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_force_ps_setpoints;
  _cfgParams[_numCfgParams].mask = FORCE_POOLSPA_SP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.force_device_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_force_frzprotect_setpoints;
  _cfgParams[_numCfgParams].mask = FORCE_FREEZEPROTECT_SP;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.force_device_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_force_chem_feeder;
  _cfgParams[_numCfgParams].mask = FORCE_CHEM_FEEDER;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

    //When we add chiller support
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.force_device_devmask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_force_chiller;
  _cfgParams[_numCfgParams].mask = FORCE_CHILLER;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.enable_scheduler;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_enable_scheduler;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.schedule_event_mask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_usecron;
  _cfgParams[_numCfgParams].mask = AQS_USE_CRON_PUMP_TIME;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.schedule_event_mask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_poweron;
  _cfgParams[_numCfgParams].mask = AQS_POWER_ON;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.schedule_event_mask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_freezeprotectoff;
  _cfgParams[_numCfgParams].mask = AQS_FRZ_PROTECT_OFF;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.schedule_event_mask;
  _cfgParams[_numCfgParams].value_type = CFG_BITMASK;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_boostoff;
  _cfgParams[_numCfgParams].mask = AQS_BOOST_OFF;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.sched_chk_pumpon_hour;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_pumpon_hour;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_zero;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.sched_chk_pumpoff_hour;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_event_check_pumpoff_hour;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_zero;
  
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.ftdi_low_latency;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_ftdi_low_latency;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.frame_delay;
  _cfgParams[_numCfgParams].value_type = CFG_INT;
  _cfgParams[_numCfgParams].name = CFG_N_rs485_frame_delay;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_zero;


  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.sync_panel_time;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_sync_panel_time;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.display_warnings_web;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = "display_warnings_in_web";
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.override_freeze_protect;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_override_freeze_protect;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_false;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.report_zero_spa_temp;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_report_zero_spa_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.report_zero_pool_temp;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_report_zero_pool_temp;
  //_cfgParams[_numCfgParams].advanced = true;
  _cfgParams[_numCfgParams].config_mask |= CFG_GRP_ADVANCED;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;

#ifdef AQ_PDA
  _numCfgParams++;
  _cfgParams[_numCfgParams].value_ptr = &_aqconfig_.pda_sleep_mode;
  _cfgParams[_numCfgParams].value_type = CFG_BOOL;
  _cfgParams[_numCfgParams].name = CFG_N_pda_sleep_mode;
  _cfgParams[_numCfgParams].default_value = (void *)&_dcfg_true;
#endif

  //#endif

  // Default to daemonize
  parms->deamonize = true;

  // clear the panel type
  parms->paneltype_mask = 0;
  
  // Create default panel if it get's missed from config
  _defaultPanel.size = 6;
  _defaultPanel.rs = true;
  _defaultPanel.combo = true;
  _defaultPanel.dual = false;

  // Few other defaults we don;t set in general config
  parms->log_protocol_packets = false; // Read & Write as packets write to file
  parms->log_raw_bytes = false; // bytes read and write to file

#ifdef AQ_NO_THREAD_NETSERVICE
  parms->rs_poll_speed = DEFAULT_POLL_SPEED;
  parms->thread_netservices = true;
#endif

  parms->device_pre_state = true;

  clearDebugLogMask();
  
  for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
    parms->RSSD_LOG_filter[i] = NUL;
  }

  generate_mqtt_id(parms->mqtt_ID, MQTT_ID_LEN);

  set_config_defaults();

  //set_config_defaults();

  //int i;
  //char *p;
  //parms->rs_panel_size = 8;
  /*
  parms->serial_port = DEFAULT_SERIALPORT;
  parms->log_level = DEFAULT_LOG_LEVEL;
  parms->socket_port = DEFAULT_WEBPORT;
  parms->web_directory = DEFAULT_WEBROOT;
  //parms->device_id = strtoul(DEFAULT_DEVICE_ID, &p, 16);
  parms->device_id = strtoul(DEFAULT_DEVICE_ID, NULL, 16);
  parms->rssa_device_id = NUL;
  */

  /*

  parms->extended_device_id = NUL;
  parms->extended_device_id_programming = false;

*/
/*
  //sscanf(DEFAULT_DEVICE_ID, "0x%x", &parms->device_id);
  parms->override_freeze_protect = FALSE;

  parms->mqtt_dz_sub_topic = DEFAULT_MQTT_DZ_OUT;
  parms->mqtt_dz_pub_topic = DEFAULT_MQTT_DZ_IN;
  parms->mqtt_hass_discover_topic = DEFAULT_HASS_DISCOVER;
  parms->mqtt_aq_topic = DEFAULT_MQTT_AQ_TP;
  parms->mqtt_server = DEFAULT_MQTT_SERVER;
  parms->mqtt_user = DEFAULT_MQTT_USER;
  parms->mqtt_passwd = DEFAULT_MQTT_PASSWD;
  parms->mqtt_hass_discover_use_mac = false;

  parms->dzidx_air_temp = TEMP_UNKNOWN;
  parms->dzidx_pool_water_temp = TEMP_UNKNOWN;
  parms->dzidx_spa_water_temp = TEMP_UNKNOWN;
  parms->dzidx_swg_percent = TEMP_UNKNOWN;
  parms->dzidx_swg_ppm = TEMP_UNKNOWN;
  parms->dzidx_swg_status = TEMP_UNKNOWN;
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
  parms->report_zero_pool_temp = true;
  parms->report_zero_spa_temp = true;
  //parms->read_all_devices = true;
  //parms->read_pentair_packets = false;
  parms->read_RS485_devmask = 0;
  parms->use_panel_aux_labels = false;
  
  //parms->force_swg = false;
  //parms->force_ps_setpoints = false;
  //parms->force_frzprotect_setpoints = false;
  //parms->force_chem_feeder = false;
  
  //parms->swg_pool_and_spa = false;
  //parms->swg_zero_ignore = DEFAULT_SWG_ZERO_IGNORE_COUNT;
  parms->display_warnings_web = false;

  parms->log_protocol_packets = false; // Read & Write as packets write to file
  parms->log_raw_bytes = false; // bytes read and write to file
  
  parms->sync_panel_time = true;

#ifdef AQ_NO_THREAD_NETSERVICE
  parms->rs_poll_speed = DEFAULT_POLL_SPEED;
  parms->thread_netservices = true;
#endif

  parms->enable_scheduler = true;

  parms->schedule_event_mask = 0;
  //parms->sched_chk_poweron = false;
  //parms->sched_chk_freezeprotectoff = false;
  //parms->sched_chk_boostoff = false;
  parms->sched_chk_pumpon_hour = 0;
  parms->sched_chk_pumpoff_hour = 0;

  parms->ftdi_low_latency = true;
  parms->frame_delay = 0;
  parms->device_pre_state = true;
  */
}


/*

strlcpy is pulled from here.
https://github.com/freebsd/freebsd-src/blob/master/sys/libkern/strlcpy.c
*/
/*
 * Copy string src to buffer dst of size dsize.  At most dsize-1
 * chars will be copied.  Always NUL terminates (unless dsize == 0).
 * Returns strlen(src); if retval >= dsize, truncation occurred.
 */
size_t strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';		/* NUL-terminate dst */
		while (*src++)
			;
	}

	return(src - osrc - 1);	/* count does not include NUL */
}

char *cleanalloc(char*str)
{
  return ncleanalloc(str, 0);
  /*
  if (str == NULL)
    return str;

  char *result;
  str = cleanwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
  */
}

char *ncleanalloc(char*str, int length)
{
  if (str == NULL)
    return str;

  char *result;
  str = cleanwhitespace(str);
  
  
  if (length > 0 ) {
    result = (char*)malloc(length);  
    strlcpy ( result, str, length );
  } else {
    result = (char*)malloc(strlen(str)+1);  
    strcpy ( result, str );
  }
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
bool mac(char *buf, int len, bool useDelimiter)
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
        if ( s.ifr_addr.sa_data[0] == 0 &&
             s.ifr_addr.sa_data[1] == 0 &&
             s.ifr_addr.sa_data[2] == 0 &&
             s.ifr_addr.sa_data[3] == 0 &&
             s.ifr_addr.sa_data[4] == 0 &&
             s.ifr_addr.sa_data[5] == 0 ) {
          continue;
        }

        if ((ioctl(fd, SIOCGIFFLAGS, &s) < 0) || !(s.ifr_flags & IFF_RUNNING))
        {
          continue;
        } 

        int i;
        int step=2;
        if (useDelimiter) {step=3;}
        
        for (i = 0; i < 6 && i * step < len; ++i)
        {
          sprintf(&buf[i * step], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
          if (useDelimiter && i<5) {
            buf[i * step + 2] = ':';
          }
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

  if ( i > 9) { i=9; } // cut down to 9 characters (aqualinkd)

  if (i < len) {
    buf[i++] = '_';
    // If we can't get MAC to pad mqtt id then use PID
    if (!mac(&buf[i], len - i, false)) {
      sprintf(&buf[i], "%.*d", (len-i), getpid());
    }
  }

  buf[len] = '\0';

  return buf;
}


bool setConfigValue(struct aqualinkdata *aqdata, char *param, char *value) {
  bool rtn = false;
  char *tmpval;
 

//#ifdef CONFIG_DEV_TEST
  //int val;
  //char *sval;
  for (int i=0; i <= _numCfgParams; i++) {
    //printf ("** Test against '%s' %d\n",_cfgParams[i].name, (int)strlen(_cfgParams[i].name));
    if (strncasecmp(param, _cfgParams[i].name, (int)strlen(_cfgParams[i].name) ) == 0) {
      //printf ("MATCH = %s\n",param);
      rtn=true;

      // Any special 
      if ( _cfgParams[i].valid_values != NULL ) {
        //printf("Checking %s in %s\n",value,_cfgParams[i].valid_values);
        if ( rsm_strstr(_cfgParams[i].valid_values, cleanwhitespace(value)) == NULL) {
          LOG(AQUA_LOG,LOG_ERR, "Config entry '%s',  %s is not valid\n",param, value);
          //rtn=false;
          return false;
        }
      }

      if (strlen(cleanwhitespace(value)) <= 0) {
        LOG(AQUA_LOG,LOG_NOTICE,"Set configuration option `%s` to default since value is blank\n",_cfgParams[i].name );
        set_cfg_parm_to_default(&_cfgParams[i]);
        return true;
      }

      switch (_cfgParams[i].value_type) {
        case CFG_STRING:
            if (_cfgParams[i].value_ptr != NULL && *(char **)_cfgParams[i].value_ptr != _cfgParams[i].default_value) {
              LOG(AQUA_LOG,LOG_DEBUG,"FREE Memory for config %s %s\n",_cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
              free(*(char **)_cfgParams[i].value_ptr);
              *(char **)_cfgParams[i].value_ptr = NULL;
           }
           *(char **)_cfgParams[i].value_ptr = cleanalloc(value);
        break;
        case CFG_INT:
          *(int *)_cfgParams[i].value_ptr = strtoul(cleanwhitespace(value), NULL, 10);
        break;
        case CFG_BOOL:
          *(bool *)_cfgParams[i].value_ptr = text2bool(value);
        break;
        case CFG_HEX:
          *(unsigned char *)_cfgParams[i].value_ptr = strtoul(cleanwhitespace(value), NULL, 16); 
        break;
        case CFG_FLOAT:
          tmpval = cleanalloc(value);
          *(float *)_cfgParams[i].value_ptr = atof(tmpval);
          free(tmpval);
        break;
        case CFG_BITMASK:
          if (text2bool(value))
            *(uint8_t *)_cfgParams[i].value_ptr |= _cfgParams[i].mask;
          else
            *(uint8_t *)_cfgParams[i].value_ptr &= ~_cfgParams[i].mask;
        break;
        case CFG_SPECIAL:
          if (strncasecmp(param, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
            *(int *)_cfgParams[i].value_ptr = text2elevel(cleanwhitespace(value));
          } else if (strncasecmp(param, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
            setPanelByName(aqdata, cleanwhitespace(value)); 
          } else {
            LOG(AQUA_LOG,LOG_ERR, "ADD SPECIAL CONFIG FOR '%s'\n",param);
          }
        break;
      }

      return rtn;
    }
  }
  //_cfgParams[_numCfgParams].value_ptr = _aqconfig_.testChar;
  //_cfgParams[_numCfgParams].value_type = CFG_STRING;
  //_cfgParams[_numCfgParams].name = "testChar";
  
  //LOG(AQUA_LOG,LOG_ERR, "Missing cfg for %s\n",param);

//#endif

if (strlen(cleanwhitespace(value)) <= 0) {
  LOG(AQUA_LOG,LOG_WARNING,"Configuration value is blank for option `%s`, Ignoring\n",param );
  return true;
}

   // Values we don't want in general config.
  if (strncasecmp(param, "debug_log_mask", 14) == 0) {
    addDebugLogMask(strtoul(value, NULL, 10));
    rtn=true;
  } else if (strncasecmp(param, CFG_N_RSSD_LOG_filter, CFG_C_RSSD_LOG_filter) == 0) {
    for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
      if (_aqconfig_.RSSD_LOG_filter[i] == NUL) {
        _aqconfig_.RSSD_LOG_filter[i] = strtoul(cleanalloc(value), NULL, 16);
        break;
      }
    }
    rtn=true; 
  } else if (strncasecmp (param, "debug_RSProtocol_bytes", 22) == 0) {
    _aqconfig_.log_raw_bytes = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "debug_RSProtocol_packets", 24) == 0) {
    _aqconfig_.log_protocol_packets = text2bool(value);
    rtn=true;
    
  // Build panel without string
  } else if (strncasecmp(param, "panel_type_size", 15) == 0) {
    _defaultPanel.size = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_combo", 16) == 0) {
    _defaultPanel.combo = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_dual", 15) == 0) {
    _defaultPanel.dual = text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_pda", 14) == 0) {
    _defaultPanel.rs = !text2bool(value);
    rtn=true;
  } else if (strncasecmp(param, "panel_type_rs", 13) == 0) {
    _defaultPanel.rs = text2bool(value);
    rtn=true;
    
/*
  } else if (strncasecmp(param, CFG_N_panel_type, CFG_C_panel_type) == 0) { // This must be last so it doesn't get picked up by other settings
    setPanelByName(aqdata, cleanwhitespace(value));
    rtn=true;
*/

  // Old names that we will map.
   } else if ((strncasecmp(param, CFG_N_mqtt_hass_discover_topic, strlen(CFG_N_mqtt_hass_discover_topic)) == 0) ||
             (strncasecmp(param, "mqtt_hassio_discover_topic", 26) == 0) || 
             (strncasecmp(param, "mqtt_hass_discover_topic", 24) == 0)) {
    _aqconfig_.mqtt_hass_discover_topic = cleanalloc(value);
    rtn=true;
  } else if ((strncasecmp(param, CFG_N_mqtt_hass_discover_use_mac, strlen(CFG_N_mqtt_hass_discover_use_mac)) == 0) ||
             (strncasecmp(param, "mqtt_hassio_discover_use_mac", 28) == 0) ||
             (strncasecmp(param, "mqtt_hass_discover_use_mac", 26) == 0)) {
    _aqconfig_.mqtt_hass_discover_use_mac = text2bool(value);
    rtn=true;
  } else if  (strncasecmp (param, "keep_paneltime_synced", 21) == 0) {
    _aqconfig_.sync_panel_time = text2bool(value);
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
  } else if (strncasecmp(param, "SWG_percent_dzidx", 17) == 0) {
    _aqconfig_.dzidx_swg_percent = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param, "SWG_PPM_dzidx", 13) == 0) {
    _aqconfig_.dzidx_swg_ppm = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp(param,"SWG_Status_dzidx", 16) == 0) {
    _aqconfig_.dzidx_swg_status = strtoul(value, NULL, 10);
    rtn=true;   
  } else if (strncasecmp(param, "convert_mqtt_temp_to_c", 22) == 0) {
    _aqconfig_.convert_mqtt_temp = text2bool(value);
    rtn=true;
   } else if (strncasecmp(param, "convert_dz_temp_to_c", 20) == 0) {
    _aqconfig_.convert_dz_temp = text2bool(value);
    rtn=true;
  } else if (strncasecmp (param, "scheduler_check_poweron", 23) == 0) {
    if (text2bool(value)) {
      _aqconfig_.schedule_event_mask |= AQS_POWER_ON;
    }
    rtn=true;
  } else if (strncasecmp (param, "scheduler_check_freezeprotectoff", 32) == 0) {
    if (text2bool(value)) {
      _aqconfig_.schedule_event_mask |= AQS_FRZ_PROTECT_OFF;
    }
    rtn=true;
  } else if (strncasecmp (param, "scheduler_check_boostoff", 24) == 0) {
    if (text2bool(value)) {
      _aqconfig_.schedule_event_mask |= AQS_BOOST_OFF;
    }
    rtn=true;
  } else if (strncasecmp (param, "scheduler_check_pumpon_hour", 27) == 0) {
    _aqconfig_.sched_chk_pumpon_hour = strtoul(value, NULL, 10);
    rtn=true;
  } else if (strncasecmp (param, "scheduler_check_pumpoff_hour", 28) == 0) {
    _aqconfig_.sched_chk_pumpoff_hour = strtoul(value, NULL, 10);
    rtn=true;

  } else if (strncasecmp(param, "light_program_", 14) == 0) {
    int num = strtoul(param + 14, NULL, 10);
    if ( num >= LIGHT_COLOR_OPTIONS ) {
      LOG(AQUA_LOG,LOG_ERR, "Config error, light_program_%d is out of range\n",num);
    }
    char *name = cleanalloc(value);
    int len = strlen(name);
    if (len > 0) {
      if ( strncmp(name+len-7, " - Show", 7) == 0 ) {
        name[len-7] = '\0';
      //printf("Value '%s' index %d is show\n",name,num);
        set_aqualinkd_light_mode_name(name,num,true);
      } else {
        set_aqualinkd_light_mode_name(name,num,false);
      }
      rtn=true;
    } else {
      LOG(AQUA_LOG,LOG_WARNING, "Config error, light_program_%d is blank\n",num);
      rtn=false;
    }
  } else if (strncasecmp(param, "button_", 7) == 0) {
    // Check we have inichalized panel information, if not use any settings we may have
    if (_aqconfig_.paneltype_mask == 0)
      setPanel(aqdata, _defaultPanel.rs, _defaultPanel.size, _defaultPanel.combo, _defaultPanel.dual);

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
      int type = strtoul(value, NULL, 10);
      // See if light is existing button / light
      if (isPLIGHT(aqdata->aqbuttons[num].special_mask)) {
        ((clight_detail *)aqdata->aqbuttons[num].special_mask_ptr)->lightType = type;
      }
      else if (aqdata->num_lights < MAX_LIGHTS) 
      {
        if (type < LC_PROGRAMABLE || type > NUMBER_LIGHT_COLOR_TYPES) {
          LOG(AQUA_LOG,LOG_ERR, "Config error, unknown light mode '%s'\n",type);
        } else {
          aqdata->lights[aqdata->num_lights].button = &aqdata->aqbuttons[num];
          aqdata->lights[aqdata->num_lights].lightType = type;          
          aqdata->aqbuttons[num].special_mask |= PROGRAM_LIGHT;
          aqdata->aqbuttons[num].special_mask_ptr = &aqdata->lights[aqdata->num_lights];
          if ( aqdata->lights[aqdata->num_lights].lightType == LC_DIMMER2 && _aqconfig_.rssa_device_id != 0x48 ) {
            LOG(AQUA_LOG,LOG_ERR, "Config error, button '%s' has light mode '%d' set. This only supported when 'rssa_device_id' is enabled, changing to light mode '%d'\n",
                aqdata->aqbuttons[num].label, LC_DIMMER2,LC_DIMMER);
            aqdata->lights[aqdata->num_lights].lightType = LC_DIMMER;
          }
          aqdata->num_lights++;
        }
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, (colored|programmable) Lights limited to %d, ignoring %s'\n",MAX_LIGHTS,param);
      }
      rtn=true;
    } else if (strncasecmp(param + 9, "_pump", 5) == 0) {

      if ( ! populatePumpData(aqdata, param + 10, &aqdata->aqbuttons[num], value) ) 
      {
        LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring : %s",MAX_PUMPS,param);
      }

      rtn=true;
    } 
//#if defined AQ_IAQTOUCH
  } else if (strncasecmp(param, "virtual_button_", 15) == 0) {
    rtn=true;
    int num = strtoul(param + 15, NULL, 10);
    if (_aqconfig_.paneltype_mask == 0) {
      // ERROR the vbutton will be irnored.
      LOG(AQUA_LOG,LOG_WARNING, "Config error, Panel type must be definied before adding a virtual_button, ignored setting : %s",param);
    //} else if (_aqconfig_.extended_device_id < 0x30 || _aqconfig_.extended_device_id > 0x33 ) {
    //  LOG(AQUA_LOG,LOG_WARNING, "Config error, extended_device_id must on of the folowing (0x30,0x31,0x32,0x33), ignored setting : %s",param);
    } else if (strncasecmp(param + 17, "_label", 6) == 0) {
      char *label = cleanalloc(value);
      aqkey *button = getVirtualButton(aqdata, num);
      if (button != NULL) {
        setVirtualButtonLabel(button, label);
      } else {
        LOG(AQUA_LOG,LOG_WARNING, "Error with '%s', total buttons=%d, config has %d already, ignoring!\n",param, TOTAL_BUTTONS, aqdata->total_buttons+1);
      }
    } else if (strncasecmp(param + 17, "_altLabel", 9) == 0) {
      char *label = cleanalloc(value);
      aqkey *button = getVirtualButton(aqdata, num);
      if (button != NULL) {
        setVirtualButtonAltLabel(button, label);
      } else {
        LOG(AQUA_LOG,LOG_WARNING, "Error with '%s', total buttons=%d, config has %d already, ignoring!\n",param, TOTAL_BUTTONS, aqdata->total_buttons+1);
      }
    } else if (strncasecmp(param + 17, "_pump", 5) == 0) {
      aqkey *vbutton = getVirtualButton(aqdata, num);
      if (vbutton != NULL) {
        vbutton->led->state = ON; //Virtual pump default to on 
        if ( ! populatePumpData(aqdata, param + 18, vbutton, value) ) 
        {
          LOG(AQUA_LOG,LOG_ERR, "Config error, VSP Pumps limited to %d, ignoring : %s",MAX_PUMPS,param);
        }
      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, could not find vitrual button for `%s`",param);
      }
    } else if (strncasecmp(param + 17, "_onetouchID", 11) == 0) {
      aqkey *vbutton = getVirtualButton(aqdata, num);
      if (vbutton != NULL) {
        switch (strtoul(value, NULL, 10)) {
          case 1:
            vbutton->code = IAQ_ONETOUCH_1;
            vbutton->rssd_code = IAQ_ONETOUCH_1;
          break;
          case 2:
            vbutton->code = IAQ_ONETOUCH_2;
            vbutton->rssd_code = IAQ_ONETOUCH_2;
          break;
          case 3:
            vbutton->code = IAQ_ONETOUCH_3;
            vbutton->rssd_code = IAQ_ONETOUCH_3;
          break;
          case 4:
            vbutton->code = IAQ_ONETOUCH_4;
            vbutton->rssd_code = IAQ_ONETOUCH_4;
          break;
          case 5:
            vbutton->code = IAQ_ONETOUCH_5;
            vbutton->rssd_code = IAQ_ONETOUCH_5;
          break;
          case 6:
            vbutton->code = IAQ_ONETOUCH_6;
            vbutton->rssd_code = IAQ_ONETOUCH_6;
          break;
          default:
            vbutton->code = NUL;
            vbutton->rssd_code = NUL;
          break;
        }

      } else {
        LOG(AQUA_LOG,LOG_ERR, "Config error, could not find vitrual button for `%s`",param);
      }
    }
  } else if (strncasecmp(param, "sensor_", 7) == 0) {
    int num = strtoul(param + 7, NULL, 10) - 1;
    if (num + 1 > MAX_SENSORS || num < 0) {
      LOG(AQUA_LOG,LOG_ERR, "Config error, Maximum of %d sensors allowd `%s` ignored!",MAX_SENSORS,param);
    } else if (strlen(cleanwhitespace(value)) > 0) {
      if ( num + 1 > aqdata->num_sensors ) {
        aqdata->num_sensors = num + 1;
      }
      if (strncasecmp(param + 9, "_label", 6) == 0) {
        aqdata->sensors[num].label = ncleanalloc(value, AQ_MSGLEN);
        rtn=true;
      } else if (strncasecmp(param + 9, "_path", 5) == 0) {
        aqdata->sensors[num].path = cleanalloc(value);
        rtn=true;
      } else if (strncasecmp(param + 9, "_factor", 7) == 0) {
        aqdata->sensors[num].factor = atof(value);
        //printf("Factor = %f - %s\n",aqdata->sensors[num].factor, value);
        if (aqdata->sensors[num].factor == 0) {
          LOG(AQUA_LOG,LOG_ERR, "Config error, couldn't understand `%s` from `%s`, using `1.0`!",value,param);
          aqdata->sensors[num].factor = 1;
        }
      }
      rtn=true;
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Config error, blank value for `%s`\n",param);
      rtn = false;
    }

  }
//#endif

  return rtn;
}

aqkey *getVirtualButton(struct aqualinkdata *aqdata, int num)
{
  aqkey *vbutton = NULL;
  //char vbname[10];
  //snprintf(vbname, 9, "%s%d", BTN_VAUX, num);


  if (aqdata->virtual_button_start <= 0) {
    return addVirtualButton(aqdata, NULL, num);
  }

  // num should be the index of vbutton (which starts at max buttons).
  // num should also match the button NAME as in "Aux_V4"
  
  char vbname[10];
  snprintf(vbname, 9, "%s%d", BTN_VAUX, num);
  for (int i = aqdata->virtual_button_start; i < aqdata->total_buttons; i++)
  {
    // printf("Checking %s agasinsdt %s\n",aqdata->aqbuttons[i].name, vbname);
    if (strcmp(aqdata->aqbuttons[i].name, vbname) == 0)
    {
      vbutton = &aqdata->aqbuttons[i];
      break;
    }
  }

  if (vbutton == NULL) {
    return addVirtualButton(aqdata, NULL, num); 
  }

  return vbutton;
}

// pumpcfg is pointer to pumpIndex, pumpName, pumpType pumpID, (ie pull off button_??_ or vurtual_button_??_)
bool populatePumpData(struct aqualinkdata *aqdata, char *pumpcfg ,aqkey *button, char *value)
{

  pump_detail *pump = getPumpFromButtonID(aqdata, button);
  if (pump == NULL) {
    return false;
  }

  if (strncasecmp(pumpcfg, "pumpIndex", 9) == 0) {
    pump->pumpIndex = strtoul(value, NULL, 10);
  } else if (strncasecmp(pumpcfg, "pumpType", 8) == 0) {
    if ( stristr(value, "Pentair VS") != 0)
      pump->pumpType = VSPUMP;
    else if ( stristr(value, "Pentair VF") != 0)
      pump->pumpType = VFPUMP;
    else if ( stristr(value, "Jandy ePump") != 0)
      pump->pumpType = EPUMP;
  } else if (strncasecmp(pumpcfg, "pumpName", 8) == 0) { 
    strncpy(pump->pumpName ,cleanwhitespace(value), PUMP_NAME_LENGTH-1);
  } else if (strncasecmp(pumpcfg, "pumpID", 6) == 0) {
    pump->pumpID = strtoul(cleanalloc(value), NULL, 16);
    if ( (int)pump->pumpID >= PENTAIR_DEC_PUMP_MIN && (int)pump->pumpID <= PENTAIR_DEC_PUMP_MAX) {
      pump->prclType = PENTAIR;
    } else {
      pump->prclType = JANDY;
    }
  } else if (strncasecmp(pumpcfg, "pumpMaxSpeed", 12) == 0) {
    pump->maxSpeed = strtoul(value, NULL, 10);
  } else if (strncasecmp(pumpcfg, "pumpMinSpeed", 12) == 0) {
    pump->minSpeed = strtoul(value, NULL, 10);
  }

  return true;
}

pump_detail *getPumpFromButtonID(struct aqualinkdata *aqdata, aqkey *button)
{
  int pi;

  // Does it exist
  for (pi=0; pi < aqdata->num_pumps; pi++) {
    if (aqdata->pumps[pi].button == button) {
      return &aqdata->pumps[pi];
    }
  }

  // Create new entry
  if (aqdata->num_pumps < MAX_PUMPS) {
    //printf ("Creating pump %d\n",button);
    button->special_mask |= VS_PUMP;
    button->special_mask_ptr = (void*)&aqdata->pumps[aqdata->num_pumps];
    aqdata->pumps[aqdata->num_pumps].button = button;
    aqdata->pumps[aqdata->num_pumps].pumpType = PT_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].rpm = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].watts = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].gpm = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].pStatus = PS_OFF;
    aqdata->pumps[aqdata->num_pumps].pumpIndex = 0;
    aqdata->pumps[aqdata->num_pumps].maxSpeed = TEMP_UNKNOWN;
    aqdata->pumps[aqdata->num_pumps].minSpeed = TEMP_UNKNOWN;
    //pumpType
    aqdata->pumps[aqdata->num_pumps].pumpName[0] = '\0';
    aqdata->num_pumps++;
    return &aqdata->pumps[aqdata->num_pumps-1];
  }

  return NULL;
}

/*
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
    aqdata->pumps[aqdata->num_pumps].pStatus = PS_OFF;
    aqdata->pumps[aqdata->num_pumps].pumpIndex = 0;
    //pumpType
    aqdata->pumps[aqdata->num_pumps].pumpName[0] = '\0';
    aqdata->num_pumps++;
    return &aqdata->pumps[aqdata->num_pumps-1];
  }

  return NULL;
}
*/

void init_config()
{
  init_parameters(&_aqconfig_);
}

//void readCfg (struct aqconfig *config_parameters, struct aqualinkdata *aqdata, char *cfgFile)
void read_config (struct aqualinkdata *aqdata, char *cfgFile)
{
  _aqdata = aqdata;
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
            if ( ! setConfigValue(aqdata, b_ptr, indx+1)) {
              //LOG(AQUA_LOG,LOG_ERR, "Unknown config parameter '%.*s'\n",strlen(b_ptr), b_ptr);
              char *end = b_ptr + strlen(b_ptr) - 1;
              while(end > b_ptr && isspace(*end)) end--;
              LOG(AQUA_LOG,LOG_ERR, "Unknown config parameter '%.*s'\n",end-b_ptr+1, b_ptr);
            }
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

  //free(_defaultPanel);
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

//#ifdef CONFIG_EDITOR

#define MAX_PRINTLEN 35
void check_print_config (struct aqualinkdata *aqdata)
{
  int i, j;
  char name[MAX_PRINTLEN];


  // Sanity checks

  // If no panel has been set, use default one
  if (_aqconfig_.paneltype_mask == 0) {
    //printf("Set temp panel info Size=%d, RS=%d, combo=%d, dual=%d\n",_defaultPanel.size,_defaultPanel.rs,_defaultPanel.combo, _defaultPanel.dual);
    LOG(AQUA_LOG,LOG_ERR, "No panel set in config, using default values\n");
    setPanel(aqdata, _defaultPanel.rs, _defaultPanel.size, _defaultPanel.combo, _defaultPanel.dual);
  }

  // Make sure all sensors are fully populated
  for (i=0; i < aqdata->num_sensors; i++ ) {
    if ( aqdata->sensors[i].label == NULL ||  aqdata->sensors[i].path == NULL ) {
      LOG(AQUA_LOG,LOG_ERR, "Invalid sensor %d, removing!\n",i+1);
      if (i == (aqdata->num_sensors-1) ) { // last sensor
        // don't need to do anything, just reduce total number sensors
      } else if (aqdata->num_sensors > 1) { // there are more sensors adter this bad one
        for (int j=i; j < aqdata->num_sensors; j++) {
          aqdata->sensors[j].label = aqdata->sensors[j+1].label;
          aqdata->sensors[j].path = aqdata->sensors[j+1].path;
          aqdata->sensors[j].factor = aqdata->sensors[j+1].factor;
        }
      }
      aqdata->num_sensors --;
    }
  }

  // Check chiller
  if (ENABLE_CHILLER) {
    for (i = 0; i < aqdata->total_buttons; i++)
    {
      if (isVBUTTON_ALTLABEL(aqdata->aqbuttons[i].special_mask) && (rsm_strmatch(((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->altlabel, "Chiller") == 0) ){
        aqdata->chiller_button = &aqdata->aqbuttons[i];
      } else if (isVBUTTON(aqdata->aqbuttons[i].special_mask) && rsm_strmatch(aqdata->aqbuttons[i].label, "Heat Pump") == 0 ) {
        LOG(AQUA_LOG,LOG_ERR, "Config error, `%s` is enabled, but Virtual Button Heat Pump does not have alt_name Chiller! Creating.",CFG_N_force_chiller);
        setVirtualButtonAltLabel(&aqdata->aqbuttons[i], "Chiller");
        aqdata->chiller_button = &aqdata->aqbuttons[i];
        aqdata->chiller_button->special_mask |= VIRTUAL_BUTTON_CHILLER;
      }
    }
    if (aqdata->chiller_button == NULL) {
      LOG(AQUA_LOG,LOG_ERR, "Config error, `%s` is enabled, but no Virtual Button set for Heat Pump / Chiller! Creating vbutton.",CFG_N_force_chiller);
      aqkey *button = getVirtualButton(aqdata, 0);
      setVirtualButtonLabel(button, "Heat Pump");
      setVirtualButtonAltLabel(button, "Chiller");
      aqdata->chiller_button = button;
      aqdata->chiller_button->special_mask |= VIRTUAL_BUTTON_CHILLER;
    }
  }


  /* 
    _cfgParams[_numCfgParams].mask = READ_RS485_IAQUALNK;
    if ( bitmask READ_RS485_IAQUALNK && _aqconfig_.enable_iaqualink ) error and use (_aqconfig_.enable_iaqualink, disable bitmask
      if (_aqconfig_.enable_iaqualink)
        LOG(AQUA_LOG,LOG_WARNING, "Config error, 'read_RS485_iAqualink' is not valid when 'enable_iaqualink=yes', ignoring read_RS485_iAqualink!\n");
      else
        _aqconfig_.read_RS485_devmask |= READ_RS485_IAQUALNK;
    } else {
      _aqconfig_.read_RS485_devmask &= ~READ_RS485_IAQUALNK;
    }
  */

  /*
    PDA sleep and PDA ID.
  */

  /*  Need to handle this case
   } else if (strncasecmp (param, CFG_N_scheduler_check_pumpon_hour, CFG_C_scheduler_check_pumpon_hour) == 0) {
    _aqconfig_.sched_chk_pumpon_hour = strtoul(value, NULL, 10);
    _aqconfig_.schedule_event_mask |= AQS_DONT_USE_CRON_PUMP_TIME;
    rtn=true;
  } else if (strncasecmp (param, CFG_N_scheduler_check_pumpoff_hour, CFG_C_scheduler_check_pumpoff_hour) == 0) {
    _aqconfig_.sched_chk_pumpoff_hour = strtoul(value, NULL, 10);
    _aqconfig_.schedule_event_mask |= AQS_DONT_USE_CRON_PUMP_TIME;
    rtn=true;

  // maybebelow.  But this would only work the first time the config it read. (might need to clear CFG???)
   if
    _aqconfig_.sched_chk_pumpon_hour != TEMP_UNKNOWN || 
    _aqconfig_.sched_chk_pumpoff_hour != TEMP_UNKNOWN
  then
    _aqconfig_.schedule_event_mask |= AQS_DONT_USE_CRON_PUMP_TIME;
  */



  for ( i=0; i <= _numCfgParams; i++) {
    rsm_nchar_replace(name, MAX_PRINTLEN, _cfgParams[i].name, "_", " ");
    switch (_cfgParams[i].value_type) {
      case CFG_STRING:
        if (*(char **)_cfgParams[i].value_ptr == NULL)
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s =\n", name);
        else
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %s\n",name, *(char **)_cfgParams[i].value_ptr);
      break;
      case CFG_INT:
        if (*(int *)_cfgParams[i].value_ptr == TEMP_UNKNOWN)
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s =\n", name);
        else
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %d\n", name, *(int *)_cfgParams[i].value_ptr);
      break;
      case CFG_BOOL:
        LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %s\n", name, bool2text(*(bool *)_cfgParams[i].value_ptr));
      break;
      case CFG_HEX:
        LOG(AQUA_LOG,LOG_NOTICE, "%-35s = 0x%02hhx\n", name, *(unsigned char *)_cfgParams[i].value_ptr);
      break;
      case CFG_FLOAT:
        LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %f\n", name, *(float *)_cfgParams[i].value_ptr);
      break;
      case CFG_BITMASK:
        LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %s\n", name, (*(uint8_t *)_cfgParams[i].value_ptr & _cfgParams[i].mask) == _cfgParams[i].mask?bool2text(true):bool2text(false));
      break;
      case CFG_SPECIAL:
        if (strncasecmp(_cfgParams[i].name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %s\n", name, loglevel2cgn_name(_aqconfig_.log_level));
        } else if (strncasecmp(_cfgParams[i].name, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %s\n", name, getPanelString());
        } else {
          LOG(AQUA_LOG,LOG_NOTICE, "%-35s = NEED TO ADD CODE TO HANDLE THIS\n",name);
        }
      break;
    }
  }

  if (isAQS_START_PUMP_EVENT_ENABLED) {
    if (isAQS_USE_CRON_PUMP_TIME_ENABLED) {
      if (_aqconfig_.enable_scheduler) {
        get_cron_pump_times();
      } else {
        LOG(AQUA_LOG,LOG_ERR,"Scheduler is disabled, but use pump times from scheduler is enabled\n");
      }
    }
    //LOG(AQUA_LOG,LOG_NOTICE, "Start Pump on events          = %s %s %s\n",isAQS_POWER_ON_ENABED?"PowerON":"",AQS_FRZ_PROTECT_OFF?"FreezeProtect":"",AQS_BOOST_OFF?"Boost":"");
    LOG(AQUA_LOG,LOG_NOTICE, "%-35s = %d:00 and %d:00\n","Start Pump between times",_aqconfig_.sched_chk_pumpon_hour,_aqconfig_.sched_chk_pumpoff_hour);
  } /*else {
    LOG(AQUA_LOG,LOG_NOTICE, "Start Pump on events          = %s\n", bool2text(false));
  }*/

  
  //for (i = 0; i < TOTAL_BUTONS; i++)
  for (i = 0; i < aqdata->total_buttons; i++)
  {
    //char ext[] = " VSP ID None | AL ID 0 ";
    char ext[40];
    ext[0] = '\0';
    for (j = 0; j < aqdata->num_pumps; j++) {
      if (aqdata->pumps[j].button == &aqdata->aqbuttons[i]) {
        sprintf(ext, "VSP ID 0x%02hhx | PumpID %-1d | %s",
                  aqdata->pumps[j].pumpID, 
                  aqdata->pumps[j].pumpIndex, 
                  aqdata->pumps[j].pumpName[0]=='\0'?"":aqdata->pumps[j].pumpName);
      }
    }
    for (j = 0; j < aqdata->num_lights; j++) {
      if (aqdata->lights[j].button == &aqdata->aqbuttons[i]) {
        sprintf(ext,"Light Progm | CTYPE %-1d  |",aqdata->lights[j].lightType);
      }
    }
    if (isVBUTTON(aqdata->aqbuttons[i].special_mask)) {
      if (aqdata->aqbuttons[i].rssd_code != NUL) {
        sprintf(ext,"OneTouch %d  |",aqdata->aqbuttons[i].rssd_code - 15);
      }
    }
    if (isVBUTTON_ALTLABEL(aqdata->aqbuttons[i].special_mask)) {
      sprintf(ext,"%-12s|", ((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->altlabel);
    }
    if (aqdata->aqbuttons[i].dz_idx > 0) {
      sprintf(ext+strlen(ext), "dzidx %-3d",aqdata->aqbuttons[i].dz_idx);
    }
    
    LOG(AQUA_LOG,LOG_NOTICE, "Button %-13s = label %-15s | %s\n", 
                           aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label, ext);  
    

    if ( ((aqdata->aqbuttons[i].special_mask & VIRTUAL_BUTTON) == VIRTUAL_BUTTON)  && 
         ((aqdata->aqbuttons[i].special_mask & VS_PUMP ) != VS_PUMP) &&
          (_aqconfig_.extended_device_id < 0x30 || _aqconfig_.extended_device_id > 0x33 ) ){
      LOG(AQUA_LOG,LOG_WARNING, "Config error, extended_device_id must be on of the folowing (0x30,0x31,0x32,0x33) to use virtual button : '%s'",aqdata->aqbuttons[i].label);
    }

  }

  for (i = 0; i < aqdata->num_sensors; i++)
  {
    LOG(AQUA_LOG,LOG_NOTICE, "Config Sensor %02d     = label %-15s | %s\n", i+1, aqdata->sensors[i].label,aqdata->sensors[i].path);
  }

  

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

int save_config_js(const char* inBuf, int inSize, char* outBuf, int outSize, struct aqualinkdata *aqdata)
{
  //printf("\n%.*s\n",inSize,inBuf);

  //const char *regexString=" *\"([^\",:]+) *\" *: *\"([^\",:]+)\" *,*";
  const char *regexString=" *\"([^\",:]+) *\" *: *\"([^\",]*)\" *,*";
  size_t maxGroups = 3;
  size_t maxMatches = 200;
  //size_t maxGroups = 3;
  regmatch_t groupArray[maxGroups];
  regex_t regexCompiled;
  int rc;
  char * cursor = (char *)inBuf;;
  unsigned int m;
  char key[64];
  char value[64];
  int psize = PANEL_SIZE(); // Get current panel size
  int ignodeBtnLabelsGrater = TOTAL_BUTTONS+1;
  bool ignorePair = false;

  // First clear out all special current config items.
  // Light Programs
  clear_aqualinkd_light_modes();
  // Lights, Buttons, Pumps, VirtButtons
  for (int i = 0; i < aqdata->total_buttons; i++)
  {
    if (isVS_PUMP(aqdata->aqbuttons[i].special_mask)) {
      ((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->button = NULL;
    } else if (isPLIGHT(aqdata->aqbuttons[i].special_mask)) {
      ((clight_detail *)aqdata->aqbuttons[i].special_mask_ptr)->button = NULL;
    }
    free ( aqdata->aqbuttons[i].label);
    aqdata->aqbuttons[i].special_mask = 0;
    aqdata->aqbuttons[i].special_mask_ptr = NULL;
    aqdata->aqbuttons[i].dz_idx = AQ_UNKNOWN;
    aqdata->aqbuttons[i].code = NUL;
    aqdata->aqbuttons[i].rssd_code = NUL;
  }
  aqdata->num_pumps = 0;
  aqdata->num_lights = 0;
  
  // Sensors
  for (int i=0; i < aqdata->num_sensors; i++ ) {
    free(aqdata->sensors[i].label);
    free(aqdata->sensors[i].path);
    aqdata->sensors[i].label = NULL;
    aqdata->sensors[i].path = NULL;
  }
  aqdata->num_sensors=0;

  // These should get set once we get panel info.
  aqdata->total_buttons = 0;
  aqdata->virtual_button_start = 0;

  // Now we can loop over the JS and set values.

  const char *sptr = strstr( inBuf, "values" );

  if (_aqdata == NULL) {
    LOG(AQUA_LOG,LOG_ERR, "Saving config, not initialized:\n'%.*s'\n", inSize, inBuf);
    return snprintf(outBuf, outSize, "{\"message\":\"ERROR saving config\"}"); 
  }

  if (sptr == NULL) {
    LOG(AQUA_LOG,LOG_ERR, "Saving config, invalid JSON:\n'%.*s'\n", inSize, inBuf);
    return snprintf(outBuf, outSize, "{\"message\":\"ERROR in Config\"}"); 
  }

  if (0 != (rc = regcomp(&regexCompiled, regexString, REG_EXTENDED))) {
    LOG(AQUA_LOG,LOG_ERR, "Saving config regcomp() failed, returning nonzero (%d)\n", rc);
    return snprintf(outBuf, outSize, "{\"message\":\"ERROR in Config\"}"); 
  }

  //cursor = inBuf+start+1;
  for (m = 0; m < maxMatches; m ++)
  {
    if (0 != (rc = regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))) {
      //printf("Failed to match '%s' with '%s',returning %d.\n", cursor, pattern, rc);
      break;
    }    

    snprintf(key, 64, "%.*s", (groupArray[1].rm_eo - groupArray[1].rm_so), (cursor + groupArray[1].rm_so));
    snprintf(value, 64, "%.*s",   (groupArray[2].rm_eo - groupArray[2].rm_so), (cursor + groupArray[2].rm_so));
    //printf("**** Pair = %s : %s \n",key,value);

    // If panel size changed, see if we should ignore the label
    if (strncasecmp(key, "button_", 7 ) == 0) {
      if ( strtoul(key + 7, NULL, 10) >= ignodeBtnLabelsGrater) {
        ignorePair = true;
        LOG(AQUA_LOG,LOG_NOTICE, "Panel size changed, ignoring %s\n",key);
      }
    }

    if (!ignorePair) {
      setConfigValue(_aqdata ,key,value);
    }

    // Check if panel size has changed
    if (strncasecmp(key, CFG_N_panel_type, (int)strlen(CFG_N_panel_type) ) == 0) {
      if (psize != PANEL_SIZE()) {
        // Panel size changed
        ignodeBtnLabelsGrater = PANEL_SIZE();
      }
    }

    cursor += groupArray[0].rm_eo;
  }

  regfree(&regexCompiled);

  check_print_config(aqdata);
  writeCfg(aqdata);

  return sprintf(outBuf, "{\"message\":\"Saved Config\"}"); 
}

//#endif
//#ifdef CONFIG_EDITOR

const char *pumpType2String(pump_type ptype) {
  switch (ptype) {
    case EPUMP:
      return "JANDY ePUMP";
    break;
    case VSPUMP:
      return "Pentair VF";
    break;
    case VFPUMP:
      return "Pentair VS";
    break;
    case PT_UNKNOWN:
    default:
      return "unknown";
    break;
  }
}

bool writeCfg (struct aqualinkdata *aqdata)
{ 
  int i;
  FILE *fp;
  char *lastName = NULL;
  bool ro_root;
  bool created_file;
  
  //LOG(AQUA_LOG,LOG_ERR, "writeCfg() not implimented\n");

  //fp = fopen(_aqconfig_.config_file, "w");
  //fp = fopen("/tmp/aqualinkd.conf", "w");

  char backup_file[256];
  sprintf(backup_file,"%s.backup",_aqconfig_.config_file);

  if (copy_file(_aqconfig_.config_file, backup_file) != true) {
    LOG(AQUA_LOG,LOG_WARNING,"Couldn't make a backup `%s` of config file `%s`\n",backup_file, _aqconfig_.config_file);
  } else {
    LOG(AQUA_LOG,LOG_NOTICE,"Made backup of config %s\n",backup_file);
  }

  fp = aq_open_file(_aqconfig_.config_file, &ro_root, &created_file);

  if (fp == NULL) {
    LOG(AQUA_LOG,LOG_ERR, "Open config file failed '%s'\n", _aqconfig_.config_file);
    //remount_root_ro(true);
    //fprintf(stdout, "Open file failed 'sprinkler.cron'\n");
    return false;
  }
  
  //char fp[100];

  for ( i=0; i <= _numCfgParams; i++) {
    //printf("Writing %s\n",_cfgParams[i].name);
    // Group values by fist letter, if the same group together.
    if (lastName != NULL && lastName[0] != _cfgParams[i].name[0]) {
      if ( lastName != NULL && rsm_strstr(lastName, "device_id") != NULL && rsm_strstr(_cfgParams[i].name, "device_id") != NULL ) {

      } else {
        fprintf(fp,"\n");
      }
    }

    switch (_cfgParams[i].value_type) {
      case CFG_STRING:
        if (*(char **)_cfgParams[i].value_ptr == NULL)
          fprintf(fp, "#%s=\n", _cfgParams[i].name);
        else
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, *(char **)_cfgParams[i].value_ptr);
      break;
      case CFG_INT:
        if (*(int *)_cfgParams[i].value_ptr == TEMP_UNKNOWN)
          fprintf(fp, "#%s=\n", _cfgParams[i].name);
        else
          fprintf(fp, "%s=%d\n", _cfgParams[i].name, *(int *)_cfgParams[i].value_ptr);
      break;
        case CFG_BOOL:
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, bool2text(*(bool *)_cfgParams[i].value_ptr));
        break;
        case CFG_HEX:
          fprintf(fp, "%s=0x%02hhx\n", _cfgParams[i].name, *(unsigned char *)_cfgParams[i].value_ptr);
        break;
         case CFG_FLOAT:
          fprintf(fp, "%s=%f\n", _cfgParams[i].name, *(float *)_cfgParams[i].value_ptr);
        break;
        case CFG_BITMASK:
          fprintf(fp, "%s=%s\n", _cfgParams[i].name, (*(uint8_t *)_cfgParams[i].value_ptr & _cfgParams[i].mask) == _cfgParams[i].mask? bool2text(true):bool2text(false));
        break;
        case CFG_SPECIAL:
          if (strncasecmp(_cfgParams[i].name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
            fprintf(fp, "%s=%s\n", _cfgParams[i].name, loglevel2cgn_name(_aqconfig_.log_level));
          } else if (strncasecmp(_cfgParams[i].name, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
            fprintf(fp, "%s=%s\n", _cfgParams[i].name, getPanelString());
          } else {
            fprintf(fp, "%s=NEED TO ADD CODE TO HANDLE THIS\n",_cfgParams[i].name);
          }
        break;
    }
    lastName = _cfgParams[i].name;
  }

  fprintf(fp,"\n");

  // Add custom censors
  for (i = 1; i <= aqdata->num_sensors; i++)
  {
    fprintf(fp,"\nsensor_%.2d_path=%s\n",i,aqdata->sensors[i-1].path);
    fprintf(fp,"sensor_%.2d_label=%s\n",i,aqdata->sensors[i-1].label);
    fprintf(fp,"sensor_%.2d_factor=%f\n",i,aqdata->sensors[i-1].factor);
  }

  fprintf(fp,"\n");
  //  add custom light modes/colors
  bool isShow;
  const char *lname;
  for (i=1; i < LIGHT_COLOR_OPTIONS; i++) {
    if ((lname = get_aqualinkd_light_mode_name(i, &isShow)) != NULL) {
      fprintf(fp,"light_program_%.2d=%s%s\n",i,lname,isShow?" - show":"");
    } else {
      break;
    }
  }

  fprintf(fp,"\n");
  // Buttons
  for (i = 0; i < aqdata->total_buttons; i++)
  {
    char prefix[30];
    if (isVBUTTON(aqdata->aqbuttons[i].special_mask)) {
      sprintf(prefix,"virtual_button_%.2d",(i+1)-aqdata->virtual_button_start);
    } else {
      sprintf(prefix,"button_%.2d",i+1);
    }
    
    fprintf(fp,"\n%s_label=%s\n", prefix, aqdata->aqbuttons[i].label);

    if (isVS_PUMP(aqdata->aqbuttons[i].special_mask)) 
    {
      if (((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpIndex > 0) {
        fprintf(fp,"%s_pumpIndex=%d\n", prefix, ((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpIndex);
      }
      
      if (((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpID != NUL) {
        fprintf(fp,"%s_pumpID=0x%02hhx\n", prefix, ((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpID);
      }

      if (((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpName[0] != '\0') {
        fprintf(fp,"%s_pumpName=%s\n", prefix, ((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpName);
      }

      if (((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpType != PT_UNKNOWN) {
        fprintf(fp,"%s_pumpType=%s\n", prefix, pumpType2String(((pump_detail *)aqdata->aqbuttons[i].special_mask_ptr)->pumpType));
      }
    } else if (isPLIGHT(aqdata->aqbuttons[i].special_mask)) {
      //if (((clight_detail *)aqdata->aqbuttons[i].special_mask_ptr)->lightType > 0) {
      fprintf(fp,"%s_lightMode=%d\n", prefix, ((clight_detail *)aqdata->aqbuttons[i].special_mask_ptr)->lightType);
      //}
    } else if ( (isVBUTTON(aqdata->aqbuttons[i].special_mask) && aqdata->aqbuttons[i].rssd_code >= IAQ_ONETOUCH_1 && aqdata->aqbuttons[i].rssd_code <= IAQ_ONETOUCH_6 ) ) {
      fprintf(fp,"%s_onetouchID=%d\n", prefix, (aqdata->aqbuttons[i].rssd_code - 15));
    }
    
    if (isVBUTTON_ALTLABEL(aqdata->aqbuttons[i].special_mask)) {
      fprintf(fp,"%s_altLabel=%s\n", prefix, ((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->altlabel);
    }
 
  }


  //remount_root_ro(fs);

  fprintf(fp,"\n");
  //fclose(fp);
  aq_close_file(fp, ro_root);

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

//#endif
