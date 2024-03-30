
#ifndef AQ_PANEL_H_
#define AQ_PANEL_H_

#include "config.h"
#include "aqualink.h"

#define PUMP_INDEX      0
#define SPA_INDEX       1
/*
#define POOL_HEAT_INDEX 9
#define SPA_HEAT_INDEX  10
*/

// Defined as int16_t so 16 bits to mask
#define RSP_4      (1 << 0) // 1
#define RSP_6      (1 << 1) // 16
#define RSP_8      (1 << 2) // 4
#define RSP_10     (1 << 3) // 2
#define RSP_12     (1 << 4) // 32
#define RSP_14     (1 << 5) // 8
#define RSP_16     (1 << 6) // 64
#define RSP_COMBO  (1 << 7) // 128
#define RSP_SINGLE (1 << 8) // 128
#define RSP_DUAL_EQPT (1 << 9) // 128
#define RSP_RS     (1 << 10) // 128
#define RSP_PDA    (1 << 11) // 128
#define RSP_ONET   (1 << 12) // 128
#define RSP_IAQT   (1 << 13) // 128
#define RSP_RSSA   (1 << 14) // 128
#define RSP_EXT_PROG (1 << 15) // 128

//void initButtons(struct aqualinkdata *aqdata);
void setPanelByName(struct aqualinkdata *aqdata, const char *str);
void setPanel(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual);
const char* getPanelString();

bool panel_device_request(struct aqualinkdata *aqdata, action_type type, int deviceIndex, int value, request_source source);

void changePanelToMode_Only();
void addPanelOneTouchInterface();
void addPanelIAQTouchInterface();
void addPanelRSserialAdapterInterface();
void changePanelToExtendedIDProgramming();
//void panneltest();

#define isPDA_PANEL ((_aqconfig_.paneltype_mask & RSP_PDA) == RSP_PDA)
#define isRS_PANEL ((_aqconfig_.paneltype_mask & RSP_RS) == RSP_RS)
#define isCOMBO_PANEL ((_aqconfig_.paneltype_mask & RSP_COMBO) == RSP_COMBO)
#define isSINGLE_DEV_PANEL ((_aqconfig_.paneltype_mask & RSP_SINGLE) == RSP_SINGLE)
#define isDUAL_EQPT_PANEL ((_aqconfig_.paneltype_mask & RSP_DUAL_EQPT) == RSP_DUAL_EQPT)
#define isONET_ENABLED ((_aqconfig_.paneltype_mask & RSP_ONET) == RSP_ONET)
#define isIAQT_ENABLED ((_aqconfig_.paneltype_mask & RSP_IAQT) == RSP_IAQT)
#define isRSSA_ENABLED ((_aqconfig_.paneltype_mask & RSP_RSSA) == RSP_RSSA)
#define isEXTP_ENABLED ((_aqconfig_.paneltype_mask & RSP_EXT_PROG) == RSP_EXT_PROG)
int PANEL_SIZE(); 
//
//#define PANEL_SIZE PANEL_SIZE()
/*
#define PANEL_SIZE ((_aqconfig_.paneltype_mask & RSP_4) == RSP_4)?4:(\
                      ((_aqconfig_.paneltype_mask & RSP_6) == RSP_6)?6:(\
                       ((_aqconfig_.paneltype_mask & RSP_8) == RSP_8)?8:(\
                        ((_aqconfig_.paneltype_mask & RSP_10) == RSP_10)?10:(\
                         ((_aqconfig_.paneltype_mask & RSP_12) == RSP_12)?12:(\
                          ((_aqconfig_.paneltype_mask & RSP_14) == RSP_14)?14:(\
                           ((_aqconfig_.paneltype_mask & RSP_16) == RSP_16)?16:0))))))
*/


#ifndef AQ_RS16
#define TOTAL_BUTTONS 12
#else
#define TOTAL_BUTTONS 20
// This needs to be called AFTER and as well as initButtons
void initButtons_RS16(struct aqualinkdata *aqdata);
#endif

#endif
