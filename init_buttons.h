
#ifndef INIT_BUTTONS_H_
#define INIT_BUTTONS_H_

#define PUMP_INDEX      0
#define SPA_INDEX       1
#define POOL_HEAT_INDEX 9
#define SPA_HEAT_INDEX  10

void initButtons(struct aqualinkdata *aqdata);

#ifndef AQ_RS16
#define TOTAL_BUTONS 12
#else
#define TOTAL_BUTONS 20
// This needs to be called AFTER and as well as initButtons
void initButtons_RS16(struct aqualinkdata *aqdata);
#endif

#endif
