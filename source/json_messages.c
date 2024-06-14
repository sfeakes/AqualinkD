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

//#define test_message "{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}"
//#define test_labels "{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}"

//#define test_message "{\"type\": \"status\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"on\",\"Spa_Mode\": \"on\",\"Aux_1\": \"on\",\"Aux_2\": \"on\",\"Aux_3\": \"on\",\"Aux_4\": \"on\",\"Aux_5\": \"on\",\"Aux_6\": \"on\",\"Aux_7\": \"on\",\"Pool_Heater\": \"on\",\"Spa_Heater\": \"on\",\"Solar_Heater\": \"on\"}}"

//{"type": "aux_labels","Pool Pump": "Pool Pump","Spa Mode": "Spa Mode","Cleaner": "Aux 1","Waterfall": "Aux 2","Spa Blower": "Aux 2","Pool Light": "Aux 4","Spa Light ": "Aux 5","Aux 6": "Aux 6","Aux 7": "Aux 7","Heater": "Heater","Heater": "Heater","Solar Heater": "Solar Heater","(null)": "(null)"}

//SPA WILL TURN OFF AFTER COOL DOWN CYCLE




int json_chars(char *dest, const char *src, int dest_len, int src_len)
{
  int i;
  int end = dest_len < src_len ? dest_len:src_len;
  for(i=0; i < end; i++) {
    if ( (src[i] < 32 || src[i] > 126) || 
          src[i] == 123 || // {
          src[i] == 125 || // }
          src[i] == 34 || // "
          src[i] == 92 // backslash
       ) // only printable chars
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

  // NSF should probably use json_chars here.
  if (aqdata->last_display_message[0] != '\0') {
    int i;
    for(i=0; i < strlen(aqdata->last_display_message); i++ ) {
      if (aqdata->last_display_message[i] <= 31 || aqdata->last_display_message[i] >= 127) {
        aqdata->last_display_message[i] = ' ';
      } else {
        switch (aqdata->last_display_message[i]) {
          case '"':
          case '/':
          case '\n':
          case '\t':
          case '\f':
          case '\r':
          case '\b':
            aqdata->last_display_message[i] = ' ';
          break;
        }
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

int build_aqualink_error_status_JSON(char* buffer, int size, char *msg)
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

  if ((button->special_mask & VS_PUMP) == VS_PUMP)
  {
//printf("Button %s is VSP\n", button->name);
    for (i=0; i < aqdata->num_pumps; i++) {
      if (button == aqdata->pumps[i].button) {       
          length += sprintf(buffer, ",\"type_ext\":\"switch_vsp\",\"Pump_RPM\":\"%d\",\"Pump_GPM\":\"%d\",\"Pump_Watts\":\"%d\",\"Pump_Type\":\"%s\"", 
                  aqdata->pumps[i].rpm,aqdata->pumps[i].gpm,aqdata->pumps[i].watts,
                  (aqdata->pumps[i].pumpType==VFPUMP?"vfPump":(aqdata->pumps[i].pumpType==VSPUMP?"vsPump":"ePump")));

          return buffer;
      }
    }
  } 
  else if ((button->special_mask & PROGRAM_LIGHT) == PROGRAM_LIGHT)
  {
//printf("Button %s is ProgramableLight\n", button->name);
    for (i=0; i < aqdata->num_lights; i++) {
      if (button == aqdata->lights[i].button) {
        length += sprintf(buffer, ",\"type_ext\": \"switch_program\", \"Light_Type\":\"%d\"", aqdata->lights[i].lightType);
        return buffer;
      }
    }
  }

//printf("Button %s is Switch\n", button->name);
  length += sprintf(buffer, ",\"type_ext\": \"switch_timer\", \"timer_active\":\"%s\"", (((button->special_mask & TIMER_ACTIVE) == TIMER_ACTIVE)?JSON_ON:JSON_OFF) );
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
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0 && (_aqconfig_.force_ps_setpoints || aqdata->pool_htr_set_point != TEMP_UNKNOWN)) {
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

    } else if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name)==0 && (_aqconfig_.force_ps_setpoints || aqdata->spa_htr_set_point != TEMP_UNKNOWN)) {
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

  if ( _aqconfig_.force_frzprotect_setpoints || (aqdata->frz_protect_set_point != TEMP_UNKNOWN && aqdata->air_temp != TEMP_UNKNOWN) ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_freeze\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     FREEZE_PROTECT,
                                    "Freeze Protection",
                                    //JSON_OFF,
                                    aqdata->frz_protect_state==ON?JSON_ON:JSON_OFF,
                                    //JSON_ENABLED,
                                    aqdata->frz_protect_state==ON?LED2text(ON):LED2text(ENABLE),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->frz_protect_set_point):aqdata->frz_protect_set_point),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->air_temp):aqdata->air_temp),
                                    aqdata->frz_protect_state==ON?1:0);
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
  length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" }",
                                   SPA_TEMP_TOPIC,
                                   /*SPA_TEMPERATURE,*/
                                   "Spa Water Temperature",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->spa_temp):aqdata->spa_temp));

/*
  length += sprintf(buffer+length,  "], \"aux_device_detail\": [");
  for (i=0; i < MAX_PUMPS; i++) {
  }
*/
  length += sprintf(buffer+length, "]}");

  LOG(NET_LOG,LOG_DEBUG, "JSON: %s used %d of %d\n", homekit?"homebridge":"web", length, size);

  buffer[length] = '\0';

  return strlen(buffer);
  
  //return length;
}

int logmaskjsonobject(int16_t flag, char* buffer)
{
  int length = sprintf(buffer, "{\"name\":\"%s\",\"id\":\"%d\",\"set\":\"%s\"},", logmask2name(flag), flag,(isDebugLogMaskSet(flag)?JSON_ON:JSON_OFF));
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
  length += logmaskjsonobject(RSSD_LOG, buffer+length);
  length += logmaskjsonobject(PROG_LOG, buffer+length);
  length += logmaskjsonobject(SCHD_LOG, buffer+length);
  length += logmaskjsonobject(RSTM_LOG, buffer+length);
  length += logmaskjsonobject(SIM_LOG, buffer+length);
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
  length += sprintf(buffer+length, ",\"panel_type\":\"%s\"",getPanelString());
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
  if ( aqdata->frz_protect_set_point != TEMP_UNKNOWN || _aqconfig_.force_frzprotect_setpoints ) {
    //length += sprintf(buffer+length, ", \"%s\": \"%s\"", FREEZE_PROTECT, aqdata->frz_protect_state==ON?JSON_ON:JSON_ENABLED);
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", FREEZE_PROTECT, LED2text(aqdata->frz_protect_state) );
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
    if (aqdata->pumps[i].pumpType != PT_UNKNOWN && (aqdata->pumps[i].rpm != TEMP_UNKNOWN || aqdata->pumps[i].gpm != TEMP_UNKNOWN || aqdata->pumps[i].watts != TEMP_UNKNOWN)) {
      length += sprintf(buffer+length, "\"Pump_%d\":{\"name\":\"%s\",\"id\":\"%s\",\"RPM\":\"%d\",\"GPM\":\"%d\",\"Watts\":\"%d\",\"Pump_Type\":\"%s\"},",
                        i+1,aqdata->pumps[i].button->label,aqdata->pumps[i].button->name,aqdata->pumps[i].rpm,aqdata->pumps[i].gpm,aqdata->pumps[i].watts,
                        (aqdata->pumps[i].pumpType==VFPUMP?"vfPump":(aqdata->pumps[i].pumpType==VSPUMP?"vsPump":"ePump")));
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


