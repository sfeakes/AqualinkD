
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mongoose.h"

#include "aqualink.h"
#include "net_services.h"
#include "json_messages.h"
#include "aq_mqtt.h"
#include "rs_msg_utils.h"
#include "config.h"
#include "version.h"


// NSF Need to find a better way, this is not thread safe, so don;t want to expost it from net_services.h.
void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message);

#define HASS_DEVICE "\"identifiers\": " \
                        "[\"" AQUALINKD_SHORT_NAME "\"]," \
                        " \"sw_version\": \"" AQUALINKD_VERSION "\"," \
                        " \"model\": \"" AQUALINKD_NAME "\"," \
                        " \"name\": \"AqualinkD\"," \
                        " \"manufacturer\": \"" AQUALINKD_SHORT_NAME "\"," \
                        " \"suggested_area\": \"pool\""

#define HASS_AVAILABILITY "\"payload_available\" : \"1\"," \
                          "\"payload_not_available\" : \"0\"," \
                          "\"topic\": \"%s/" MQTT_LWM_TOPIC "\""
  

const char *HASSIO_CLIMATE_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"climate\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"%s\","
    "\"modes\": [\"off\", \"heat\"],"
    "\"send_if_off\": true,"
    "\"initial\": 36,"
    "\"power_command_topic\": \"%s/%s/set\","
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"current_temperature_topic\": \"%s/%s\","
    "\"min_temp\": 36,"
    "\"max_temp\": 104,"
    "\"mode_command_topic\": \"%s/%s/set\","
    "\"mode_state_topic\": \"%s/%s/enabled\","
    "\"mode_state_template\": \"{%% set values = { '0':'off', '1':'heat'} %%}{{ values[value] if value in values.keys() else 'off' }}\","
    "\"temperature_command_topic\": \"%s/%s/setpoint/set\","
    "\"temperature_state_topic\": \"%s/%s/setpoint\","
    /*"\"temperature_state_template\": \"{{ value_json }}\","*/
    "%s,"
    "\"qos\": 1,"
    "\"retain\": false"
"}";

const char *HASSIO_FREEZE_PROTECT_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"climate\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"Freeze Protect\","
    "\"modes\": [\"off\", \"auto\"],"
    "\"send_if_off\": true,"
    "\"initial\": 34,"
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"current_temperature_topic\": \"%s/%s\","
    "\"min_temp\": 34,"
    "\"max_temp\": 42,"
    "\"mode_state_topic\": \"%s/%s\","
    "\"mode_state_template\": \"{%% set values = { '0':'off', '1':'auto'} %%}{{ values[value] if value in values.keys() else 'off' }}\","
    "\"temperature_command_topic\": \"%s/%s/setpoint/set\","
    "\"temperature_state_topic\": \"%s/%s/setpoint\","
    /*"\"temperature_state_template\": \"{{ value_json }}\""*/
     "%s"
"}";

const char *HASSIO_CONVERT_CLIMATE_TOF = "\"temperature_state_template\": \"{{ (value | float(0) * 1.8 + 32 + 0.5) | int }}\","
                           "\"current_temperature_template\": \"{{ (value | float(0) * 1.8 + 32 + 0.5 ) | int }}\","
                           "\"temperature_command_template\": \"{{ ((value | float(0) -32 ) / 1.8 + 0.5) | int }}\"";

const char *HASSIO_NO_CONVERT_CLIMATE = "\"temperature_state_template\": \"{{ value_json }}\"";

const char *HASSIO_SWG_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"humidifier\","
    "\"device_class\": \"humidifier\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"Salt Water Generator\","
    "\"state_topic\": \"%s/%s\","
    "\"state_template\": \"{%% set values = { '0':'off', '2':'on'} %%}{{ values[value] if value in values.keys() else 'off' }}\","
    "\"command_topic\": \"%s/%s/set\","
    "\"current_humidity_topic\": \"%s/%s\","
    "\"target_humidity_command_topic\": \"%s/%s/set\","
    "\"target_humidity_state_topic\": \"%s/%s\","
    "\"payload_on\": \"2\","
    "\"payload_off\": \"0\","
    "\"min_humidity\":0,"
    "\"max_humidity\":100,"
    "\"optimistic\": false"
