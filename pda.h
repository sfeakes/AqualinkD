

#ifndef PDA_H_
#define PDA_H_


void init_pda(struct aqualinkdata *aqdata);

bool process_pda_packet(unsigned char* packet, int length);
bool pda_shouldSleep();
void pda_wake();
void pda_reset_sleep();

#endif // PDA_MESSAGES_H_