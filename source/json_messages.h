#ifndef JSON_MESSAGES_H_
#define JSON_MESSAGES_H_

//FUNCTION PROTOTYPES

//#define JSON_LABEL_SIZE 300
//#define JSON_BUFFER_SIZE 4000
//#define JSON_STATUS_SIZE 1024
#define JSON_LABEL_SIZE 600
#define JSON_BUFFER_SIZE 5120
#define JSON_STATUS_SIZE 2048
#define JSON_SIMULATOR_SIZE 2048

#define JSON_MQTT_MSG_SIZE 100

#define JSON_ON      "on"
#define JSON_OFF     "off"
#define JSON_FLASH   "flash"
#define JSON_ENABLED "enabled"

#define JSON_FAHRENHEIT "f"
#define JSON_CELSIUS    "c"
#define JSON_UNKNOWN    "u"

#define JSON_OK      "ok"
#define JSON_LOW     "low"

#define JSON_PROGRAMMING "Programming"
#define JSON_SERVICE     "Service Mode"
#define JSON_TIMEOUT     "Timeout Mode"
#define JSON_READY       "Ready"

struct JSONkeyvalue{
  char *key;
  char *value;
};
struct JSONwebrequest {
  struct JSONkeyvalue first;
  struct JSONkeyvalue second;
  struct JSONkeyvalue third;
};
struct JSONkvptr {
  struct JSONkeyvalue kv[4];
};

const char* getAqualinkDStatusMessage(struct aqualinkdata *aqdata);

int build_aqualink_status_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
int build_aux_labels_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request);
bool parseJSONrequest(char *buffer, struct JSONkvptr *request);
int build_logmsg_JSON(char *dest, int loglevel, const char *src, int dest_len, int src_len);
int build_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float setpoint/*char *svalue*/);
bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue);
int build_aqualink_error_status_JSON(char* buffer, int size, char *msg);
int build_mqtt_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue);
int build_aqualink_aqmanager_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
//int build_device_JSON(struct aqualinkdata *aqdata, int programable_switch, char* buffer, int size, bool homekit);
//int build_device_JSON(struct aqualinkdata *aqdata, int programable_switch1, int programable_switch2, char* buffer, int size, bool homekit);
int build_device_JSON(struct aqualinkdata *aqdata, char* buffer, int size, bool homekit);
int build_aqualink_simulator_packet_JSON(struct aqualinkdata *aqdata, char* buffer, int size);

#endif /* JSON_MESSAGES_H_ */

/*
{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}

{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}


{"type": "status","version": "8157 REV MMM","date": "09/01/16 THU","time": "1:16 PM","temp_units": "F","air_temp": "96","pool_temp": "86","spa_temp": " ","battery": "ok","pool_htr_set_pnt": "85","spa_htr_set_pnt": "99","freeze_protection": "off","frz_protect_set_pnt": "0","leds": {"pump": "on","spa": "off","aux1": "off","aux2": "off","aux3": "off","aux4": "off","aux5": "off","aux6": "off","aux7": "off","pool_heater": "off","spa_heater": "off","solar_heater": "off"}}

{"type": "aux_labels","aux1_label": "Cleaner","aux2_label": "Waterfall","aux3_label": "Spa Blower","aux4_label": "Pool Light","aux5_label": "Spa Light","aux6_label": "Unassigned","aux7_label": "Unassigned"}


{  
  "type":"aux_labels",
  "aux1_label":"Cleaner",
  "aux2_label":"Waterfall",
  "aux3_label":"Spa Blower",
  "aux4_label":"Pool Light",
  "aux5_label":"Spa Light",
  "aux6_label":"Unassigned",
  "aux7_label":"Unassigned"
}


{  
  "type":"status",
  "version":"8157 REV MMM",
  "date":"09/01/16 THU",
  "time":"1:16 PM",
  "temp_units":"F",
  "air_temp":"96",
  "pool_temp":"86",
  "spa_temp":" ",
  "battery":"ok",
  "pool_htr_set_pnt":"85",
  "spa_htr_set_pnt":"99",
  "freeze_protection":"off",
  "frz_protect_set_pnt":"0",
  "leds":{  
    "pump":"on",
    "spa":"off",
    "aux1":"off",
    "aux2":"off",
    "aux3":"off",
    "aux4":"off",
    "aux5":"off",
    "aux6":"off",
    "aux7":"off",
    "pool_heater":"off",
    "spa_heater":"off",
    "solar_heater":"off"
  }
}



*/
