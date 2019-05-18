
#ifndef CONFIG_H_
#define CONFIG_H_

#include "utils.h"
#include "aq_serial.h"
#include "aqualink.h"


//#define DEFAULT_LOG_LEVEL    10 
#define DEFAULT_LOG_LEVEL    LOG_NOTICE
#define DEFAULT_WEBPORT      "6580"
#define DEFAULT_WEBROOT      "./"
#define DEFAULT_SERIALPORT   "/dev/ttyUSB0"
#define DEFAULT_DEVICE_ID    "0x0a"
#define DEFAULT_MQTT_DZ_IN   NULL
#define DEFAULT_MQTT_DZ_OUT  NULL
#define DEFAULT_MQTT_AQ_TP   NULL
#define DEFAULT_MQTT_SERVER  NULL
#define DEFAULT_MQTT_USER    NULL
#define DEFAULT_MQTT_PASSWD  NULL
#define DEFAULT_SWG_ZERO_IGNORE_COUNT 0

#define MQTT_ID_LEN 18 // 20 seems to kill mosquitto 1.6

// For aqconfig.read_RS485_devmask
#define READ_RS485_SWG      (1 << 0) // 1   SWG
#define READ_RS485_JAN_PUMP (1 << 1) // 2   Jandy Pump
#define READ_RS485_PEN_PUMP (1 << 2) // 4   Pentair Pump

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
  bool extended_device_id_programming;
#endif
  bool deamonize;
  char *log_file;
  char *mqtt_dz_sub_topic;
  char *mqtt_dz_pub_topic;
  char *mqtt_aq_topic;
  char *mqtt_server;
  char *mqtt_user;
  char *mqtt_passwd;
  char *habridge_server;
  char mqtt_ID[MQTT_ID_LEN];
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
  bool use_panel_aux_labels;
  bool force_swg;
  int swg_zero_ignore;
  bool display_warnings_web;
  bool read_pentair_packets;
  bool debug_RSProtocol_packets;
  bool log_raw_RS_bytes;
  bool readahead_b4_write;
  bool mqtt_timed_update;
  bool sync_panel_time;
  int rs_poll_speed;
  bool thread_netservices;
};

#ifndef CONFIG_C
extern struct aqconfig _aqconfig_;
#else
struct aqconfig _aqconfig_;
#endif


#define READ_RSDEV_SWG ((_aqconfig_.read_RS485_devmask & READ_RS485_SWG) == READ_RS485_SWG)
#define READ_RSDEV_ePUMP ((_aqconfig_.read_RS485_devmask & READ_RS485_JAN_PUMP) == READ_RS485_JAN_PUMP)
#define READ_RSDEV_vsfPUMP ((_aqconfig_.read_RS485_devmask & READ_RS485_PEN_PUMP) == READ_RS485_PEN_PUMP)

//#define isPDA ((_aqconfig_.paneltype_mask & RSP_PDA) == RSP_PDA)


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

char *cleanalloc(char*str);

#endif
