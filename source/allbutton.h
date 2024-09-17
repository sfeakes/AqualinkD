#ifndef ALLBUTTON_H_
#define ALLBUTTON_H_


void processLEDstate(struct aqualinkdata *aq_data, unsigned char *packet, logmask_t from);
bool process_allbutton_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data);

#endif //ALLBUTTON_H_