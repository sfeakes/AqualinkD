



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
int save_schedules_js(char* inBuf, int inSize, char* outBuf, int outSize);
//void read_schedules();
//void write_schedules();

#endif // AQ_SCHEDULER_H_
