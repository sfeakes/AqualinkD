
#ifndef CONFIG_H_
#define CONFIG_H_

#include "utils.h"
#include "aq_serial.h"
#include "aqualink.h"

//#define DEFAULT_LOG_LEVEL    10 
#define DEFAULT_LOG_LEVEL    LOG_NOTICE
//#define DEFAULT_WEBPORT      "6580"
//#define DEFAULT_WEBROOT      "./"
#define DEFAULT_WEBPORT      "80"
#define DEFAULT_WEBROOT      "/var/www/aqualinkd/"
#define DEFAULT_SERIALPORT   "/dev/ttyUSB0"
#define DEFAULT_DEVICE_ID    "0x0a"
#define DEFAULT_MQTT_DZ_IN   NULL // "domoticz/in"
#define DEFAULT_MQTT_DZ_OUT  NULL // "domoticz/out"
#define DEFAULT_HASS_DISCOVER "homeassistant"
#define DEFAULT_MQTT_AQ_TP   "aqualinkd"
#define DEFAULT_MQTT_SERVER  NULL
#define DEFAULT_MQTT_USER    NULL
#define DEFAULT_MQTT_PASSWD  NULL

//#define DEFAULT_SWG_ZERO_IGNORE_COUNT 0

#define MQTT_ID_LEN 18 // 20 seems to kill mosquitto 1.6

// For aqconfig.read_RS485_devmask
#define READ_RS485_SWG      (1 << 0) // 1   SWG
#define READ_RS485_JAN_PUMP (1 << 1) // 2   Jandy Pump
#define READ_RS485_PEN_PUMP (1 << 2) // 4   Pentair Pump
#define READ_RS485_JAN_JXI  (1 << 3) //    Jandy JX & LXi heater
#define READ_RS485_JAN_LX   (1 << 4) //     Jandy LX heater
#define READ_RS485_JAN_CHEM (1 << 5) //     Jandy Chemical Feeder
#define READ_RS485_IAQUALNK (1 << 6) // Read iAqualink messages 
#define READ_RS485_HEATPUMP (1 << 7) // Read HeatPump messages

#define MAX_RSSD_LOG_FILTERS 4

struct aqconfig
{
  char *config_file;
  char *serial_port;
  unsigned int log_level;
  char *socket_port;
  char *web_directory;
  unsigned char device_id;
  unsigned char rssa_device_id;
  int16_t paneltype_mask;
#if defined AQ_ONETOUCH || defined AQ_IAQTOUCH
  unsigned char extended_device_id;
  unsigned char extended_device_id2;
  bool extended_device_id_programming;
  bool enable_iaqualink;
  //bool enable_RS_device_value_print;
#endif
  bool deamonize;
#ifndef AQ_MANAGER // Need to uncomment and clean up referances in future.
  char *log_file;
#endif
  char *mqtt_dz_sub_topic;
  char *mqtt_dz_pub_topic;
  char *mqtt_aq_topic;
  char *mqtt_hass_discover_topic;
  char *mqtt_server;
  char *mqtt_user;
  char *mqtt_passwd;
  bool mqtt_hass_discover_use_mac;
  char mqtt_ID[MQTT_ID_LEN+1];
  int dzidx_air_temp;
  int dzidx_pool_water_temp;
  int dzidx_spa_water_temp;
  int dzidx_swg_percent;
  int dzidx_swg_ppm;
  int dzidx_swg_status;
  float light_programming_mode;
  int light_programming_initial_on;
  int light_programming_initial_off;
  bool override_freeze_protect;
  #ifdef AQ_PDA
  bool pda_sleep_mode;
  #endif
  bool convert_mqtt_temp;
  bool convert_dz_temp;
  bool report_zero_spa_temp;
  bool report_zero_pool_temp;
  //bool read_all_devices;
  //bool read_pentair_packets;
  uint8_t read_RS485_devmask;
  bool use_panel_aux_labels; // Took this option out of config

  uint8_t force_device_devmask;

  //int swg_zero_ignore; // This can be removed since this was due to VSP that's been fixed.
  bool display_warnings_web;
  bool log_protocol_packets; // Read & Write as packets
  bool log_raw_bytes; // Read as bytes
  unsigned char RSSD_LOG_filter[MAX_RSSD_LOG_FILTERS];
  //bool log_raw_RS_bytes;

  bool mqtt_timed_update;
  bool sync_panel_time;
  bool enable_scheduler;
  int8_t schedule_event_mask; // Was int16_t, but no need
  int  sched_chk_pumpon_hour;
  int  sched_chk_pumpoff_hour;
  bool ftdi_low_latency;
  int frame_delay;
  bool device_pre_state;
#ifdef AQ_NO_THREAD_NETSERVICE
  int rs_poll_speed; // Need to remove
  bool thread_netservices; // Need to remove
#endif
};

