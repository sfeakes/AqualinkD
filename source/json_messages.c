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
#include <string.h>
#include <stdbool.h>

#include "aqualink.h"
#include "config.h"
//#include "aq_programmer.h"
#include "utils.h"
//#include "web_server.h"
#include "json_messages.h"
#include "domoticz.h"
#include "aq_mqtt.h"
#include "devices_jandy.h"
#include "version.h"
#include "aq_timer.h"
#include "aq_programmer.h"
#include "rs_msg_utils.h"
#include "color_lights.h"
#include "iaqualink.h"

//#define test_message "{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}"
//#define test_labels "{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}"

//#define test_message "{\"type\": \"status\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"on\",\"Spa_Mode\": \"on\",\"Aux_1\": \"on\",\"Aux_2\": \"on\",\"Aux_3\": \"on\",\"Aux_4\": \"on\",\"Aux_5\": \"on\",\"Aux_6\": \"on\",\"Aux_7\": \"on\",\"Pool_Heater\": \"on\",\"Spa_Heater\": \"on\",\"Solar_Heater\": \"on\"}}"

//{"type": "aux_labels","Pool Pump": "Pool Pump","Spa Mode": "Spa Mode","Cleaner": "Aux 1","Waterfall": "Aux 2","Spa Blower": "Aux 2","Pool Light": "Aux 4","Spa Light ": "Aux 5","Aux 6": "Aux 6","Aux 7": "Aux 7","Heater": "Heater","Heater": "Heater","Solar Heater": "Solar Heater","(null)": "(null)"}

//SPA WILL TURN OFF AFTER COOL DOWN CYCLE


bool printableChar(char ch)
{
  if ( (ch < 32 || ch > 126) || 
        ch == 123 || // {
        ch == 125 || // }
        ch == 34 || // "
        ch == 92    // backslash
  ) {
    return false;
  }
  return true;

}

int json_chars(char *dest, const char *src, int dest_len, int src_len)
{
  int i;
  int end = dest_len < src_len ? dest_len:src_len;
  for(i=0; i < end; i++) {
    /*
    if ( (src[i] < 32 || src[i] > 126) || 
          src[i] == 123 || // {
          src[i] == 125 || // }
          src[i] == 34 || // "
          src[i] == 92    // backslash
       ) // only printable chars*/
    if (! printableChar(src[i]))
      dest[i] = ' ';
    else
      dest[i] = src[i];
  }

  i--;
  /* // Don't delete trailing whitespace
  while (dest[i] == ' ')
    i--;
    */

  if (dest[i] != '\0') {
    if (i < (dest_len-1))
      i++;

    dest[i] = '\0';
  }
  
  return i;
}

int build_logmsg_JSON(char *dest, int loglevel, const char *src, int dest_len, int src_len)
{
  int length = sprintf(dest, "{\"logmsg\":\"%-7s",elevel2text(loglevel));
  length += json_chars(dest+length, src, (dest_len-20), src_len);
  length += sprintf(dest+length, "\"}");
  dest[length] = '\0';
  //dest[length] = '\n';
  //dest[length+1] = '\0';

  return length;
}

const char* _getStatus(struct aqualinkdata *aqdata, const char *blankmsg)
{
  /*
  if (aqdata->active_thread.thread_id != 0 && !aqdata->simulate_panel) {
    //return JSON_PROGRAMMING;
    return programtypeDisplayName(aqdata->active_thread.ptype);
  }
  */

/*
 printf("**** Programming=%s, length=%ld, empty=%s, Message=%s \n",
         aqdata->active_thread.thread_id==0?"no":"yes",
         strlen(aqdata->last_display_message),
         rsm_isempy(aqdata->last_display_message,strlen(aqdata->last_display_message))==true?"yes":"no",
         aqdata->last_display_message);
 */

  // If only one bit set (conected) then ignore all these if's
  if (aqdata->status_mask != CONNECTED) {
    if ((aqdata->status_mask & ERROR_SERIAL) == ERROR_SERIAL)
      return "ERROR No Serial connection";
    else if ((aqdata->status_mask & ERROR_NO_DEVICE_ID) == ERROR_NO_DEVICE_ID)
      return "ERROR No device ID";
    else if ((aqdata->status_mask & CHECKING_CONFIG) == CHECKING_CONFIG)
      return "Checking Config";
    else if ((aqdata->status_mask & AUTOCONFIGURE_ID) == AUTOCONFIGURE_ID)
      return "Searching for free device ID's, Please wait!";
    else if ((aqdata->status_mask & AUTOCONFIGURE_PANEL) == AUTOCONFIGURE_PANEL)
      return "Getting Panel Information";
    else if ((aqdata->status_mask & CONNECTING) == CONNECTING)
      return "Connecting (waiting for control panel)";
    else if ((aqdata->status_mask & NOT_CONNECTED) == NOT_CONNECTED)
      return "Not Connected to panel";
  }

 if (aqdata->active_thread.thread_id != 0) {
   if (!aqdata->is_display_message_programming || rsm_isempy(aqdata->last_display_message,strlen(aqdata->last_display_message))){
     return programtypeDisplayName(aqdata->active_thread.ptype);
   }
 }
 
  
 //if (aqdata->last_message != NULL && stristr(aqdata->last_message, "SERVICE") != NULL ) {
  if (aqdata->service_mode_state == ON) {
    return JSON_SERVICE;
  } else if (aqdata->service_mode_state == FLASH) {
    return JSON_TIMEOUT;
  }

  if (aqdata->last_display_message[0] != '\0') {
    int i;
    for(i=0; i < strlen(aqdata->last_display_message); i++ ) {
      if (! printableChar(aqdata->last_display_message[i])) {
        aqdata->last_display_message[i] = ' ';
      }
    }
    //printf("JSON Sending '%s'\n",aqdata->last_display_message);
    return aqdata->last_display_message;
  }
/*
  if (aqdata->display_last_message == true) {
    return aqdata->last_message;
  }
*/
  return blankmsg; 
}

const char* getStatus(struct aqualinkdata *aqdata) {
  return _getStatus(aqdata, JSON_READY);
}
/* External version of above */
const char* getAqualinkDStatusMessage(struct aqualinkdata *aqdata) {
  return _getStatus(aqdata, " ");
}


int build_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float tvalue/*char *svalue*/)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  if (tvalue == TEMP_UNKNOWN) {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"\"}", idx, nvalue);
  } else {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"stype\":\"SetPoint\",\"svalue\":\"%.2f\"}", idx, nvalue, tvalue);
  }

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_mqtt_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  //json.htm?type=command&param=udevice&idx=IDX&nvalue=LEVEL&svalue=TEXT

  length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%s\"}", idx, nvalue, svalue);

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_aqualink_error_status_JSON(char* buffer, int size, const char *msg)
{
  //return snprintf(buffer, size, "{\"type\": \"error\",\"status\":\"%s\"}", msg);
  return snprintf(buffer, size, "{\"type\": \"status\",\"status\":\"%s\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"off\",\"Spa_Mode\": \"off\",\"Aux_1\": \"off\",\"Aux_2\": \"off\",\"Aux_3\": \"off\",\"Aux_4\": \"off\",\"Aux_5\": \"off\",\"Aux_6\": \"off\",\"Aux_7\": \"off\",\"Pool_Heater\": \"off\",\"Spa_Heater\": \"off\",\"Solar_Heater\": \"off\"}}", msg);

}

