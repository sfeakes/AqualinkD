#ifndef ALLBUTTON_PROGRAMMER_H_
#define ALLBUTTON_PROGRAMMER_H_


void *set_allbutton_pool_heater_temps( void *ptr );
void *set_allbutton_spa_heater_temps( void *ptr );
void *set_allbutton_freeze_heater_temps( void *ptr );
void *set_allbutton_time( void *ptr );
void *get_allbutton_pool_spa_heater_temps( void *ptr );
void *get_allbutton_programs( void *ptr );
void *get_allbutton_freeze_protect_temp( void *ptr );
void *get_allbutton_diag_model( void *ptr );
void *get_allbutton_aux_labels( void *ptr );
//void *threadded_send_cmd( void *ptr );
void *set_allbutton_light_programmode( void *ptr );
void *set_allbutton_light_colormode( void *ptr );
void *set_allbutton_SWG( void *ptr );
void *set_allbutton_boost( void *ptr );

unsigned char pop_allb_cmd(struct aqualinkdata *aq_data);



void aq_send_allb_cmd(unsigned char cmd);

#endif //ALLBUTTON_PROGRAMMER_H_