#ifndef CONFIG_C
extern struct aqconfig _aqconfig_;
#else
struct aqconfig _aqconfig_;
#endif


#define READ_RSDEV_SWG ((_aqconfig_.read_RS485_devmask & READ_RS485_SWG) == READ_RS485_SWG)
#define READ_RSDEV_ePUMP ((_aqconfig_.read_RS485_devmask & READ_RS485_JAN_PUMP) == READ_RS485_JAN_PUMP)
#define READ_RSDEV_vsfPUMP ((_aqconfig_.read_RS485_devmask & READ_RS485_PEN_PUMP) == READ_RS485_PEN_PUMP)
#define READ_RSDEV_JXI ((_aqconfig_.read_RS485_devmask & READ_RS485_JAN_JXI) == READ_RS485_JAN_JXI)
#define READ_RSDEV_LX ((_aqconfig_.read_RS485_devmask & READ_RS485_JAN_LX) == READ_RS485_JAN_LX)
#define READ_RSDEV_CHEM ((_aqconfig_.read_RS485_devmask & READ_RS485_JAN_CHEM) == READ_RS485_JAN_CHEM)
#define READ_RSDEV_iAQLNK ((_aqconfig_.read_RS485_devmask & READ_RS485_IAQUALNK) == READ_RS485_IAQUALNK)
#define READ_RSDEV_HPUMP ((_aqconfig_.read_RS485_devmask & READ_RS485_HEATPUMP) == READ_RS485_HEATPUMP)

#define isPDA_IAQT (_aqconfig_.device_id == 0x33)
//#define isPDA ((_aqconfig_.paneltype_mask & RSP_PDA) == RSP_PDA)


#define FORCE_SWG_SP           (1 << 0)
#define FORCE_POOLSPA_SP       (1 << 1)
#define FORCE_FREEZEPROTECT_SP (1 << 2)
#define FORCE_CHEM_FEEDER      (1 << 3)
#define FORCE_CHILLER          (1 << 4)

#define ENABLE_SWG           ((_aqconfig_.force_device_devmask & FORCE_SWG_SP) == FORCE_SWG_SP)
#define ENABLE_HEATERS       ((_aqconfig_.force_device_devmask & FORCE_POOLSPA_SP) == FORCE_POOLSPA_SP)
#define ENABLE_FREEZEPROTECT ((_aqconfig_.force_device_devmask & FORCE_FREEZEPROTECT_SP) == FORCE_FREEZEPROTECT_SP)
#define ENABLE_CHEM_FEEDER   ((_aqconfig_.force_device_devmask & FORCE_CHEM_FEEDER) == FORCE_CHEM_FEEDER)
#define ENABLE_CHILLER       ((_aqconfig_.force_device_devmask & FORCE_CHILLER) == FORCE_CHILLER)

/*
#ifndef CONFIG_C
#ifdef AQUALINKD_C
extern struct aqconfig _aqconfig_;
#else
extern const struct aqconfig _aqconfig_;
#endif
#endif
*/

void init_parameters (struct aqconfig * parms);
//bool parse_config (struct aqconfig * parms, char *cfgfile);
//void readCfg (struct aqconfig *config_parameters, char *cfgFile);
//void readCfg (struct aqconfig *config_parameters, struct aqualinkdata *aqualink_data, char *cfgFile);
void read_config(struct aqualinkdata *aqdata, char *cfgFile);
void init_config();

bool writeCfg (struct aqualinkdata *aqdata);
bool setConfigValue(struct aqualinkdata *aqdata, char *param, char *value);
bool mac(char *buf, int len, bool useDelimiter);
char *cleanalloc(char *str);
char *ncleanalloc(char *str, int length);

const char *pumpType2String(pump_type ptype);

int save_config_js(const char* inBuf, int inSize, char* outBuf, int outSize, struct aqualinkdata *aqdata);
void check_print_config (struct aqualinkdata *aqdata);


typedef enum cfg_value_type{
  CFG_STRING,
  CFG_INT,
  CFG_FLOAT,
  CFG_HEX,
  CFG_BOOL,
  CFG_BITMASK,
  CFG_SPECIAL
} cfg_value_type;


#define CFG_PERSISTANT        (1 << 0) // Don't free memory, things referance the pointer
#define CFG_NO_EDIT           (1 << 1) // Don't allow editing
#define CFG_GRP_ADVANCED      (1 << 2) // Show in group advanced
#define CFG_HIDE              (1 << 3) // Like passwords.
//#define CFG_      (1 << 3)

#define isMASKSET(mask, bit) ((mask & bit) == bit)