char *LED2text(aqledstate state)
{
  switch (state) {
    case ON:
      return JSON_ON;
    break;
    case OFF:
      return JSON_OFF;
    break;
    case FLASH:
      return JSON_FLASH;
    break;
    case ENABLE:
      return JSON_ENABLED;
    break;
    case LED_S_UNKNOWN:
    default:
      return "unknown";
    break;
  }
}

int LED2int(aqledstate state)
{
  switch (state) {
    case ON:
      return 1;
    break;
    case OFF:
      return 0;
    break;
    case FLASH:
      return 2;
    break;
    case ENABLE:
      return 3;
    break;
    case LED_S_UNKNOWN:
    default:
      return 4;
    break;
  }
}

#define AUX_BUFFER_SIZE 200

char *get_aux_information(aqkey *button, struct aqualinkdata *aqdata, char *buffer)
{
  int i;
  int length = 0;
  buffer[0] = '\0';

  //if ((button->special_mask & VS_PUMP) == VS_PUMP)
  if (isVS_PUMP(button->special_mask))
  {
//printf("Button %s is VSP\n", button->name);
    for (i=0; i < aqdata->num_pumps; i++) {
      if (button == aqdata->pumps[i].button) {       
          length += sprintf(buffer, ",\"type_ext\":\"switch_vsp\",\"Pump_RPM\":\"%d\",\"Pump_GPM\":\"%d\",\"Pump_Watts\":\"%d\",\"Pump_Type\":\"%s\",\"Pump_Status\":\"%d\",\"Pump_Speed\":\"%d\"", 
                  aqdata->pumps[i].rpm,
                  aqdata->pumps[i].gpm,
                  aqdata->pumps[i].watts,
                  (aqdata->pumps[i].pumpType==VFPUMP?"vfPump":(aqdata->pumps[i].pumpType==VSPUMP?"vsPump":"ePump")),
                  getPumpStatus(i, aqdata),
                  getPumpSpeedAsPercent(&aqdata->pumps[i]));

          return buffer;
      }
    }
  } 
  //else if ((button->special_mask & PROGRAM_LIGHT) == PROGRAM_LIGHT)
  else if (isPLIGHT(button->special_mask))
  {
//printf("Button %s is ProgramableLight\n", button->name);
    for (i=0; i < aqdata->num_lights; i++) {
      if (button == aqdata->lights[i].button) {
        if (aqdata->lights[i].lightType == LC_DIMMER2) {
          length += sprintf(buffer, ",\"type_ext\": \"light_dimmer\", \"Light_Type\":\"%d\", \"Light_Program\":\"%d\", \"Program_Name\":\"%d%%\" ",
                                  aqdata->lights[i].lightType,
                                  aqdata->lights[i].currentValue,
                                  aqdata->lights[i].currentValue);
        } else {
          length += sprintf(buffer, ",\"type_ext\": \"switch_program\", \"Light_Type\":\"%d\", \"Light_Program\":\"%d\", \"Program_Name\":\"%s\" ",
                                  aqdata->lights[i].lightType,
                                  aqdata->lights[i].currentValue,
                                  get_currentlight_mode_name(aqdata->lights[i], ALLBUTTON));
                                  //light_mode_name(aqdata->lights[i].lightType, aqdata->lights[i].currentValue, ALLBUTTON));
        }
        return buffer;
      }
    }
  }
  if (isVBUTTON_ALTLABEL(button->special_mask))
  {
    length += sprintf(buffer, ",\"alt_label\":\"%s\", \"in_alt_mode\": \"%s\" ",((vbutton_detail *)button->special_mask_ptr)->altlabel, ((vbutton_detail *)button->special_mask_ptr)->in_alt_mode?JSON_ON:JSON_OFF );
    //return buffer;
  }

//printf("Button %s is Switch\n", button->name);
  length += sprintf(buffer+length, ",\"type_ext\": \"switch_timer\", \"timer_active\":\"%s\"", (((button->special_mask & TIMER_ACTIVE) == TIMER_ACTIVE)?JSON_ON:JSON_OFF) );
  if ((button->special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
    length += sprintf(buffer+length,",\"timer_duration\":\"%d\"", get_timer_left(button));
  }
  return buffer;
}

//int build_device_JSON(struct aqualinkdata *aqdata, int programable_switch1, int programable_switch2, char* buffer, int size, bool homekit)
int build_device_JSON(struct aqualinkdata *aqdata, char* buffer, int size, bool homekit)
{
  char aux_info[AUX_BUFFER_SIZE];
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  // IF temp units are F assume homekit is using F
  bool homekit_f = (homekit && ( aqdata->temp_units==FAHRENHEIT || aqdata->temp_units == UNKNOWN) );

  length += sprintf(buffer+length, "{\"type\": \"devices\"");
  length += sprintf(buffer+length, ",\"aqualinkd_version\":\"%s\"",AQUALINKD_VERSION);//"09/01/16 THU",
  length += sprintf(buffer+length, ",\"date\":\"%s\"",aqdata->date );//"09/01/16 THU",
  length += sprintf(buffer+length, ",\"time\":\"%s\"",aqdata->time );//"1:16 PM",
  if ( aqdata->temp_units == FAHRENHEIT )
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"",JSON_FAHRENHEIT );
  else if ( aqdata->temp_units == CELSIUS )
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"", JSON_CELSIUS);
  else
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"",JSON_UNKNOWN );

  length += sprintf(buffer+length,  ", \"devices\": [");
  
  for (i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0 && (ENABLE_HEATERS || aqdata->pool_htr_set_point != TEMP_UNKNOWN)) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_thermo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\", \"timer_active\":\"%s\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->pool_htr_set_point):aqdata->pool_htr_set_point),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->pool_temp):aqdata->pool_temp),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     ((aqdata->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE?JSON_ON:JSON_OFF) );

    } else if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name)==0 && (ENABLE_HEATERS || aqdata->spa_htr_set_point != TEMP_UNKNOWN)) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_thermo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\", \"timer_active\":\"%s\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->spa_htr_set_point):aqdata->spa_htr_set_point),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->spa_temp):aqdata->spa_temp),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     ((aqdata->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE?JSON_ON:JSON_OFF));

    } else {
      if (!homekit && ENABLE_CHILLER && isVBUTTON_CHILLER(aqdata->aqbuttons[i].special_mask) ) {
        // We will add this VButton as a thermostat
        continue;
      }
      get_aux_information(&aqdata->aqbuttons[i], aqdata, aux_info);
      //length += sprintf(buffer+length, "{\"type\": \"switch\", \"type_ext\": \"switch_vsp\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\" %s},", 
      length += sprintf(buffer+length, "{\"type\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\" %s},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     aux_info);
    }
    /*    
    } else if ( (programable_switch1 > 0 && programable_switch1 == i) || 
                (programable_switch2 > 0 && programable_switch2 == i)) {
        length += sprintf(buffer+length, "{\"type\": \"switch\", \"type_ext\": \"switch_program\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\"},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state));
    } else {
      if ( get_aux_information(&aqdata->aqbuttons[i], aqdata, aux_info)[0] == '\0' ) {
        length += sprintf(buffer+length, "{\"type\": \"switch\", \"type_ext\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\"},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state));
      } else {
        length += sprintf(buffer+length, "{\"type\": \"switch\", \"type_ext\": \"switch_vsp\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\" %s},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     aux_info);
                                     //get_aux_information(&aqdata->aqbuttons[i], aqdata, aux_info));
      }
    }*/
  }

  if ( ENABLE_FREEZEPROTECT || (aqdata->frz_protect_set_point != TEMP_UNKNOWN && aqdata->air_temp != TEMP_UNKNOWN) ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_freeze\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     FREEZE_PROTECT,
                                    "Freeze Protection",
                                    aqdata->frz_protect_state==ON?JSON_ON:JSON_OFF,
                                    aqdata->frz_protect_state==ON?LED2text(ON):LED2text(ENABLE),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->frz_protect_set_point):aqdata->frz_protect_set_point),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->air_temp):aqdata->air_temp),
                                    aqdata->frz_protect_state==ON?1:0);
  }

  if ( (ENABLE_CHILLER || (aqdata->chiller_set_point != TEMP_UNKNOWN && getWaterTemp(aqdata) != TEMP_UNKNOWN)) && (aqdata->chiller_button != NULL) ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_chiller\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
      CHILLER,
     "Heat Pump Chiller",
     aqdata->chiller_button->led->state==ON?JSON_ON:JSON_OFF,
     //((vbutton_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode?JSON_ON:JSON_OFF,
     aqdata->chiller_button->led->state==ON?LED2text(ON):LED2text(ENABLE),
     //((vbutton_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode?(aqdata->chiller_button->led->state==ON?LED2text(ON):LED2text(ENABLE)):JSON_OFF,
     ((homekit)?2:0),
     ((homekit_f)?degFtoC(aqdata->chiller_set_point):aqdata->chiller_set_point),
     ((homekit)?2:0),
     ((homekit_f)?degFtoC(getWaterTemp(aqdata)):getWaterTemp(aqdata)),
     aqdata->chiller_button->led->state==ON?1:0);
  }

  if (aqdata->swg_led_state != LED_S_UNKNOWN) {
    if ( aqdata->swg_percent != TEMP_UNKNOWN ) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_swg\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     SWG_TOPIC,
                                    "Salt Water Generator",
                                    //aqdata->ar_swg_status == SWG_STATUS_OFF?JSON_OFF:JSON_ON,
                                    aqdata->swg_led_state == OFF?JSON_OFF:JSON_ON,
                                    //LED2text(get_swg_led_state(aqdata)),
                                    LED2text(aqdata->swg_led_state),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent),
                                    LED2int(aqdata->swg_led_state) );
                                    //aqdata->ar_swg_status == SWG_STATUS_OFF?LED2int(OFF):LED2int(ON));

    //length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
      length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?SWG_PERCENT_F_TOPIC:SWG_PERCENT_TOPIC),
                                   "Salt Water Generator Percent",
                                   "on",
                                   ((homekit_f)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent));
      //if (!homekit) { // For the moment keep boost off homekit   

        length += sprintf(buffer+length, "{\"type\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\"},", 
                                     SWG_BOOST_TOPIC, 
                                     "SWG Boost",
                                     aqdata->boost?JSON_ON:JSON_OFF,
                                     aqdata->boost?JSON_ON:JSON_OFF,
                                     aqdata->boost?LED2int(ON):LED2int(OFF));
      //}
    }

    if ( aqdata->swg_ppm != TEMP_UNKNOWN ) {

      length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                     ((homekit_f)?SWG_PPM_F_TOPIC:SWG_PPM_TOPIC),
                                     "Salt Level PPM",
                                     "on",
                                     ((homekit)?2:0),
                                     ((homekit_f)?roundf(degFtoC(aqdata->swg_ppm)):aqdata->swg_ppm)); 
                                   
     /*         
   length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
                                   SWG_PPM_TOPIC,
                                   "Salt Level PPM",
                                   "on",
                                   aqdata->swg_ppm);
   */
                                   
    }
  }

  if ( aqdata->ph != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?CHRM_PH_F_TOPIC:CHEM_PH_TOPIC),
                                   "Water Chemistry pH",
                                   "on",
                                   ((homekit)?2:1),
                                   ((homekit_f)?(degFtoC(aqdata->ph)):aqdata->ph)); 
  }
  if ( aqdata->orp != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?CHRM_ORP_F_TOPIC:CHEM_ORP_TOPIC),
                                   "Water Chemistry ORP",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?(degFtoC(aqdata->orp)):aqdata->orp)); 
  }

  length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   AIR_TEMP_TOPIC,
                                   /*AIR_TEMPERATURE,*/
                                   "Pool Air Temperature",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->air_temp):aqdata->air_temp));
  length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   POOL_TEMP_TOPIC,
                                   /*POOL_TEMPERATURE,*/
                                   "Pool Water Temperature",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->pool_temp):aqdata->pool_temp));
  length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   SPA_TEMP_TOPIC,
                                   /*SPA_TEMPERATURE,*/
                                   "Spa Water Temperature",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->spa_temp):aqdata->spa_temp));

  for (i=0; i < aqdata->num_sensors; i++) 
  {
    if (aqdata->sensors[i].value != TEMP_UNKNOWN) {
       //length += sprintf(buffer+length, "\"%s\": \"%.2f\",", aqdata->sensors[i].label, aqdata->sensors[i].value );
      length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s/%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
        SENSOR_TOPIC,aqdata->sensors[i].label,
        aqdata->sensors[i].label,
        "on",
        ((homekit)?2:0),
        ((homekit_f)?aqdata->sensors[i].value:aqdata->sensors[i].value));
    }
  }
