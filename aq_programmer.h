
#ifndef AQ_PROGRAMMER_H_
#define AQ_PROGRAMMER_H_


// need to get the C values from aqualink manual and add those just incase
// someone has the controller set to C.
#define HEATER_MAX 104
#define MEATER_MIN 36
#define FREEZE_PT_MAX 42
#define FREEZE_PT_MIN 36
#define SWG_PERCENT_MAX 101
#define SWG_PERCENT_MIN 0

#define PTHREAD_ARG 25
#define LIGHT_MODE_BUFER PTHREAD_ARG

typedef enum {
  AQP_NULL = -1,
  AQ_GET_POOL_SPA_HEATER_TEMPS,
  AQ_GET_FREEZE_PROTECT_TEMP,
  AQ_SET_TIME,
  AQ_SET_POOL_HEATER_TEMP,
  AQ_SET_SPA_HEATER_TEMP,
  AQ_SET_FRZ_PROTECTION_TEMP,
  AQ_GET_DIAGNOSTICS_MODEL,
  AQ_SEND_CMD,
  AQ_GET_PROGRAMS,
  AQ_SET_COLORMODE,
  AQ_PDA_INIT,
  AQ_SET_SWG_PERCENT,
  AQ_PDA_DEVICE_STATUS,
  AQ_PDA_DEVICE_ON_OFF
} program_type;

struct programmingThreadCtrl {
  pthread_t thread_id;
  //void *thread_args;
  char thread_args[PTHREAD_ARG];
  struct aqualinkdata *aq_data;
};



//void aq_programmer(program_type type, void *args, struct aqualinkdata *aq_data);
void aq_programmer(program_type type, char *args, struct aqualinkdata *aq_data);
void kick_aq_program_thread(struct aqualinkdata *aq_data);

unsigned char pop_aq_cmd(struct aqualinkdata *aq_data);
//bool push_aq_cmd(unsigned char cmd);

//void send_cmd(unsigned char cmd, struct aqualinkdata *aq_data);
//void cancel_menu(struct aqualinkdata *aq_data);

//void *set_aqualink_time( void *ptr );
//void *get_aqualink_pool_spa_heater_temps( void *ptr );

int get_aq_cmd_length();

#endif
