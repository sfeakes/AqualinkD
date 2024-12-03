



#ifndef AQ_SCHEDULER_H_
#define AQ_SCHEDULER_H_


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

typedef enum reset_event_type{
  POWER_ON,
  FREEZE_PROTECT_OFF,
  BOOST_OFF
} reset_event_type;

bool event_happened_set_device_state(reset_event_type type, struct aqualinkdata *aq_data);
//void read_schedules();
//void write_schedules();

#endif // AQ_SCHEDULER_H_