/*
  length += sprintf(buffer+length,  "], \"aux_device_detail\": [");
  for (i=0; i < MAX_PUMPS; i++) {
  }
*/
  if (buffer[length-1] == ',')
    length--;

  length += sprintf(buffer+length, "]}");

  LOG(NET_LOG,LOG_DEBUG, "JSON: %s used %d of %d\n", homekit?"homebridge":"web", length, size);

  buffer[length] = '\0';

  return strlen(buffer);
  
  //return length;
}

int logmaskjsonobject(logmask_t flag, char* buffer)
{
  int length = sprintf(buffer, "{\"name\":\"%s\",\"id\":\"%d\",\"set\":\"%s\"},", logmask2name(flag), flag,(isDebugLogMaskSet(flag)?JSON_ON:JSON_OFF));

  if (flag == RSSD_LOG) {
    //length = sprintf(buffer, "{\"name\":\"%s\",\"id\":\"%d\",\"set\":\"%s\",\"filter\":\"0x%02hhx\"},", logmask2name(flag), flag,(isDebugLogMaskSet(flag)?JSON_ON:JSON_OFF), _aqconfig_.RSSD_LOG_filter[0]);
    length = sprintf(buffer, "{\"name\":\"%s\",\"id\":\"%d\",\"set\":\"%s\",\"filters\":[", logmask2name(flag), flag,(isDebugLogMaskSet(flag)?JSON_ON:JSON_OFF));
    for (int i=0; i < MAX_RSSD_LOG_FILTERS; i++) {
      length += sprintf(buffer+length, "\"0x%02hhx\",", _aqconfig_.RSSD_LOG_filter[i]);
    }
    //"]},"
    length += sprintf(buffer+length-1, "]},");
    length--;
  }
  return length;
}
int logleveljsonobject(int level, char* buffer)
{
  int length = sprintf(buffer, "{\"name\":\"%s\",\"id\":\"%d\",\"set\":\"%s\"},", loglevel2name(level), level,(getSystemLogLevel()==level?JSON_ON:JSON_OFF));
  return length;
}