typedef struct cfgParam {
  void *value_ptr;
  void *default_value;
  cfg_value_type value_type;
  uint8_t config_mask;
  char *name;
  char *valid_values;
  uint8_t mask;
  //bool advanced;
} cfgParam;

#ifndef CONFIG_C
extern cfgParam _cfgParams[];
extern int _numCfgParams;
#else
cfgParam _cfgParams[100];
int _numCfgParams;
#endif // CONFIG_C


// Below are missed
//RSSD_LOG_filter
//debug_log_mask
#define CFG_V_BOOL                              "[\"Yes\", \"No\"]"

#define CFG_N_serial_port                       "serial_port"
#define CFG_C_serial_port                       11
#define CFG_N_log_level                         "log_level"
#define CFG_V_log_level                         "[\"DEBUG\", \"INFO\", \"NOTICE\", \"WARNING\", \"ERROR\"]"
#define CFG_C_log_level                         9
#define CFG_N_socket_port                       "socket_port" // Change to Web_socket
#define CFG_C_socket_port                       11
#define CFG_N_web_directory                     "web_directory"
#define CFG_C_web_directory                     13
#define CFG_N_device_id                         "device_id"
#define CFG_V_device_id                         "[\"0x0a\", \"0x0b\", \"0x09\", \"0x08\", \"0x60\", \"0xFF\"]"
#define CFG_C_device_id                         9
#define CFG_N_rssa_device_id                    "rssa_device_id"
#define CFG_V_rssa_device_id                    "[\"0x00\", \"0x48\", \"0xFF\"]"
#define CFG_C_rssa_device_id                    14

#define CFG_N_RSSD_LOG_filter                   "RSSD_LOG_filter"
#define CFG_C_RSSD_LOG_filter                   15

#define CFG_N_panel_type                        "panel_type"
#define CFG_C_panel_type                        10
#define CFG_N_extended_device_id                "extended_device_id"
#define CFG_V_extended_device_id                "[\"0x00\", \"0x30\", \"0x31\", \"0x32\", \"0x33\", \"0x40\", \"0x41\", \"0x42\", \"0x43\", \"0xFF\"]"
#define CFG_C_extended_device_id                18

#define CFG_N_sync_panel_time                   "sync_panel_time"
#define CFG_C_sync_panel_time                   15 

