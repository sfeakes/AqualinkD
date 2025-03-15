
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

//#define VBUTTON_ONETOUCH_RSSD 0xFF
//#define VBUTTON_RSSD 0xFE

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
// ....Remeber no more for int16_t.......

// Bitmask for pannel support against board rev
// used in getPanelSupport()
#define RSP_SUP_ONET  (1 << 0)
#define RSP_SUP_AQLT  (1 << 1)  // Aqualink Touch
#define RSP_SUP_IAQL (1 << 2 ) // iAqualink Wifi
#define RSP_SUP_RSSA  (1 << 3 ) // RS Serial Adapter
#define RSP_SUP_VSP   (1 << 4)
#define RSP_SUP_CHEM  (1 << 5)  // chem feeder
#define RSP_SUP_TSCHEM (1 << 6 ) // true sense chem reader
#define RSP_SUP_SWG   (1 << 7) // Salt water generator
#define RSP_SUP_CLIT  (1 << 8) // color lights 
#define RSP_SUP_DLIT  (1 << 9) // dimmer lights 
#define RSP_SUP_VBTN  (1 << 10) // Virtual button
#define RSP_SUP_PLAB (1 << 11) // Pump VSP by Label and not number


//void initButtons(struct aqualinkdata *aqdata);
void setPanelByName(struct aqualinkdata *aqdata, const char *str);
void setPanel(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual);
const char* getPanelString();
const char* getShortPanelString();

bool panel_device_request(struct aqualinkdata *aqdata, action_type type, int deviceIndex, int value, request_source source);

void updateButtonLightProgram(struct aqualinkdata *aqdata, int value, int button); 

int getWaterTemp(struct aqualinkdata *aqdata);

void changePanelToMode_Only();
void addPanelOneTouchInterface();
void addPanelIAQTouchInterface();
void addPanelRSserialAdapterInterface();
void changePanelToExtendedIDProgramming();

int getPumpSpeedAsPercent(pump_detail *pump);
int convertPumpPercentToSpeed(pump_detail *pump, int value); // This is probable only needed internally

uint16_t getPanelSupport( char *rev_string, int rev_len);

aqkey *addVirtualButton(struct aqualinkdata *aqdata, char *label, int vindex);
bool setVirtualButtonLabel(aqkey *button, const char *label);
bool setVirtualButtonAltLabel(aqkey *button, const char *label);

clight_detail *getProgramableLight(struct aqualinkdata *aqdata, int button);
pump_detail *getPumpDetail(struct aqualinkdata *aqdata, int button);

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

#define isIAQL_ACTIVE ((_aqconfig_.extended_device_id2 != NUL))

#define isVS_PUMP(mask) ((mask & VS_PUMP) == VS_PUMP)
#define isPLIGHT(mask) ((mask & PROGRAM_LIGHT) == PROGRAM_LIGHT)
#define isVBUTTON(mask) ((mask & VIRTUAL_BUTTON) == VIRTUAL_BUTTON)
#define isVBUTTON_ALTLABEL(mask) ((mask & VIRTUAL_BUTTON_ALT_LABEL) == VIRTUAL_BUTTON_ALT_LABEL)
#define isVBUTTON_CHILLER(mask) ((mask & VIRTUAL_BUTTON_CHILLER) == VIRTUAL_BUTTON_CHILLER)

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



// If we need to increase virtual buttons, then increase below.

// NEED TO FIX, IF WE CHANGE TO ANOTHING OTHER THAN 0 CORE DUMP (we also had "panel_type = RS-16 Combo" in config)
// FIX IS PROBABLY MAKE SURE LEDS IS SAME OR MORE.
#define VIRTUAL_BUTTONS 0


//#define TOTAL_BUTTONS 12+VIRTUAL_BUTTONS

#define TOTAL_BUTTONS 20+VIRTUAL_BUTTONS // Biggest jandy panel
// This needs to be called AFTER and as well as initButtons
void initButtons_RS16(struct aqualinkdata *aqdata);


#endif