int build_aqualink_aqmanager_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  length += sprintf(buffer+length, "{\"type\": \"aqmanager\"");
  length += sprintf(buffer+length, ",\"deamonized\": \"%s\"", (_aqconfig_.deamonize?JSON_ON:JSON_OFF) );

  if ( isMASK_SET(aqdata->status_mask,AUTOCONFIGURE_ID ) ||
       isMASK_SET(aqdata->status_mask,AUTOCONFIGURE_PANEL ) /*||
       isMASK_SET(aqdata->status_mask,CONNECTING )*/  ) 
  {
    length += sprintf(buffer+length, ",\"config_editor\": \"no\"");
  } else {
    length += sprintf(buffer+length, ",\"config_editor\": \"yes\"");
  }

  
  /*
  length += sprintf(buffer+length, ",\"panel_type\":\"%s\"",getPanelString());
  length += sprintf(buffer+length, ",\"version\":\"%s\"",aqdata->version );//8157 REV MMM",
  length += sprintf(buffer+length, ",\"aqualinkd_version\":\"%s\"", AQUALINKD_VERSION ); //1.0b,
  */
  //length += sprintf(buffer+length, ",\"logging2file\": \"%s\"",islogFileReady()?JSON_ON:JSON_OFF);
  //length += sprintf(buffer+length, ",\"logfileready\": \"%s\"",islogFileReady()?JSON_ON:JSON_OFF);
  //length += sprintf(buffer+length, ",\"logfilename\": \"%s\"",_aqconfig_.log_file);
  length += sprintf(buffer+length, ",\"debugmasks\":[");
  length += logmaskjsonobject(AQUA_LOG, buffer+length);
  length += logmaskjsonobject(NET_LOG, buffer+length);
  length += logmaskjsonobject(ALLB_LOG, buffer+length);
  length += logmaskjsonobject(ONET_LOG, buffer+length);
  length += logmaskjsonobject(IAQT_LOG, buffer+length);
  length += logmaskjsonobject(PDA_LOG, buffer+length);
  length += logmaskjsonobject(RSSA_LOG, buffer+length);
  length += logmaskjsonobject(DJAN_LOG, buffer+length);
  length += logmaskjsonobject(DPEN_LOG, buffer+length);
  length += logmaskjsonobject(PROG_LOG, buffer+length);
  length += logmaskjsonobject(SCHD_LOG, buffer+length);
  length += logmaskjsonobject(RSTM_LOG, buffer+length);
  length += logmaskjsonobject(SIM_LOG, buffer+length);

  length += logmaskjsonobject(RSSD_LOG, buffer+length); // Make sure the last one.
  // DBGT_LOG is a compile time only, so don;t include
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "]");

  length += sprintf(buffer+length, ",\"loglevels\":[");
  length += logleveljsonobject(LOG_DEBUG_SERIAL, buffer+length);
  length += logleveljsonobject(LOG_DEBUG, buffer+length);
  length += logleveljsonobject(LOG_INFO, buffer+length);
  length += logleveljsonobject(LOG_NOTICE, buffer+length);
  length += logleveljsonobject(LOG_WARNING, buffer+length);
  length += logleveljsonobject(LOG_ERR, buffer+length);
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "]");

  length += sprintf(buffer+length, "}");
  
  return length;
}
int build_aqualink_status_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  //strncpy(buffer, test_message, strlen(test_message)+1);
  //return strlen(test_message);
  //char buffer2[600];
  
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  length += sprintf(buffer+length, "{\"type\": \"status\"");
  length += sprintf(buffer+length, ",\"status\":\"%s\"",getStatus(aqdata) );
  length += sprintf(buffer+length, ",\"panel_message\":\"%s\"",aqdata->last_message );
  length += sprintf(buffer+length, ",\"panel_type_full\":\"%s\"",getPanelString());
  length += sprintf(buffer+length, ",\"panel_type\":\"%s\"",getShortPanelString());
  //length += sprintf(buffer+length, ",\"message\":\"%s\"",aqdata->message );
  length += sprintf(buffer+length, ",\"version\":\"%s\"",aqdata->version );//8157 REV MMM",
  length += sprintf(buffer+length, ",\"aqualinkd_version\":\"%s\"", AQUALINKD_VERSION ); //1.0b,
  length += sprintf(buffer+length, ",\"date\":\"%s\"",aqdata->date );//"09/01/16 THU",
  length += sprintf(buffer+length, ",\"time\":\"%s\"",aqdata->time );//"1:16 PM",
  //length += sprintf(buffer+length, ",\"air_temp\":\"%d\"",aqdata->air_temp );//"96",
  //length += sprintf(buffer+length, ",\"pool_temp\":\"%d\"",aqdata->pool_temp );//"86",
  //length += sprintf(buffer+length, ",\"spa_temp\":\"%d\"",aqdata->spa_temp );//" ",
  
  length += sprintf(buffer+length, ",\"pool_htr_set_pnt\":\"%d\"",aqdata->pool_htr_set_point );//"85",
  length += sprintf(buffer+length, ",\"spa_htr_set_pnt\":\"%d\"",aqdata->spa_htr_set_point );//"99",
  //length += sprintf(buffer+length, ",\"freeze_protection":\"%s\"",aqdata->frz_protect_set_point );//"off",
  length += sprintf(buffer+length, ",\"frz_protect_set_pnt\":\"%d\"",aqdata->frz_protect_set_point );//"0",
  if ( (ENABLE_CHILLER || aqdata->chiller_set_point != TEMP_UNKNOWN) && aqdata->chiller_button != NULL) {
    length += sprintf(buffer+length, ",\"chiller_set_pnt\":\"%d\"",aqdata->chiller_set_point );//"0",
    if (isVBUTTON_CHILLER(aqdata->chiller_button->special_mask))
      length += sprintf(buffer+length, ",\"chiller_mode\":\"%s\"",((vbutton_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode?"cool":"heat");
  }
  
  if ( aqdata->air_temp == TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"air_temp\":\" \"");
  else
    length += sprintf(buffer+length, ",\"air_temp\":\"%d\"",aqdata->air_temp );
  
  if ( aqdata->pool_temp == TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"pool_temp\":\" \"");
  else
    length += sprintf(buffer+length, ",\"pool_temp\":\"%d\"",aqdata->pool_temp );
    
  if ( aqdata->spa_temp == TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"spa_temp\":\" \"");
  else
    length += sprintf(buffer+length, ",\"spa_temp\":\"%d\"",aqdata->spa_temp );

  if (aqdata->swg_led_state != LED_S_UNKNOWN) {
    if ( aqdata->swg_percent != TEMP_UNKNOWN )
      length += sprintf(buffer+length, ",\"swg_percent\":\"%d\"",aqdata->swg_percent );
  
    if ( aqdata->swg_ppm != TEMP_UNKNOWN )
      length += sprintf(buffer+length, ",\"swg_ppm\":\"%d\"",aqdata->swg_ppm );
  }

  if ( aqdata->temp_units == FAHRENHEIT )
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"",JSON_FAHRENHEIT );
  else if ( aqdata->temp_units == CELSIUS )
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"", JSON_CELSIUS);
  else
    length += sprintf(buffer+length, ",\"temp_units\":\"%s\"",JSON_UNKNOWN );
  
  if (aqdata->battery == OK)
    length += sprintf(buffer+length, ",\"battery\":\"%s\"",JSON_OK );//"ok",
  else
    length += sprintf(buffer+length, ",\"battery\":\"%s\"",JSON_LOW );//"ok",

  if ( aqdata->swg_percent == 101 )
    length += sprintf(buffer+length, ",\"swg_boost_msg\":\"%s\"",aqdata->boost_msg );
  
  if ( aqdata->ph != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"chem_ph\":\"%.1f\"",aqdata->ph );
    
  if ( aqdata->orp != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"chem_orp\":\"%d\"",aqdata->orp );

  //if ( READ_RSDEV_SWG )
    length += sprintf(buffer+length, ",\"swg_fullstatus\": \"%d\"", aqdata->ar_swg_device_status);

  length += sprintf(buffer+length, ",\"leds\":{" );
  for (i=0; i < aqdata->total_buttons; i++) 
  {
    char *state = LED2text(aqdata->aqbuttons[i].led->state);
    length += sprintf(buffer+length, "\"%s\": \"%s\"", aqdata->aqbuttons[i].name, state);

    if (i+1 < aqdata->total_buttons)
      length += sprintf(buffer+length, "," );
  }

  if ( aqdata->swg_percent != TEMP_UNKNOWN && aqdata->swg_led_state != LED_S_UNKNOWN ) {
    //length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_TOPIC, LED2text(get_swg_led_state(aqdata)));
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_TOPIC, LED2text(aqdata->swg_led_state));
    //length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_TOPIC, aqdata->ar_swg_status == SWG_STATUS_OFF?JSON_OFF:JSON_ON);
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_BOOST_TOPIC, aqdata->boost?JSON_ON:JSON_OFF);
  }
  //NSF Need to come back and read what the display states when Freeze protection is on
  if ( aqdata->frz_protect_set_point != TEMP_UNKNOWN || ENABLE_FREEZEPROTECT ) {
    //length += sprintf(buffer+length, ", \"%s\": \"%s\"", FREEZE_PROTECT, aqdata->frz_protect_state==ON?JSON_ON:JSON_ENABLED);
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", FREEZE_PROTECT, LED2text(aqdata->frz_protect_state) );
  }
  // Add Chiller if exists
  if (aqdata->chiller_button != NULL) {
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", CHILLER, LED2text(aqdata->chiller_button->led->state) );
  }
  //length += sprintf(buffer+length, "}, \"extra\":{" );
  length += sprintf(buffer+length, "},");

  // NSF Check below needs to be for VSP Pump (any state), not just known state
  for (i=0; i < aqdata->num_pumps; i++) {
    /*  NSF There is a problem here that needs to be fixed.
printf("Loop %d Message '%s'\n",i,buffer);
printf("Pump Label %s\n",aqdata->pumps[i].button->label);
printf("Pump Name %s\n",aqdata->pumps[i].button->name);
printf("Pump RPM %d\n",aqdata->pumps[i].rpm);
printf("Pump GPM %d\n",aqdata->pumps[i].gpm);
printf("Pump GPM %d\n",aqdata->pumps[i].watts);
printf("Pump Type %d\n",aqdata->pumps[i].pumpType);
    */
    //if (aqdata->pumps[i].pumpType != PT_UNKNOWN && (aqdata->pumps[i].rpm != TEMP_UNKNOWN || aqdata->pumps[i].gpm != TEMP_UNKNOWN || aqdata->pumps[i].watts != TEMP_UNKNOWN)) {
    if (aqdata->pumps[i].pumpType != PT_UNKNOWN ) {
      length += sprintf(buffer+length, "\"Pump_%d\":{\"name\":\"%s\",\"id\":\"%s\",\"RPM\":\"%d\",\"GPM\":\"%d\",\"Watts\":\"%d\",\"Pump_Type\":\"%s\",\"Status\":\"%d\"},",
                        i+1,aqdata->pumps[i].button->label,aqdata->pumps[i].button->name,aqdata->pumps[i].rpm,aqdata->pumps[i].gpm,aqdata->pumps[i].watts,
                        (aqdata->pumps[i].pumpType==VFPUMP?"vfPump":(aqdata->pumps[i].pumpType==VSPUMP?"vsPump":"ePump")),
                        getPumpStatus(i, aqdata));
    }
  }
  if (buffer[length-1] == ',')
    length--;

  length += sprintf(buffer+length, ",\"timers\":{" );
  for (i=0; i < aqdata->total_buttons; i++) 
  {
    if ((aqdata->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
      length += sprintf(buffer+length, "\"%s\": \"on\",", aqdata->aqbuttons[i].name);
      //length += sprintf(buffer+length, "\"%s_duration\": \"%d\",", aqdata->aqbuttons[i].name, get_timer_left(&aqdata->aqbuttons[i]) );
    }
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "}");

  length += sprintf(buffer+length, ",\"timer_durations\":{" );
  for (i=0; i < aqdata->total_buttons; i++) 
  {
    if ((aqdata->aqbuttons[i].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE) {
      length += sprintf(buffer+length, "\"%s\": \"%d\",", aqdata->aqbuttons[i].name, get_timer_left(&aqdata->aqbuttons[i]) );
    }
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "}");

  length += sprintf(buffer+length, ",\"light_program_names\":{" );
  for (i=0; i < aqdata->num_lights; i++) 
  {
    if (aqdata->lights[i].lightType == LC_DIMMER2) {
      length += sprintf(buffer+length, "\"%s\": \"%d%%\",", aqdata->lights[i].button->name, aqdata->lights[i].currentValue );
    } else {
      //length += sprintf(buffer+length, "\"%s\": \"%s\",", aqdata->lights[i].button->name, light_mode_name(aqdata->lights[i].lightType, aqdata->lights[i].currentValue, RSSADAPTER) );
      length += sprintf(buffer+length, "\"%s\": \"%s\",", aqdata->lights[i].button->name, get_currentlight_mode_name(aqdata->lights[i], RSSADAPTER) );
    }
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "}");


  length += sprintf(buffer+length, ",\"alternate_modes\":{" );
  for (i=aqdata->virtual_button_start; i < aqdata->total_buttons; i++) 
  {
    if (isVBUTTON_ALTLABEL(aqdata->aqbuttons[i].special_mask)) {
       length += sprintf(buffer+length, "\"%s\": \"%s\",",aqdata->aqbuttons[i].name, ((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->in_alt_mode?JSON_ON:JSON_OFF );
    }
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "}");


  length += sprintf(buffer+length, ",\"sensors\":{" );
  for (i=0; i < aqdata->num_sensors; i++) 
  {
    //printf("Sensor value %f %.2f\n",aqdata->sensors[i].value,aqdata->sensors[i].value);
    if (aqdata->sensors[i].value != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "\"%s\": \"%.2f\",", aqdata->sensors[i].label, aqdata->sensors[i].value );
    }
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "}");


  length += sprintf(buffer+length, "}" );
  
  buffer[length] = '\0';
  
  return strlen(buffer);
}

int build_aux_labels_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;
  
  length += sprintf(buffer+length, "{\"type\": \"aux_labels\"");
  
  for (i=0; i < aqdata->total_buttons; i++) 
  {
    length += sprintf(buffer+length, ",\"%s\": \"%s\"", aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label);
  }
  
  length += sprintf(buffer+length, "}");
  
  return length;
  
//printf("%s\n",buffer);
  
  //return strlen(buffer);
}
/*
const char* emulationtype2name(emulation_type type) {
  switch (type) {
    case ALLBUTTON:
      return "allbutton";
    break;
    case RSSADAPTER:
      return "allbutton";
    break;
    case ONETOUCH:
      return "onetouch";
    break;
    case IAQTOUCH:
      return "iaqualinktouch";
    break;
    case AQUAPDA:
      return "aquapda";
    break;
    case JANDY_DEVICE:
      return "jandydevice";
    break;
    case SIMULATOR:
      return "allbutton";
    break;
    default:
      return "none";
    break;
  } 
}
*/
int build_aqualink_simulator_packet_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  length += sprintf(buffer+length, "{\"type\": \"simpacket\"");

  if (aqdata->simulator_packet[PKT_DEST] >= 0x40 && aqdata->simulator_packet[PKT_DEST] <= 0x43) {
    length += sprintf(buffer+length, ",\"simtype\": \"onetouch\"");
  } else if (aqdata->simulator_packet[PKT_DEST] >= 0x08 && aqdata->simulator_packet[PKT_DEST] <= 0x0a) {
    length += sprintf(buffer+length, ",\"simtype\": \"allbutton\"");
  } else if (aqdata->simulator_packet[PKT_DEST] >= 0x30 && aqdata->simulator_packet[PKT_DEST] <= 0x33) {
    length += sprintf(buffer+length, ",\"simtype\": \"iaqtouch\"");
  } else if (aqdata->simulator_packet[PKT_DEST] >= 0x60 && aqdata->simulator_packet[PKT_DEST] <= 0x63) {
    length += sprintf(buffer+length, ",\"simtype\": \"aquapda\"");
  } else {
    length += sprintf(buffer+length, ",\"simtype\": \"unknown\"");
  }
  //if (aqdata->simulator_packet[i][])
  //length += sprintf(buffer+length, ",\"simtype\": \"onetouch\"");

  length += sprintf(buffer+length, ",\"raw\": [");
  for (i=0; i < aqdata->simulator_packet_length; i++) 
  {
    length += sprintf(buffer+length, "\"0x%02hhx\",", aqdata->simulator_packet[i]);
  }
  if (buffer[length-1] == ',')
    length--;
  length += sprintf(buffer+length, "]");
  
  length += sprintf(buffer+length, ",\"dec\": [");
  for (i=0; i < aqdata->simulator_packet_length; i++) 
  {
    length += sprintf(buffer+length, "%d,", aqdata->simulator_packet[i]);
  }
  if (buffer[length-1] == ',')
    length--;

  length += sprintf(buffer+length, "]");

  length += sprintf(buffer+length, "}");

//printf("Buffer=%d, used=%d, OUT='%s'\n",size,length,buffer);

  return length;
}