//#define CFG_N_extended_device_id2               "extended_device_id2"
//#define CFG_C_extended_device_id2               20
#define CFG_N_extended_device_id_programming    "extended_device_id_programming"
#define CFG_C_extended_device_id_programming    30
#define CFG_N_enable_iaqualink                  "enable_iaqualink"
#define CFG_C_enable_iaqualink                  16
#define CFG_N_log_file                          "log_file"
#define CFG_C_log_file                          8
#define CFG_N_mqtt_aq_topic                     "mqtt_aq_topic"
#define CFG_C_mqtt_aq_topic                     13
#define CFG_N_mqtt_server                       "mqtt_address"
#define CFG_C_mqtt_server                       12
#define CFG_N_mqtt_user                         "mqtt_user"
#define CFG_C_mqtt_user                         9
#define CFG_N_mqtt_passwd                       "mqtt_passwd"
#define CFG_C_mqtt_passwd                       11
#define CFG_N_mqtt_hass_discover_topic          "mqtt_ha_discover_topic"
#define CFG_C_mqtt_hass_discover_topic          24
#define CFG_N_mqtt_hass_discover_use_mac        "mqtt_ha_discover_use_mac"
#define CFG_C_mqtt_hass_discover_use_mac        27
#define CFG_N_mqtt_timed_update                 "mqtt_timed_update"
#define CFG_C_mqtt_timed_update                 17
//#define CFG_N_mqtt_ID                           "mqtt_ID"
//#define CFG_C_mqtt_ID                           7
#define CFG_N_mqtt_dz_sub_topic                 "mqtt_dz_sub_topic"
#define CFG_C_mqtt_dz_sub_topic                 17
#define CFG_N_mqtt_dz_pub_topic                 "mqtt_dz_pub_topic"
#define CFG_C_mqtt_dz_pub_topic                 17
#define CFG_N_dzidx_air_temp                    "dzidx_air_temp"
#define CFG_C_dzidx_air_temp                    14
#define CFG_N_dzidx_pool_water_temp             "dzidx_pool_water_temp"
#define CFG_C_dzidx_pool_water_temp             21
#define CFG_N_dzidx_spa_water_temp              "dzidx_spa_water_temp"
#define CFG_C_dzidx_spa_water_temp              20
#define CFG_N_dzidx_swg_percent                 "dzidx_SWG_percent"
#define CFG_C_dzidx_swg_percent                 17
#define CFG_N_dzidx_swg_ppm                     "dzidx_SWG_PPM"
#define CFG_C_dzidx_swg_ppm                     13
#define CFG_N_dzidx_swg_status                  "dzidx_SWG_Status"
#define CFG_C_dzidx_swg_status                  16
#define CFG_N_light_programming_mode            "light_programming_mode"
#define CFG_C_light_programming_mode            22
#define CFG_N_light_programming_initial_on      "light_programming_initial_on"
#define CFG_C_light_programming_initial_on      28
#define CFG_N_light_programming_initial_off     "light_programming_initial_off"
#define CFG_C_light_programming_initial_off     29
#define CFG_N_override_freeze_protect           "override_freeze_protect"
#define CFG_C_override_freeze_protect           23
#define CFG_N_pda_sleep_mode                    "pda_sleep_mode"
#define CFG_C_pda_sleep_mode                    14
#define CFG_N_convert_mqtt_temp                 "mqtt_convert_temp_to_c"
#define CFG_C_convert_mqtt_temp                 22
#define CFG_N_convert_dz_temp                   "dz_convert_temp_to_c"
#define CFG_C_convert_dz_temp                   20
#define CFG_N_report_zero_spa_temp              "report_zero_spa_temp"
#define CFG_C_report_zero_spa_temp              20
#define CFG_N_report_zero_pool_temp             "report_zero_pool_temp"
#define CFG_C_report_zero_pool_temp             21
#define CFG_N_read_RS485_devmask                "read_RS485_devmask"
#define CFG_C_read_RS485_devmask                18
#define CFG_N_use_panel_aux_labels              "use_panel_aux_labels"
#define CFG_C_use_panel_aux_labels              20
#define CFG_N_force_swg                         "force_swg"
#define CFG_C_force_swg                         9
#define CFG_N_force_ps_setpoints                "force_ps_setpoints"
#define CFG_C_force_ps_setpoints                18
#define CFG_N_force_frzprotect_setpoints        "force_frzprotect_setpoints"
#define CFG_C_force_frzprotect_setpoints        26
#define CFG_N_force_chem_feeder                 "force_chem_feeder"
#define CFG_C_force_chem_feeder                 17
#define CFG_N_force_chiller                     "force_chiller"
#define CFG_N_display_warnings_web              "display_warnings_web"
#define CFG_C_display_warnings_web              20
#define CFG_N_log_protocol_packets              "log_protocol_packets"
#define CFG_C_log_protocol_packets              20
#define CFG_N_device_pre_state                  "device_pre_state"
#define CFG_C_device_pre_state                  16

#define CFG_N_read_RS485_swg                    "read_RS485_swg"
#define CFG_C_read_RS485_swg                    14
#define CFG_N_read_RS485_ePump                  "read_RS485_ePump"
#define CFG_C_read_RS485_ePump                  16
#define CFG_N_read_RS485_vsfPump                "read_RS485_vsfPump"
#define CFG_C_read_RS485_vsfPump                18
#define CFG_N_read_RS485_JXi                    "read_RS485_JXi"
#define CFG_C_read_RS485_JXi                    14
#define CFG_N_read_RS485_LX                     "read_RS485_LX"
#define CFG_C_read_RS485_LX                     13
#define CFG_N_read_RS485_Chem                   "read_RS485_Chem"
#define CFG_C_read_RS485_Chem                   15
#define CFG_N_read_RS485_iAqualink              "read_RS485_iAqualink"
#define CFG_C_read_RS485_iAqualink              20
#define CFG_N_read_RS485_HeatPump               "read_RS485_HeatPump"


#define CFG_N_enable_scheduler                  "enable_scheduler"
#define CFG_C_enable_scheduler                  16

#define CFG_N_event_check_poweron               "event_poweron_check_pump"
#define CFG_C_event_check_poweron               24
#define CFG_N_event_check_freezeprotectoff      "event_freezeprotectoff_check_pump"
#define CFG_C_event_check_freezeprotectoff      33
#define CFG_N_event_check_boostoff              "event_boostoff_check_pump"
#define CFG_C_event_check_boostoff              25
#define CFG_N_event_check_pumpon_hour           "event_check_pumpon_hour"
#define CFG_C_event_check_pumpon_hour           23
#define CFG_N_event_check_pumpoff_hour         "event_check_pumpoff_hour"
#define CFG_C_event_check_pumpoff_hour          24
#define CFG_N_event_check_usecron               "event_check_use_scheduler_times"
#define CFG_C_event_check_usecron               32

#define CFG_N_ftdi_low_latency                  "ftdi_low_latency"
#define CFG_C_ftdi_low_latency                  16
#define CFG_N_rs485_frame_delay                 "rs485_frame_delay"
#define CFG_C_rs485_frame_delay                 17

#endif
