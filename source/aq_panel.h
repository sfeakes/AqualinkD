
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

typedef enum  {
  RSP_UNKNOWN = -1,
  RSP_SUPPORTED,
  RSP_NOT_SUPPORTED
} panel_support_t;

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

#ifdef AQ_PANEL_C_
const uint32_t PANEL_COMPARISON_MASK = (1U << 12) - 1;  // We only care about bits 1 to 12 (ie nothing higher than RSP_ONET)
#endif


// Bitmask for pannel support against board rev
// used in getPanelSupport()
// uint32_tso 31 is MAX.
#define RSP_SUP_ONET       (1U << 0)  // OneTouch
#define RSP_SUP_AQLT       (1U << 1)  // Aqualink Touch
#define RSP_SUP_IAQL       (1U << 2)  // iAqualink Wifi (1.0/2.0)
#define RSP_SUP_IAQL3      (1U << 3)  // iAqualink WiFi (3.0)
#define RSP_SUP_RSSA       (1U << 4)  // RS Serial Adapter
#define RSP_SUP_VSP        (1U << 5)  // Variable Speed Pumps
#define RSP_SUP_CHEM       (1U << 6)  // chem feeder
#define RSP_SUP_TSCHEM     (1U << 7)  // true sense chem reader
#define RSP_SUP_SWG        (1U << 8)  // Salt water generator
#define RSP_SUP_CLIT       (1U << 9)  // color lights
#define RSP_SUP_DLIT       (1U << 10) // dimmer lights
#define RSP_SUP_VBTN       (1U << 11) // Virtual button
#define RSP_SUP_PLAB       (1U << 12) // Pump VSP by Label and not number
#define RSP_SUP_HPCHIL     (1U << 13) // Heat Pump chiller
#define RSP_SUP_PCDOC      (1U << 14) // PC Dock
#define RSP_SUP_CLIT_RSSA  (1U << 15) // Color Lights can be set over serial adapter.
#define RSP_SUP_ONET_EARLY (1U << 16) // OneTouch REV O uses different tpage for VSP
#define RSP_SUP_SWG_PS     (1U << 17) // Can set SWG setpoints for both pool and spa.
// Don't forget to add to aq_panel.c -- `static const struct { uint32_t mask; const char *name; } s_rsp_sup_names[] = {` -- ~line 500
// uint32_tso 31 is MAX.

#define PANEL_CPU    (1 << 0)
#define PANEL_REV    (1 << 1)
#define PANEL_STRING (1 << 2)
uint8_t setPanelInformationFromPanelMsg(struct aqualinkdata *aqdata, const char *input, uint8_t type, emulation_type source);
//bool setPanelStringFromPanelMsg(struct aqualinkdata *aqdata, const char *src, int src_len, logmask_t from);
//bool setBoardCPURevisionFromPanelMsg (struct aqualinkdata *aqdata, const char *src, int src_len, logmask_t from);
//void initButtons(struct aqualinkdata *aqdata);
void setPanelByName(struct aqualinkdata *aqdata, const char *str);
void setPanel(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual);
const char* getPanelString();
const char* getShortPanelString();

panel_support_t doesPanelSupport(const struct aqualinkdata *aqdata, uint32_t mask);

bool panel_device_request(struct aqualinkdata *aqdata, action_type type, int deviceIndex, int value, request_source source);

void updateLightProgram(struct aqualinkdata *aqdata, int value, clight_detail *light);
void updateButtonLightProgram(struct aqualinkdata *aqdata, int value, int button);

int getWaterTemp(struct aqualinkdata *aqdata);

void changePanelToMode_Only();
void addPanelOneTouchInterface();
void addPanelIAQTouchInterface();
void addPanelRSserialAdapterInterface();
void changePanelToExtendedIDProgramming();

int getPumpDefaultSpeed(pump_detail *pump, bool max);
int getPumpSpeedAsPercent(pump_detail *pump);
int convertPumpPercentToSpeed(pump_detail *pump, int value); // This is probable only needed internally

//uint16_t getPanelSupport( char *rev_string, int rev_len);

aqkey *addVirtualButton(struct aqualinkdata *aqdata, char *label, int vindex);
bool setVirtualButtonLabel(aqkey *button, const char *label);
bool setVirtualButtonAltLabel(aqkey *button, char *label);

clight_detail *getProgramableLight(struct aqualinkdata *aqdata, int button);
pump_detail *getPumpDetail(struct aqualinkdata *aqdata, int button);
void setButtonSpecialMask(aqkey *button, uint16_t masktoset);
float calculate_flow_efficiency_pct(pump_detail *pump);

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


#define VIRTUAL_BUTTONS 32 // This is the only parameter to change if we need more virtual buttons.

#define TOTAL_BUTTONS 20+VIRTUAL_BUTTONS // Biggest jandy panel is 20 buttons (RS16)

#define TOTAL_LEDS  TOTAL_BUTTONS+4  // Only 20 exist in control panel, but need space for the 4 extra buttons on RS16 panel, + every virtual button

// This needs to be called AFTER and as well as initButtons
void initButtons_RS16(struct aqualinkdata *aqdata);


#endif