// WS Received '{"parameter":"SPA_HTR","value":99}'
// WS Received '{"command":"KEY_HTR_POOL"}'
// WS Received '{"command":"GET_AUX_LABELS"}'

bool parseJSONrequest(char *buffer, struct JSONkvptr *request)
{
  int i=0;
  int found=0;
  bool reading = false;

  request->kv[0].key   = NULL;
  request->kv[0].value = NULL;
  request->kv[1].key   = NULL;
  request->kv[1].value = NULL;
  request->kv[2].key   = NULL;
  request->kv[2].value = NULL;
  request->kv[3].key   = NULL;
  request->kv[3].value = NULL;

  int length = strlen(buffer);

  while ( i < length )
  {
//printf ("Reading %c",buffer[i]);
    switch (buffer[i]) {
    case '{':
    case '"':
    case '}':
    case ':':
    case ',':
    case ' ':
      // Ignore space , : if reading a string
      if (reading == true && buffer[i] != ' ' && buffer[i] != ',' && buffer[i] != ':'){
 //printf (" <-  END");
        reading = false;
        buffer[i] = '\0';
        found++;
      }
      break;
      
    default:
      if (reading == false) {
//printf (" <-  START");
        reading = true;
        if ( found%2 == 0 )
          request->kv[found / 2].key = &buffer[i];
        else
          request->kv[(found-1) / 2].value = &buffer[i];
      }
      break;
    }
//printf ("\n");
    if (found >= 8)
    break;
    
    i++;
  }
  
  return true;
}

bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request)
{
  int i=0;
  int found=0;
  bool reading = false;

  request->first.key    = NULL;
  request->first.value  = NULL;
  request->second.key   = NULL;
  request->second.value = NULL;
  request->third.key   = NULL;
  request->third.value = NULL;

  int length = strlen(buffer);

  while ( i < length )
  {
//printf ("Reading %c",buffer[i]);
    switch (buffer[i]) {
    case '{':
    case '"':
    case '}':
    case ':':
    case ',':
    case ' ':
      // Ignore space , : if reading a string
      if (reading == true && buffer[i] != ' ' && buffer[i] != ',' && buffer[i] != ':'){
 //printf (" <-  END");
        reading = false;
        buffer[i] = '\0';
        found++;
      }
      break;
      
    default:
      if (reading == false) {
//printf (" <-  START");
        reading = true;
        switch(found) {
        case 0:
          request->first.key = &buffer[i];
          break;
        case 1:
          request->first.value = &buffer[i];
          break;
        case 2:
          request->second.key = &buffer[i];
          break;
        case 3:
          request->second.value = &buffer[i];
          break;
        case 4:
          request->third.key = &buffer[i];
          break;
        case 5:
          request->third.value = &buffer[i];
          break;
        }
      }
      break;
    }
//printf ("\n");
//    if (found >= 4)
    if (found >= 6)
    break;
    
    i++;
  }
  
  return true;
}

bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue) {
  unsigned int i = 0;
  int found = 0;
  
  svalue[0] = '\0';

  for (i = 0; i < len && str[i] != '\0'; i++) {
    if (str[i] == '"') {
      if (strncmp("\"idx\"", (char *)&str[i], 5) == 0) {
        i = i + 5;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            *idx = atoi(&str[i + 1]);
            found++;
          }
        }
        //if (*idx == 45) 
        //  printf("%s\n",str);
      } else if (strncmp("\"nvalue\"", (char *)&str[i], 8) == 0) {
        i = i + 8;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            *nvalue = atoi(&str[i + 1]);
            found++;
          }
        }
      } else if (strncmp("\"svalue1\"", (char *)&str[i], 9) == 0) {
        i = i + 9;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            while(str[i] == ':' || str[i] == ' ' || str[i] == '"' || str[i] == '\'') i++;
            int j=i+1;
            while(str[j] != '"' && str[j] != '\'' && str[j] != ',' && str[j] != '}') j++;
            strncpy(svalue, &str[i], ((j-i)>DZ_SVALUE_LEN?DZ_SVALUE_LEN:(j-i)));
            svalue[((j-i)>DZ_SVALUE_LEN?DZ_SVALUE_LEN:(j-i))] = '\0'; // Simply force the last termination
            found++;
          }
        }
      } 
      if (found >= 4) {
        return true;
      }
    }
  }
  // Just incase svalue is not found, we really don;t care for most devices.
  if (found >= 2) {
    return true;
  }
  return false;
}

