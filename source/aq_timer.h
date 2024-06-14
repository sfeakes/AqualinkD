
#ifndef AQ_TIMER_H_
#define AQ_TIMER_H_

#include "aqualink.h"

void start_timer(struct aqualinkdata *aq_data, /*aqkey *button,*/ int deviceIndex, int duration);
int get_timer_left(aqkey *button);
void clear_timer(struct aqualinkdata *aq_data, /*aqkey *button,*/ int deviceIndex);
// Not best place for this, but leave it here so all requests are in net services, this is forward decleration of function in net_services.c
#ifdef AQ_PDA
void create_PDA_on_off_request(aqkey *button, bool isON);
#endif

#endif // AQ_TIMER_H_