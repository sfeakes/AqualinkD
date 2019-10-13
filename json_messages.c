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
#include "version.h"


//#define test_message "{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}"
//#define test_labels "{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}"

//#define test_message "{\"type\": \"status\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"on\",\"Spa_Mode\": \"on\",\"Aux_1\": \"on\",\"Aux_2\": \"on\",\"Aux_3\": \"on\",\"Aux_4\": \"on\",\"Aux_5\": \"on\",\"Aux_6\": \"on\",\"Aux_7\": \"on\",\"Pool_Heater\": \"on\",\"Spa_Heater\": \"on\",\"Solar_Heater\": \"on\"}}"

//{"type": "aux_labels","Pool Pump": "Pool Pump","Spa Mode": "Spa Mode","Cleaner": "Aux 1","Waterfall": "Aux 2","Spa Blower": "Aux 2","Pool Light": "Aux 4","Spa Light ": "Aux 5","Aux 6": "Aux 6","Aux 7": "Aux 7","Heater": "Heater","Heater": "Heater","Solar Heater": "Solar Heater","(null)": "(null)"}

//SPA WILL TURN OFF AFTER COOL DOWN CYCLE

const char* getStatus(struct aqualinkdata *aqdata)
{
  if (aqdata->active_thread.thread_id != 0 && !aqdata->simulate_panel) {
    return JSON_PROGRAMMING;
  }
 
  if (aqdata->last_message != NULL && stristr(aqdata->last_message, "SERVICE") != NULL ) {
    return JSON_SERVICE;
  }

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
  return JSON_READY; 
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

#define AUX_BUFFER_SIZE 100

char *get_aux_information(aqkey *button, struct aqualinkdata *aqdata, char *buffer)
{
  int i;
  int length = 0;
  buffer[0] = '\0';

  for (i=0; i < MAX_PUMPS; i++) {
    if (button == aqdata->pumps[i].button) {
      if (aqdata->pumps[i].rpm != TEMP_UNKNOWN || aqdata->pumps[i].gph != TEMP_UNKNOWN || aqdata->pumps[i].watts != TEMP_UNKNOWN) {
        length += sprintf(buffer, ",\"Pump_RPM\":\"%d\",\"Pump_GPH\":\"%d\",\"Pump_Watts\":\"%d\"", aqdata->pumps[i].rpm,aqdata->pumps[i].gph,aqdata->pumps[i].watts);
        break;
      }
    }
  }

  return buffer;
}

int build_device_JSON(struct aqualinkdata *aqdata, int programable_switch1, int programable_switch2, char* buffer, int size, bool homekit)
{
  char aux_info[AUX_BUFFER_SIZE];
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  // IF temp units are F assume homekit is using F
  bool homekit_f = (homekit && aqdata->temp_units==FAHRENHEIT);

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
  
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0 && aqdata->pool_htr_set_point != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_thermo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->pool_htr_set_point):aqdata->pool_htr_set_point),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->pool_temp):aqdata->pool_temp),
                                     LED2int(aqdata->aqbuttons[i].led->state));
    } else if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name)==0 && aqdata->spa_htr_set_point != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_thermo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->spa_htr_set_point):aqdata->spa_htr_set_point),
                                     ((homekit)?2:0),
                                     ((homekit_f)?degFtoC(aqdata->spa_temp):aqdata->spa_temp),
                                     LED2int(aqdata->aqbuttons[i].led->state));
    } else if ( (programable_switch1 > 0 && programable_switch1 == i) || 
                (programable_switch2 > 0 && programable_switch2 == i)) {
      length += sprintf(buffer+length, "{\"type\": \"switch_program\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\" %s},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     get_aux_information(&aqdata->aqbuttons[i], aqdata, aux_info));
    } else {
      length += sprintf(buffer+length, "{\"type\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\" %s},", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?JSON_ON:JSON_OFF,
                                     LED2text(aqdata->aqbuttons[i].led->state),
                                     LED2int(aqdata->aqbuttons[i].led->state),
                                     get_aux_information(&aqdata->aqbuttons[i], aqdata, aux_info));
    }
  }

  if ( aqdata->frz_protect_set_point != TEMP_UNKNOWN && aqdata->air_temp != TEMP_UNKNOWN) {
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

  if ( aqdata->swg_percent != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_swg\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"int_status\": \"%d\" },",
                                     SWG_TOPIC,
                                    "Salt Water Generator",
                                    aqdata->ar_swg_status == SWG_STATUS_OFF?JSON_OFF:JSON_ON,
                                    aqdata->ar_swg_status == SWG_STATUS_OFF?JSON_OFF:JSON_ON,
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent),
                                    aqdata->ar_swg_status == SWG_STATUS_OFF?LED2int(OFF):LED2int(ON));

    //length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
    length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?SWG_PERCENT_F_TOPIC:SWG_PERCENT_TOPIC),
                                   "Salt Water Generator Percent",
                                   "on",
                                   ((homekit_f)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->swg_percent):aqdata->swg_percent));
    if (!homekit) { // For the moment keep boost off homekit   
      length += sprintf(buffer+length, "{\"type\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"int_status\": \"%d\"},", 
                                     SWG_BOOST_TOPIC, 
                                     "SWG Boost",
                                     aqdata->boost?JSON_ON:JSON_OFF,
                                     aqdata->boost?JSON_ON:JSON_OFF,
                                     aqdata->boost?LED2int(ON):LED2int(OFF));
    }
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
                                   ((homekit_f)?degFtoC(aqdata->air_temp):aqdata->pool_temp));
  length += sprintf(buffer+length, "{\"type\": \"temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" }",
                                   SPA_TEMP_TOPIC,
                                   /*SPA_TEMPERATURE,*/
                                   "Spa Water Temperature",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->air_temp):aqdata->spa_temp));