"}";

// Use Fan for VSP
// Need to change the max / min.  These do NOT lomit the slider in hassio, only the MQTT limits.
// So the 0-100% should be 600-3450 RPM and 15-130 GPM (ie 1% would = 600 & 0%=off)
// (value-600) / (3450-600) * 100   
// (value) / 100 * (3450-600) + 600  
const char *HASSIO_VSP_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"fan\","
    "\"unique_id\": \"aqualinkd_%s_%s\","   // filter_pump, RPM|GPM
    "\"name\": \"%s Speed\","               // filter_pump
    "\"state_topic\": \"%s/%s\","          // aqualinkd,filter_pump
    "\"command_topic\": \"%s/%s/set\","    // aqualinkd,filter_pump
    "\"json_attributes_topic\": \"%s/%s/delay\","  // aqualinkd,filter_pump
    "\"json_attributes_template\": \"{{ {'delay': value|int} | tojson }}\","
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"percentage_command_topic\": \"%s/%s/%s/set\","       // aqualinkd,filter_pump , RPM|GPM
    "\"percentage_state_topic\":  \"%s/%s/%s\","   // aqualinkd,filter_pump , RPM|GPM
    "\"percentage_value_template\": \"{{ (((value | float(0) - %d) / %d) * 100) | int }}\","     // 600, (3450-600)
    "\"percentage_command_template\": \"{{ ((value | float(0) / 100) * %d) + %d | int }}\","   // (3450-130), 600
    "\"speed_range_max\": \"100\","
    "\"speed_range_min\": \"0\"," //  18|12  600rpm|15gpm
    "\"qos\": 1,"
    "\"retain\": false"
"}";

// Need to add timer attributes to the switches, once figure out how to use in homeassistant
// ie aqualinkd/Filter_Pump/timer/duration

const char *HASSIO_SWITCH_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"switch\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"%s\","
    "\"state_topic\": \"%s/%s\","
    "\"command_topic\": \"%s/%s/set\","
    "\"json_attributes_topic\": \"%s/%s/delay\","
    "\"json_attributes_topic\": \"%s/%s/delay\","
    "\"json_attributes_template\": \"{{ {'delay': value|int} | tojson }}\","
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"qos\": 1,"
    "\"retain\": false"
"}";

const char *HASSIO_TEMP_SENSOR_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"sensor\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"%s Temp\","
    "\"state_topic\": \"%s/%s\","
    "\"value_template\": \"{{ value_json }}\","
    /*"\"unit_of_measurement\": \"°F\","*/
    "\"unit_of_measurement\": \"%s\","
    "\"device_class\": \"temperature\","
    "\"icon\": \"%s\""
"}";

const char *HASSIO_SENSOR_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"sensor\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"%s\","
    "\"state_topic\": \"%s/%s\","
    "\"value_template\": \"{{ value_json }}\","
    "\"unit_of_measurement\": \"%s\","
    "\"icon\": \"%s\""
"}";

const char *HASSIO_ONOFF_SENSOR_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"sensor\","
    "\"unique_id\": \"aqualinkd_%s\","
    "\"name\": \"%s\","
    "\"state_topic\": \"%s/%s\","
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"value_template\": \"{%% set values = { '0':'off', '1':'on'} %%}{{ values[value] if value in values.keys() else 'off' }}\","
    "\"icon\": \"%s\""
"}";

const char *HASSIO_PUMP_SENSOR_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"sensor\","
    "\"unique_id\": \"aqualinkd_%s%d_%s\","
    "\"name\": \"%s %s %s\","
    "\"state_topic\": \"%s/%s%s\","
    "\"value_template\": \"{{ value_json }}\","
    "\"unit_of_measurement\": \"%s\","
    "\"icon\": \"mdi:pump\""
"}";

