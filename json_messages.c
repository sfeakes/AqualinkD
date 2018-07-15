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


//#define test_message "{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}"
//#define test_labels "{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}"

//#define test_message "{\"type\": \"status\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"on\",\"Spa_Mode\": \"on\",\"Aux_1\": \"on\",\"Aux_2\": \"on\",\"Aux_3\": \"on\",\"Aux_4\": \"on\",\"Aux_5\": \"on\",\"Aux_6\": \"on\",\"Aux_7\": \"on\",\"Pool_Heater\": \"on\",\"Spa_Heater\": \"on\",\"Solar_Heater\": \"on\"}}"

//{"type": "aux_labels","Pool Pump": "Pool Pump","Spa Mode": "Spa Mode","Cleaner": "Aux 1","Waterfall": "Aux 2","Spa Blower": "Aux 2","Pool Light": "Aux 4","Spa Light ": "Aux 5","Aux 6": "Aux 6","Aux 7": "Aux 7","Heater": "Heater","Heater": "Heater","Solar Heater": "Solar Heater","(null)": "(null)"}


const char* getStatus(struct aqualinkdata *aqdata)
{
  if (aqdata->active_thread.thread_id != 0) {
    return JSON_PROGRAMMING;
  }
 
  if (aqdata->last_message != NULL && stristr(aqdata->last_message, "SERVICE") != NULL ) {
    return JSON_SERVICE;
  }

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

int build_homebridge_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  length += sprintf(buffer+length, "{\"type\": \"devices\",");
  length += sprintf(buffer+length,  " \"devices\": [");
  
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0 && aqdata->pool_htr_set_point != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_themo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%d\", \"spvalue\": \"%d\", \"value\": \"%d\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?"on":"off",
                                     aqdata->aqbuttons[i].led->state,
                                     aqdata->pool_htr_set_point,
                                     aqdata->pool_temp);
    } else if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name)==0 && aqdata->spa_htr_set_point != TEMP_UNKNOWN) {
      length += sprintf(buffer+length, "{\"type\": \"setpoint_themo\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%d\", \"spvalue\": \"%d\", \"value\": \"%d\" },",
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?"on":"off",
                                     aqdata->aqbuttons[i].led->state,
                                     aqdata->spa_htr_set_point,
                                     aqdata->spa_temp);
    } else {
      length += sprintf(buffer+length, "{\"type\": \"switch\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%d\" },", 
                                     aqdata->aqbuttons[i].name, 
                                     aqdata->aqbuttons[i].label,
                                     aqdata->aqbuttons[i].led->state==ON?"on":"off",
                                     aqdata->aqbuttons[i].led->state);
    }
  }

  //FREEZE_PROTECT  // could add freeze setpoint in future.

  if ( aqdata->swg_percent != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_swg\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"spvalue\": \"%d\", \"value\": \"%d\" },",
                                     SWG_PERCENT_TOPIC,
                                    "Salt Water Genrator %",
                                    aqdata->ar_swg_status == 0x00?"on":"off",
                                    aqdata->swg_percent,
                                    aqdata->swg_percent);

    length += sprintf(buffer+length, "{\"type\": \"Value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
                                   SWG_PPM_TOPIC,
                                   "Salt Water Generator PPM",
                                   "on",
                                   aqdata->swg_ppm);
  }

  length += sprintf(buffer+length, "{\"type\": \"Temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
                                   AIR_TEMPERATURE,
                                   "Pool Air Temperature",
                                   "on",
                                   aqdata->air_temp);
  length += sprintf(buffer+length, "{\"type\": \"Temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
                                   POOL_TEMPERATURE,
                                   "Pool Water Temperature",
                                   "on",
                                   aqdata->pool_temp);
  length += sprintf(buffer+length, "{\"type\": \"Temperature\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" }",
                                   SPA_TEMPERATURE,
                                   "Spa Water Temperature",
                                   "on",
                                   aqdata->spa_temp);

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
  
  length += sprintf(buffer+length, ",\"leds\":{" );
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    char *state;
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
  length += sprintf(buffer+length, "}}" );
  
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
        }
      }
      break;
    }
//printf ("\n");
    if (found >= 4)
    break;
    
    i++;
  }
  
  return true;
}

bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue) {
  int i = 0;
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