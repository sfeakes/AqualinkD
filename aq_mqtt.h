
#ifndef AQ_MQTT_H_
#define AQ_MQTT_H_


#define AIR_TEMP_TOPIC "Temperature/Air"
#define POOL_TEMP_TOPIC "Temperature/Pool"
#define SPA_TEMP_TOPIC "Temperature/Spa"
//#define POOL_SETPT_TOPIC "Pool_Heater/setpoint"
//#define SPA_SETPT_TOPIC "Spa_Heater/setpoint"

#define SERVICE_MODE_TOPIC "Service_Mode"

#define ENABELED_SUBT "/enabled"

#define SWG_TOPIC "SWG"
#define SWG_PPM_TOPIC SWG_TOPIC "/PPM"
#define SWG_PPM_F_TOPIC SWG_TOPIC "/PPM_f"
#define SWG_ENABELED_TOPIC SWG_TOPIC ENABELED_SUBT
#define SWG_PERCENT_TOPIC SWG_TOPIC "/Percent"
#define SWG_PERCENT_F_TOPIC SWG_TOPIC "/Percent_f"
#define SWG_SETPOINT_TOPIC SWG_TOPIC "/setpoint"
#define SWG_EXTENDED_TOPIC SWG_TOPIC "/fullstatus"

#define FREEZE_PROTECT "Freeze_Protect"
#define FREEZE_PROTECT_ENABELED FREEZE_PROTECT ENABELED_SUBT

#define BATTERY_STATE "Battery"

#define POOL_THERMO_TEMP_TOPIC BTN_POOL_HTR "/Temperature"
#define SPA_THERMO_TEMP_TOPIC BTN_SPA_HTR "/Temperature"

//#define PUMP_TOPIC "Pump_"
#define PUMP_RPM_TOPIC "/RPM"
#define PUMP_GPH_TOPIC "/GPH"
#define PUMP_WATTS_TOPIC "/Watts"
/*
#define AIR_TEMPERATURE   "Air"
#define POOL_TEMPERATURE  "Pool_Water"
#define SPA_TEMPERATURE   "Spa_Water"
*/
/*
#define AIR_TEMPERATURE_TOPIC AIR_TEMPERATURE "/Temperature"
#define POOL_TEMPERATURE_TOPIC POOL_TEMPERATURE "/Temperature"
#define SPA_TEMPERATURE_TOPIC SPA_TEMPERATURE "/Temperature"
*/
#define SWG_ON 2
#define SWG_OFF 0

#define MQTT_ON "1"
#define MQTT_OFF "0"

#define MQTT_LWM_TOPIC "Alive"


#endif // AQ_MQTT_H_
