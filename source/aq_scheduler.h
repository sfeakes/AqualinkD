



#ifndef AQ_SCHEDULER_H_
#define AQ_SCHEDULER_H_

#include "config.h"

#define CRON_FILE "/etc/cron.d/aqualinkd"
#define CURL "curl"

#define CV_SIZE 20

typedef struct aqs_cron
{
  int enabled;
  char minute[CV_SIZE];
  char hour[CV_SIZE];
  char daym[CV_SIZE];
  char month[CV_SIZE];
  char dayw[CV_SIZE];
  char url[CV_SIZE * 2];
  char value[CV_SIZE];
} aqs_cron;

int build_schedules_js(char* buffer, int size);
int save_schedules_js(const char* inBuf, int inSize, char* outBuf, int outSize);
void get_cron_pump_times();




#define AQS_PUMP_URL BTN_PUMP "/set"

// All below AQS_ are the same mask, but don;t want CRON in the emum
#define AQS_USE_CRON_PUMP_TIME (1 << 0)
typedef enum reset_event_type{
  AQS_POWER_ON               = (1 << 1),
  AQS_FRZ_PROTECT_OFF        = (1 << 2),
  AQS_BOOST_OFF              = (1 << 3)
} reset_event_type;

#define isAQS_START_PUMP_EVENT_ENABLED ( ((_aqconfig_.schedule_event_mask & AQS_POWER_ON) == AQS_POWER_ON) || \
                                         ((_aqconfig_.schedule_event_mask & AQS_FRZ_PROTECT_OFF) == AQS_FRZ_PROTECT_OFF) || \
                                         ((_aqconfig_.schedule_event_mask & AQS_BOOST_OFF) == AQS_BOOST_OFF) )

//#define isAQS_USE_PUMP_TIME_FROM_CRON_ENABLED !((_aqconfig_.schedule_event_mask & AQS_DONT_USE_CRON_PUMP_TIME) == AQS_DONT_USE_CRON_PUMP_TIME)
#define isAQS_USE_CRON_PUMP_TIME_ENABLED ((_aqconfig_.schedule_event_mask & AQS_USE_CRON_PUMP_TIME) == AQS_USE_CRON_PUMP_TIME)
#define isAQS_POWER_ON_ENABED ((_aqconfig_.schedule_event_mask & AQS_POWER_ON) == AQS_POWER_ON)
#define isAQS_FRZ_PROTECT_OFF_ENABED ((_aqconfig_.schedule_event_mask & AQS_FRZ_PROTECT_OFF) == AQS_FRZ_PROTECT_OFF)
#define isAQS_BOOST_OFF_ENABED ((_aqconfig_.schedule_event_mask & AQS_BOOST_OFF) == AQS_BOOST_OFF)
/*
typedef enum reset_event_type{
  POWER_ON,
  FREEZE_PROTECT_OFF,
  BOOST_OFF
} reset_event_type;
*/
bool event_happened_set_device_state(reset_event_type type, struct aqualinkdata *aq_data);
//void read_schedules();
//void write_schedules();

#endif // AQ_SCHEDULER_H_