/*
int json_cfg_element_OLD(char* buffer, int size, const char *name, const void *value, cfg_value_type type, char *valid_val) {
  int result = 0;

  char valid_values[256];

  if (valid_val != NULL) {
    sprintf(valid_values,",\"valid values\":%s",valid_val);
  }

  switch(type){
    case CFG_INT:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%d\", \"type\":\"int\" %s}", name, *(int *)value, (valid_val==NULL?"":valid_values) );
    break;
    case CFG_STRING:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"string\" %s}", name, (char *)value, (valid_val==NULL?"":valid_values) );
    break;
    case CFG_BOOL:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"bool\" %s}", name, bool2text(*(bool *)value), (valid_val==NULL?"":valid_values));
    break;
    case CFG_HEX:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"0x%02hhx\", \"type\":\"hex\" %s}", name, *(unsigned char *)value, (valid_val==NULL?"":valid_values) );
    break;
  }

  if (result <= 0 || result >= size) {
    LOG(NET_LOG,LOG_ERR, "Buffer full in build_aqualink_config_JSON(), result truncated!");
    return 0;
  }

  return result;
}
*/

//#ifdef CONFIG_EDITOR

int json_cfg_element(char* buffer, int size, const char *name, const void *value, cfg_value_type type, uint8_t mask, char *valid_val, uint8_t config_mask) {
  int result = 0;

  char valid_values[256];
  char adv[128];
  int adv_size=0;

  // We shouldn't get CFG_HIDE here. Since we can't exit with 0, simply add a space
  if (isMASKSET(config_mask, CFG_HIDE)) {
    return snprintf(buffer, size, " ");
  }

  if (valid_val != NULL) {
    sprintf(valid_values,",\"valid values\":%s",valid_val);
  }

  adv_size = sprintf(adv,",\"advanced\": \"%s\"", isMASKSET(config_mask, CFG_GRP_ADVANCED)?"yes":"no");
  
  if (isMASKSET(config_mask, CFG_READONLY))
    adv_size += sprintf(adv+adv_size,",\"readonly\": \"yes\"");

  if (isMASKSET(config_mask, CFG_FORCE_RESTART))
    adv_size += sprintf(adv+adv_size,",\"force_restart\": \"yes\"");
  

  switch(type){
    case CFG_INT:
      if (*(int *)value == AQ_UNKNOWN) { 
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"\", \"type\":\"int\" %s %s}", name, (valid_val==NULL?"":valid_values),adv );
      } else {
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%d\", \"type\":\"int\" %s %s}", name, *(int *)value, (valid_val==NULL?"":valid_values),adv );
      }
    break;
    case CFG_STRING:
      if (*(char **)value == NULL) {
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"\", \"type\":\"string\" %s %s}", name, (valid_val==NULL?"":valid_values),adv );
      } else {
        if (isMASK_SET(config_mask, CFG_PASSWD_MASK)) {
          result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"string\", \"passwd_mask\":\"yes\" %s}", name, PASSWD_MASK_TEXT,adv);
        } else {
          result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"string\" %s %s}", name, *(char **)value, (valid_val==NULL?"":valid_values),adv );
        }
      }
    break;
    case CFG_BOOL:
      //result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"bool\" %s}", name, bool2text(*(bool *)value), (valid_val==NULL?"":valid_values));
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"bool\", \"valid values\": %s %s}", name, bool2text(*(bool *)value), CFG_V_BOOL, adv);
    break;
    case CFG_HEX:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"0x%02hhx\", \"type\":\"hex\" %s %s}", name, *(unsigned char *)value, (valid_val==NULL?"":valid_values), adv );
    break;
    case CFG_FLOAT:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%f\", \"type\":\"float\" %s %s}", name, *(float *)value, (valid_val==NULL?"":valid_values), adv );
    break;
    case CFG_BITMASK:
      result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"bool\", \"valid values\": %s %s}", name, (*(uint8_t *)value & mask) == mask? bool2text(true):bool2text(false) ,CFG_V_BOOL, adv );
    break;
    case CFG_SPECIAL:
      if (strncasecmp(name, CFG_N_log_level, strlen(CFG_N_log_level)) == 0) {
        //fprintf(fp, "%s=%s\n", _cfgParams[i].name, loglevel2cgn_name(_aqconfig_.log_level));
        //result = json_cfg_element(buffer+length, size-length, CFG_N_log_level, &stringptr, CFG_STRING, "[\"DEBUG\", \"INFO\", \"NOTICE\", \"WARNING\", \"ERROR\"]"));
        //stringptr = loglevel2cgn_name(_aqconfig_.log_level);
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"string\" %s %s}",name,loglevel2cgn_name(*(int *)value), ",\"valid values\":[\"DEBUG\", \"INFO\", \"NOTICE\", \"WARNING\", \"ERROR\"]", adv);
      } else if (strncasecmp(name, CFG_N_panel_type, strlen(CFG_N_panel_type)) == 0) {
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"%s\", \"type\":\"string\" %s}",name,getShortPanelString(), adv);
      } else {
        result = snprintf(buffer, size, ",\"%s\" : {\"value\":\"Something went wrong\", \"type\":\"string\"}",name);
      }
    break;
  }

  if (result <= 0 || result >= size) {
    LOG(NET_LOG,LOG_ERR, "Buffer full, result truncated! size left=%d needed=%d @ element %s\n",size,result,name);     
    return 0;
  }

  return result;
}



/*
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
*/

int build_aqualink_config_JSON(char* buffer, int size, struct aqualinkdata *aq_data)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int result;
  int i;
  char buf[256];
  char buf1[256];
  const char *stringptr;
  int delectCharAt = 0;

  if ((result = snprintf(buffer+length, size-length, "{\"type\": \"config\"")) < 0 || result >= size-length) {
    length += snprintf(buffer+length, size-length, "}");
    return length;
  } else {
    length += result;
  }
