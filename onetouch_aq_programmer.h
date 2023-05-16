
#ifndef ONETOUCH_AQ_PROGRAMMER_H_
#define ONETOUCH_AQ_PROGRAMMER_H_

unsigned char pop_ot_cmd(unsigned char receive_type);
bool ot_queue_cmd(unsigned char cmd);

//bool in_ot_programming_mode(struct aqualinkdata *aq_data);

void *set_aqualink_onetouch_pump_rpm( void *ptr );
void *set_aqualink_onetouch_macro( void *ptr );
void *get_aqualink_onetouch_setpoints( void *ptr );
void *set_aqualink_onetouch_spa_heater_temp( void *ptr );
void *set_aqualink_onetouch_pool_heater_temp( void *ptr );
void *set_aqualink_onetouch_swg_percent( void *ptr );
void *set_aqualink_onetouch_boost( void *ptr );
void *set_aqualink_onetouch_time( void *ptr );
void *get_aqualink_onetouch_freezeprotect( void *ptr );
void *set_aqualink_onetouch_freezeprotect( void *ptr );

#endif // ONETOUCH_AQ_PROGRAMMER_H_