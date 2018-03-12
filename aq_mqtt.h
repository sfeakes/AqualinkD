
#ifndef AQ_MQTT_H_
#define AQ_MQTT_H_


#define AIR_TEMP_TOPIC "Temperature/Air"
#define POOL_TEMP_TOPIC "Temperature/Pool"
#define SPA_TEMP_TOPIC "Temperature/Spa"
#define SWG_PERCENT_TOPIC "SWG/Percent"
#define SWG_PPM_TOPIC "SWG/PPM"

#define FREEZE_PROTECT "Freeze_Protect"

// These need to be moved, but at present only aq_mqtt uses them, so there are here.
// Also need to get the C values from aqualink manual and add those just incase
// someone has the controller set to C.
#define HEATER_MAX 104
#define MEATER_MIN 36
#define FREEZE_PT_MAX 42
#define FREEZE_PT_MIN 36

#endif // AQ_MQTT_H_