/*
  if ((result = snprintf(buffer+length, size-length, ",\"status\": \"!!!! NOT FULLY IMPLIMENTED YET !!!!\"")) < 0 || result >= size-length) {
    length += snprintf(buffer+length, size-length, "}");
    return length;
  } else {
    length += result;
  }
*/
  if ((result = snprintf(buffer+length, size-length, ",\"max_pumps\": \"%d\",\"max_lights\": \"%d\",\"max_sensors\": \"%d\",\"max_light_programs\": \"%d\",\"max_vbuttons\": \"%d\"",
                         MAX_PUMPS,MAX_LIGHTS,MAX_SENSORS,LIGHT_COLOR_OPTIONS-1, (TOTAL_BUTTONS - aq_data->virtual_button_start) )) < 0 || result >= size-length) {
    length += snprintf(buffer+length, size-length, "}");
    return length;
  } else {
    length += result;
  }


  //#ifdef CONFIG_DEV_TEST
  for (int i=0; i <= _numCfgParams; i++) {
    if (isMASK_SET(_cfgParams[i].config_mask, CFG_HIDE) ) {
      continue;
    }
    // We can't change web_directory or port while running, so don;t even chow those options.
    // mongoose holds a pointer to the string web_directoy, so can;t change that easily while running
    // web port = well derr we are using that currently
    /*
    if ( strncasecmp(_cfgParams[i].name, CFG_N_socket_port, strlen(CFG_N_socket_port)) == 0 || 
         strncasecmp(_cfgParams[i].name, CFG_N_web_directory, strlen(CFG_N_web_directory)) == 0 ) {
      continue;
    }
    */
    if (isMASK_SET(_cfgParams[i].config_mask, CFG_READONLY) ) {
      // NSF in the future we should allow these to pass, but set the UI as readonly.
      continue;
    }

    if ((result = json_cfg_element(buffer+length, size-length, _cfgParams[i].name, _cfgParams[i].value_ptr, _cfgParams[i].value_type, _cfgParams[i].mask, _cfgParams[i].valid_values, _cfgParams[i].config_mask)) <= 0) {
      LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
      return length;
    } else {
      length += result;
    }
  }

  for (i = 1; i <= aq_data->num_sensors; i++)
  {
    length += sprintf(buffer+length, ",\"sensor_%.2d\":{ \"advanced\":\"yes\",",i );
    // The next json_cfg_element() call will add a , at the beginning, so save the next char index so we can delete it later.
    delectCharAt = length;

    //fprintf(fp,"\nsensor_%.2d_path=%s\n",i+1,aqdata->sensors->path);
    //fprintf(fp,"sensor_%.2d_label=%s\n",i+1,aqdata->sensors->label);
    //fprintf(fp,"sensor_%.2d_factor=%f\n",i+1,aqdata->sensors->factor);
    sprintf(buf,"sensor_%.2d_path", i);
    if ((result = json_cfg_element(buffer+length, size-length, buf, &aq_data->sensors[i-1].path, CFG_STRING, 0, NULL, CFG_GRP_ADVANCED)) <= 0)
      return length;
    else
      length += result;
  
    sprintf(buf,"sensor_%.2d_label", i);
    if ((result = json_cfg_element(buffer+length, size-length, buf, &aq_data->sensors[i-1].label, CFG_STRING, 0, NULL, CFG_GRP_ADVANCED)) <= 0)
      return length;
    else
      length += result;

    sprintf(buf,"sensor_%.2d_factor", i);
    if ((result = json_cfg_element(buffer+length, size-length, buf, &aq_data->sensors[i-1].factor, CFG_FLOAT, 0, NULL, CFG_GRP_ADVANCED)) <= 0)
      return length;
    else
      length += result;

    length += sprintf(buffer+length, "}" );
    if (delectCharAt != 0) {
      buffer[delectCharAt] = ' ';
      delectCharAt = 0;
    }
  }

  //  add custom light modes/colors
  bool isShow;
  const char *lname;
  const char *bufptr = buf1;
  for (i=1; i < LIGHT_COLOR_OPTIONS; i++) {
    if ((lname = get_aqualinkd_light_mode_name(i, &isShow)) != NULL) {
      //fprintf(fp,"light_program_%.2d=%s%s\n",i,lname,isShow?" - show":"");
      sprintf(buf,"light_program_%.2d", i);
      sprintf(buf1,"%s%s",lname,isShow?" - show":"");
      //printf("%s %s\n",buf,buf1);
      if ((result = json_cfg_element(buffer+length, size-length, buf, &bufptr, CFG_STRING, 0, NULL, CFG_GRP_ADVANCED)) <= 0)
        return length;
      else
        length += result;
      
    } else {
      break;
    }
  }
  
  // All buttons

  for (i = 0; i < aq_data->total_buttons; i++)
  {
    char prefix[30];
    if (isVBUTTON(aq_data->aqbuttons[i].special_mask)) {
      sprintf(prefix,"virtual_button_%.2d",(i+1)-aq_data->virtual_button_start);
    } else {
      sprintf(prefix,"button_%.2d",i+1);
    }

    //length += sprintf(buffer+length, ",\"%s\":{",prefix );
    length += sprintf(buffer+length, ",\"%s\":{ \"default\":\"%s\", ",prefix, aq_data->aqbuttons[i].name );
    // The next json_cfg_element() call will add a , at the beginning, so save the next char index so we can delete it later.
    delectCharAt = length;
    
    sprintf(buf,"%s_label", prefix);
    if ((result = json_cfg_element(buffer+length, size-length, buf, &aq_data->aqbuttons[i].label, CFG_STRING, 0, NULL, 0)) <= 0) {
      LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
      return length;
    } else
      length += result;

    if (isVS_PUMP(aq_data->aqbuttons[i].special_mask)) 
    {
      if (((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpIndex > 0) {
        sprintf(buf,"%s_pumpIndex", prefix);
        if ((result = json_cfg_element(buffer+length, size-length, buf, &((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpIndex, CFG_INT, 0, NULL, 0)) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
      }
      
      if (((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpID != NUL) {
        sprintf(buf,"%s_pumpID", prefix);
        if ((result = json_cfg_element(buffer+length, size-length, buf, &((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpID, CFG_HEX, 0, NULL, 0)) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
      }

      if (((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpName[0] != '\0') {
        sprintf(buf,"%s_pumpName", prefix);
        stringptr = ((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpName;
        if ((result = json_cfg_element(buffer+length, size-length, buf, &stringptr, CFG_STRING, 0, NULL, 0)) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
      }

      if (((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpType != PT_UNKNOWN) {
        sprintf(buf,"%s_pumpType", prefix);
        stringptr = pumpType2String(((pump_detail *)aq_data->aqbuttons[i].special_mask_ptr)->pumpType);
        if ((result = json_cfg_element(buffer+length, size-length, buf, &stringptr, CFG_STRING, 0, "[\"\", \"JANDY ePUMP\",\"Pentair VS\",\"Pentair VF\"]", 0) ) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
      }
    } else if (isPLIGHT(aq_data->aqbuttons[i].special_mask)) {
      if (((clight_detail *)aq_data->aqbuttons[i].special_mask_ptr)->lightType > 0) {
        sprintf(buf,"%s_lightMode", prefix);
        if ((result = json_cfg_element(buffer+length, size-length, buf, &((clight_detail *)aq_data->aqbuttons[i].special_mask_ptr)->lightType, CFG_INT, 0, NULL, 0)) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
      }
    } else if ( (isVBUTTON(aq_data->aqbuttons[i].special_mask) && aq_data->aqbuttons[i].rssd_code >= IAQ_ONETOUCH_1 && aq_data->aqbuttons[i].rssd_code <= IAQ_ONETOUCH_6 ) ) {
        sprintf(buf,"%s_onetouchID", prefix);
        int oID = (aq_data->aqbuttons[i].rssd_code - 15);
        if ((result = json_cfg_element(buffer+length, size-length, buf, &oID, CFG_INT, 0, "[\"\", \"1\",\"2\",\"3\",\"4\",\"5\",\"6\"]", 0)) <= 0) {
          LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
          return length;
        } else
          length += result;
    } else if ( isVBUTTON_ALTLABEL(aq_data->aqbuttons[i].special_mask)) {
      sprintf(buf,"%s_altlabel", prefix);
      if ((result = json_cfg_element(buffer+length, size-length, buf, &((vbutton_detail *)aq_data->aqbuttons[i].special_mask_ptr)->altlabel, CFG_STRING, 0, NULL, 0)) <= 0) {
        LOG(NET_LOG,LOG_ERR, "Config json buffer full in, result truncated! size=%d curently used=%d\n",size,length);
        return length;
      } else
        length += result;
    }

    length += sprintf(buffer+length, "}" );
    if (delectCharAt != 0) {
      buffer[delectCharAt] = ' ';
      delectCharAt = 0;
    }
    
  }

  // Need to add one last element, can be crap. Makes the HTML/JS easier in the loop
  if ((result = snprintf(buffer+length, size-length, ",\"version\": \"1.0\"")) < 0 || result >= size-length) {
    length += snprintf(buffer+length, size-length, "}");
    return length;
  } else {
    length += result;
  }


  if ((result = snprintf(buffer+length, size-length, "}")) < 0) {
    return 0;
  } else {
    length += result;
  }


  return length;
}


//#endif