const char *HASSIO_TEXT_SENSOR_DISCOVER = "{"
   "\"device\": {" HASS_DEVICE "},"
   "\"availability\": {" HASS_AVAILABILITY "},"
   "\"type\": \"sensor\","
   "\"unique_id\": \"aqualinkd_%s\","
   "\"name\": \"%s\","
   "\"state_topic\": \"%s/%s\","
   "\"icon\": \"mdi:card-text\""
"}";

const char *HASSIO_SWG_TEXT_SENSOR_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"sensor\","
    "\"unique_id\": \"%s\","
    "\"name\": \"%s\","
    "\"state_topic\": \"%s/%s\","
    "\"payload_on\": \"0\","
    "\"payload_off\": \"255\","
    "\"value_template\": \"{%% set values = { '0':'Generating',"
                                        "'1':'No flow', "
                                        "'2':'low salt', "
                                        "'4':'high salt', "
                                        "'8':'clean cell', "
                                        "'9':'turning off', "
                                        "'16':'high current', "
                                        "'32':'low volts', "
                                        "'64':'low temp', "
                                        "'128':'Check PCB',"
                                        "'253':'General Fault',"
                                        "'254':'Unknown',"
                                        "'255':'Off'} %%}"
                     "{{ values[value] if value in values.keys() else 'off' }}\","
    "\"icon\": \"mdi:card-text\""
"}";

/*
char *HASSIO_TEXT_DISCOVER = "{"
   "\"device\": {" HASS_DEVICE "},"
   "\"type\": \"text\","
   "\"unique_id\": \"aqualinkd_%s\","
   "\"name\": \"%s\","
   "\"command_topic\": \"junk/null\","
   "\"state_topic\": \"%s/%s\""
"}";
*/
/*
char *HASSIO_SERVICE_MODE_DISCOVER = "{"
  "\"type\": \"sensor\","
  "\"unique_id\": \"aqualinkd_Service_Mode\","
  "\"name\": \"Service Mode\","
  "\"state_topic\": \"aqualinkd/Service_Mode\","
  "\"value_template\": \"{% set values = { '0':'off', '1':'on'} %}{{ values[value] if value in values.keys() else 'off' }}\","
  "\"icon\": \"mdi:account-wrench\""
"}";
*/

/*
Others to add

{
    "type": "text",
    "unique_id": "display",
    "name": "AqualinkD Display Message",
    "command_topic": "junk/null",
    "state_topic": "aqualinkd/Display_Message"
}

{
  "type": "sensor",
  "unique_id": "Service_Mode",
  "name": "Service Mode",
  "state_topic": "aqualinkd/Service_Mode",
  "value_template": "{% set values = { '0':'off', '1':'on'} %}{{ values[value] if value in values.keys() else 'off' }}",
  "icon": "mdi:account-wrench"
}

mdi:pump

mdi:water-outline // orph, ph, ppm, swg

mdi:water-thermometer // water
mdi:thermometer // air

mdi:account-wrench // server
*/