/*
  length += sprintf(buffer+length,  "], \"aux_device_detail\": [");
  for (i=0; i < MAX_PUMPS; i++) {
  }
*/
  length += sprintf(buffer+length, "]}");

  logMessage(LOG_DEBUG, "WEB: homebridge used %d of %d", length, size);

  buffer[length] = '\0';

  return strlen(buffer);
  
  //return length;
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

  if ( aqdata->swg_percent != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"swg_percent\":\"%d\"",aqdata->swg_percent );
  
  if ( aqdata->swg_ppm != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"swg_ppm\":\"%d\"",aqdata->swg_ppm );

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
  
  length += sprintf(buffer+length, ",\"leds\":{" );
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    char *state = JSON_OFF;
    switch (aqdata->aqbuttons[i].led->state)
    {
      case ON:
        state = JSON_ON;
      break;
      case OFF:
      case LED_S_UNKNOWN:
        state = JSON_OFF;
      break;
      case FLASH:
        state = JSON_FLASH;
      break;
      case ENABLE:
        state = JSON_ENABLED;
      break;
    }    
    length += sprintf(buffer+length, "\"%s\": \"%s\"", aqdata->aqbuttons[i].name, state);
    
    if (i+1 < TOTAL_BUTTONS)
      length += sprintf(buffer+length, "," );
  }

  if ( aqdata->swg_percent != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_TOPIC, aqdata->ar_swg_status == SWG_STATUS_OFF?JSON_OFF:JSON_ON);
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", SWG_BOOST_TOPIC, aqdata->boost?JSON_ON:JSON_OFF);
  }
  //NSF Need to come back and read what the display states when Freeze protection is on
  if ( aqdata->frz_protect_set_point != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, ", \"%s\": \"%s\"", FREEZE_PROTECT, aqdata->frz_protect_state==ON?JSON_ON:JSON_ENABLED);
  }

  //length += sprintf(buffer+length, "}, \"extra\":{" );
  length += sprintf(buffer+length, "},");

  for (i=0; i < MAX_PUMPS; i++) {
    if (aqdata->pumps[i].rpm != TEMP_UNKNOWN || aqdata->pumps[i].gph != TEMP_UNKNOWN || aqdata->pumps[i].watts != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "\"Pump_%d\":{\"name\":\"%s\",\"id\":\"%s\",\"RPM\":\"%d\",\"GPH\":\"%d\",\"Watts\":\"%d\"},",
                        i+1,aqdata->pumps[i].button->label,aqdata->pumps[i].button->name,aqdata->pumps[i].rpm,aqdata->pumps[i].gph,aqdata->pumps[i].watts);
    }
  }

  if (buffer[length-1] == ',')
    length--;

  length += sprintf(buffer+length, "}" );
  
  buffer[length] = '\0';
 
 /*
  buffer[length] = '\0';
  
  strncpy(buffer2, test_message, strlen(test_message)+1);
   
  for (i=0; i < strlen(buffer); i++) {
    logMessage (LOG_DEBUG, "buffer[%d] = '%c' | '%c'\n",i,buffer[i],buffer2[i]);
  }
  
  logMessage (LOG_DEBUG, "JSON Size %d\n",strlen(buffer));
  printf("%s\n",buffer);
  for (i=strlen(buffer); i > strlen(buffer)-10; i--) {
    logMessage (LOG_DEBUG, "buffer[%d] = '%c'\n",i,buffer[i]);
  }
  for (i=10; i >= 0; i--) {
    logMessage (LOG_DEBUG, "buffer[%d] = '%c'\n",i,buffer[i]);
  }
  
  //return length-1;
  
 
  logMessage (LOG_DEBUG, "JSON Size %d\n",strlen(buffer2));
  printf("%s\n",buffer2);
  for (i=strlen(buffer2); i > strlen(buffer2)-10; i--) {
    logMessage (LOG_DEBUG, "buffer[%d] = '%c'\n",i,buffer2[i]);
  }
  for (i=10; i >= 0; i--) {
    logMessage (LOG_DEBUG, "buffer[%d] = '%c'\n",i,buffer2[i]);
  }
  //return strlen(test_message);
  */
  
  //printf("Buffer = %d, JSON = %d",size ,strlen(buffer));
  
  return strlen(buffer);
}

int build_aux_labels_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;
  
  length += sprintf(buffer+length, "{\"type\": \"aux_labels\"");
  
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    length += sprintf(buffer+length, ",\"%s\": \"%s\"", aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label);
  }
  
  length += sprintf(buffer+length, "}");
  
  return length;
  
//printf("%s\n",buffer);
  
  //return strlen(buffer);
}

// WS Received '{"parameter":"SPA_HTR","value":99}'
// WS Received '{"command":"KEY_HTR_POOL"}'
// WS Received '{"command":"GET_AUX_LABELS"}'

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
