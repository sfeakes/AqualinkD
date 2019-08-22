

#ifndef PDA_H_
#define PDA_H_

#ifdef BETA_PDA_AUTOLABEL
  #include "aqualink.h"
  #include "config.h"
  void init_pda(struct aqualinkdata *aqdata, struct aqconfig *aqconfig);
#else
 void init_pda(struct aqualinkdata *aqdata);
#endif

bool process_pda_packet(unsigned char* packet, int length);
bool pda_shouldSleep();
void pda_wake();
void pda_reset_sleep();

#endif // PDA_MESSAGES_H_