void publish_mqtt_hassio_discover(struct aqualinkdata *aqdata, struct mg_connection *nc)
{
  if (_aqconfig_.mqtt_hass_discover_topic == NULL)
    return;

  int i;
  char msg[JSON_STATUS_SIZE];
  char topic[250];
  char idbuf[128];

  LOG(NET_LOG,LOG_INFO, "MQTT: Publishing discover messages to '%s'\n", _aqconfig_.mqtt_hass_discover_topic);

  for (i=0; i < aqdata->total_buttons; i++) 
  { 
    if (strcmp("NONE",aqdata->aqbuttons[i].label) != 0 ) {
      // Heaters
      if ( (strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0 && (_aqconfig_.force_ps_setpoints || aqdata->pool_htr_set_point != TEMP_UNKNOWN)) ||
           (strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name)==0 && (_aqconfig_.force_ps_setpoints || aqdata->spa_htr_set_point != TEMP_UNKNOWN)) ) {
        sprintf(msg,HASSIO_CLIMATE_DISCOVER,
             _aqconfig_.mqtt_aq_topic,
             aqdata->aqbuttons[i].name, 
             aqdata->aqbuttons[i].label,
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name,
             _aqconfig_.mqtt_aq_topic,(strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0)?POOL_TEMP_TOPIC:SPA_TEMP_TOPIC,
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name, 
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name, 
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name, 
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name,
             (_aqconfig_.convert_mqtt_temp?HASSIO_CONVERT_CLIMATE_TOF:HASSIO_NO_CONVERT_CLIMATE));
        sprintf(topic, "%s/climate/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, aqdata->aqbuttons[i].name);
        send_mqtt(nc, topic, msg);     
      } else {
      // Switches
      //sprintf(msg,"{\"type\": \"switch\",\"unique_id\": \"%s\",\"name\": \"%s\",\"state_topic\": \"aqualinkd/%s\",\"command_topic\": \"aqualinkd/%s/set\",\"json_attributes_topic\": \"aqualinkd/%s/delay\",\"json_attributes_topic\": \"aqualinkd/%s/delay\",\"json_attributes_template\": \"{{ {'delay': value|int} | tojson }}\",\"payload_on\": \"1\",\"payload_off\": \"0\",\"qos\": 1,\"retain\": false}" ,
        sprintf(msg, HASSIO_SWITCH_DISCOVER,
             _aqconfig_.mqtt_aq_topic,
             aqdata->aqbuttons[i].name, 
             aqdata->aqbuttons[i].label, 
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name,
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name,
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name,
             _aqconfig_.mqtt_aq_topic,aqdata->aqbuttons[i].name);
        sprintf(topic, "%s/switch/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, aqdata->aqbuttons[i].name);
        send_mqtt(nc, topic, msg);
      }
    }
  }
  
  // Freezeprotect
  if ( _aqconfig_.force_frzprotect_setpoints || (aqdata->frz_protect_set_point != TEMP_UNKNOWN && aqdata->air_temp != TEMP_UNKNOWN) ) {
    sprintf(msg, HASSIO_FREEZE_PROTECT_DISCOVER,
            _aqconfig_.mqtt_aq_topic,
            FREEZE_PROTECT,
            _aqconfig_.mqtt_aq_topic,AIR_TEMP_TOPIC,
            _aqconfig_.mqtt_aq_topic,FREEZE_PROTECT_ENABELED,
            _aqconfig_.mqtt_aq_topic,FREEZE_PROTECT,
            _aqconfig_.mqtt_aq_topic,FREEZE_PROTECT,
            (_aqconfig_.convert_mqtt_temp?HASSIO_CONVERT_CLIMATE_TOF:HASSIO_NO_CONVERT_CLIMATE));
    sprintf(topic, "%s/climate/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, FREEZE_PROTECT);
    send_mqtt(nc, topic, msg);
  }

  // SWG
  if ( aqdata->swg_percent != TEMP_UNKNOWN ) {

    sprintf(msg, HASSIO_SWG_DISCOVER,
            _aqconfig_.mqtt_aq_topic,
            SWG_TOPIC,
            _aqconfig_.mqtt_aq_topic,SWG_TOPIC,
            _aqconfig_.mqtt_aq_topic,SWG_PERCENT_TOPIC,
            _aqconfig_.mqtt_aq_topic,SWG_PERCENT_TOPIC,
            _aqconfig_.mqtt_aq_topic,SWG_PERCENT_TOPIC,
            _aqconfig_.mqtt_aq_topic,SWG_PERCENT_TOPIC
            );
    sprintf(topic, "%s/humidifier/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, SWG_TOPIC);
    send_mqtt(nc, topic, msg);

    rsm_char_replace(idbuf, SWG_BOOST_TOPIC, "/", "_");
    sprintf(msg, HASSIO_SWITCH_DISCOVER,
             _aqconfig_.mqtt_aq_topic,
             idbuf, 
             "SWG Boost", 
             _aqconfig_.mqtt_aq_topic,SWG_BOOST_TOPIC,
             _aqconfig_.mqtt_aq_topic,SWG_BOOST_TOPIC,
             _aqconfig_.mqtt_aq_topic,SWG_BOOST_TOPIC,
             _aqconfig_.mqtt_aq_topic,SWG_BOOST_TOPIC);
    sprintf(topic, "%s/switch/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);

    rsm_char_replace(idbuf, SWG_PERCENT_TOPIC, "/", "_");
    sprintf(msg, HASSIO_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,idbuf,"SWG Percent",_aqconfig_.mqtt_aq_topic,SWG_PERCENT_TOPIC, "%", "mdi:water-outline");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);

    rsm_char_replace(idbuf, SWG_PPM_TOPIC, "/", "_");
    sprintf(msg, HASSIO_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,idbuf,"SWG PPM",_aqconfig_.mqtt_aq_topic,SWG_PPM_TOPIC, "ppm", "mdi:water-outline");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);

    rsm_char_replace(idbuf, SWG_EXTENDED_TOPIC, "/", "_"); 
    sprintf(msg, HASSIO_SWG_TEXT_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,idbuf,"SWG Msg",_aqconfig_.mqtt_aq_topic,SWG_EXTENDED_TOPIC);
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);
    /*
    // SWG Display message (move to SWG area)
    rsm_char_replace(idbuf, SWG_STATUS_MSG_TOPIC, "/", "_"); 
    sprintf(msg, HASSIO_TEXT_SENSOR_DISCOVER,idbuf,"SWG Msg",_aqconfig_.mqtt_aq_topic,SWG_STATUS_MSG_TOPIC);
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);
    */
  }

  // Temperatures
  sprintf(msg, HASSIO_TEMP_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,"Pool","Pool",_aqconfig_.mqtt_aq_topic,POOL_TEMP_TOPIC,(_aqconfig_.convert_mqtt_temp?"°C":"°F"),"mdi:water-thermometer");
  sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Pool");
  send_mqtt(nc, topic, msg);

  sprintf(msg, HASSIO_TEMP_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,"Spa","Spa",_aqconfig_.mqtt_aq_topic,SPA_TEMP_TOPIC,(_aqconfig_.convert_mqtt_temp?"°C":"°F"),"mdi:water-thermometer");
  sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Spa");
  send_mqtt(nc, topic, msg);

  sprintf(msg, HASSIO_TEMP_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,"Air","Air",_aqconfig_.mqtt_aq_topic,AIR_TEMP_TOPIC,(_aqconfig_.convert_mqtt_temp?"°C":"°F"),"mdi:thermometer");
  sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Air");
  send_mqtt(nc, topic, msg);
  
  // VSP Pumps
  for (i=0; i < aqdata->num_pumps; i++) {
    int maxspeed=3450;   // Min is 600
    int minspeed=600;  // 600 as % of max
    char units[4];
    sprintf(units, "RPM");

    if ( aqdata->pumps[i].pumpType == VFPUMP ) {
      maxspeed=130; // Min is 15
      minspeed=15;   // 15 as % of max
      sprintf(units, "GPM");
    }
    // Create a FAN for pump against the button it' assigned to
    // In the future maybe change this to the pump# or change the sensors to button???
    // Need to change the max / min.  These do NOT lomit the slider in hassio, only the MQTT limits.
    // So the 0-100% should be 600-3450 RPM and 15-130 GPM (ie 1% would = 600 & 0%=off)
    sprintf(msg, HASSIO_VSP_DISCOVER,
            _aqconfig_.mqtt_aq_topic,
            aqdata->pumps[i].button->name,units,
            aqdata->pumps[i].button->label,
            _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name,
            _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name,
            _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name,
            _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name,units,
            _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name,units,
            minspeed, (maxspeed - minspeed),
            (maxspeed - minspeed), minspeed);

    sprintf(topic, "%s/fan/aqualinkd/aqualinkd_%s_%s/config", _aqconfig_.mqtt_hass_discover_topic, aqdata->pumps[i].button->name, units);
    send_mqtt(nc, topic, msg);

    // Create sensors for each pump, against it's pump number
    int pn=i+1;
    if (aqdata->pumps[i].pumpType==VFPUMP || aqdata->pumps[i].pumpType==VSPUMP) {
      // We have GPM info
      sprintf(msg, HASSIO_PUMP_SENSOR_DISCOVER,
              _aqconfig_.mqtt_aq_topic,
              "Pump",pn,"GPM",
              aqdata->pumps[i].button->label,(rsm_strncasestr(aqdata->pumps[i].button->label,"pump",strlen(aqdata->pumps[i].button->label))!=NULL)?"":"Pump","GPM",
              _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name ,PUMP_GPM_TOPIC,
              "GPM");
      sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s%d_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Pump",pn,"GPM");
      send_mqtt(nc, topic, msg);
    }
    sprintf(msg, HASSIO_PUMP_SENSOR_DISCOVER,
              _aqconfig_.mqtt_aq_topic,
              "Pump",pn,"RPM",
              aqdata->pumps[i].button->label,(rsm_strncasestr(aqdata->pumps[i].button->label,"pump",strlen(aqdata->pumps[i].button->label))!=NULL)?"":"Pump","RPM",
              _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name ,PUMP_RPM_TOPIC,
              "RPM");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s%d_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Pump",pn,"RPM");
    send_mqtt(nc, topic, msg);

     sprintf(msg, HASSIO_PUMP_SENSOR_DISCOVER,
              _aqconfig_.mqtt_aq_topic,
              "Pump",pn,"Watts",
              aqdata->pumps[i].button->label,(rsm_strncasestr(aqdata->pumps[i].button->label,"pump",strlen(aqdata->pumps[i].button->label))!=NULL)?"":"Pump","Watts",
              _aqconfig_.mqtt_aq_topic,aqdata->pumps[i].button->name ,PUMP_WATTS_TOPIC,
              "Watts");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s%d_%s/config", _aqconfig_.mqtt_hass_discover_topic, "Pump",pn,"Watts");
    send_mqtt(nc, topic, msg);
  }

  // Chem feeder (ph/orp)
  if (_aqconfig_.force_chem_feeder || aqdata->ph != TEMP_UNKNOWN) { 
    rsm_char_replace(idbuf, CHEM_PH_TOPIC, "/", "_");
    sprintf(msg, HASSIO_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,idbuf,"Water Chemistry pH",_aqconfig_.mqtt_aq_topic,CHEM_PH_TOPIC, "pH", "mdi:water-outline");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);
  }

  if (_aqconfig_.force_chem_feeder || aqdata->orp != TEMP_UNKNOWN) { 
    rsm_char_replace(idbuf, CHEM_ORP_TOPIC, "/", "_");
    sprintf(msg, HASSIO_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,idbuf,"Water Chemistry ORP",_aqconfig_.mqtt_aq_topic,CHEM_ORP_TOPIC, "orp", "mdi:water-outline");
    sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, idbuf);
    send_mqtt(nc, topic, msg);
  }

  // Misc stuff
  sprintf(msg, HASSIO_ONOFF_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,SERVICE_MODE_TOPIC,"Service Mode",_aqconfig_.mqtt_aq_topic,SERVICE_MODE_TOPIC, "mdi:account-wrench");
  sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, SERVICE_MODE_TOPIC);
  send_mqtt(nc, topic, msg);

  /* // Leave below if we decide to go back to a text box
  sprintf(msg, HASSIO_TEXT_DISCOVER,DISPLAY_MSG_TOPIC,"Display Messages",_aqconfig_.mqtt_aq_topic,DISPLAY_MSG_TOPIC);
  sprintf(topic, "%s/text/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, DISPLAY_MSG_TOPIC);
  */
  // It actually works better posting this to sensor and not text.  
  sprintf(msg, HASSIO_TEXT_SENSOR_DISCOVER,_aqconfig_.mqtt_aq_topic,DISPLAY_MSG_TOPIC,"Display Msg",_aqconfig_.mqtt_aq_topic,DISPLAY_MSG_TOPIC);
  sprintf(topic, "%s/sensor/aqualinkd/aqualinkd_%s/config", _aqconfig_.mqtt_hass_discover_topic, DISPLAY_MSG_TOPIC);
  send_mqtt(nc, topic, msg);
  

   
}