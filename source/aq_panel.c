/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "domoticz.h"
#include "aq_panel.h"
#include "serialadapter.h"
#include "aq_timer.h"
#include "allbutton_aq_programmer.h"
#include "rs_msg_utils.h"
#include "iaqualink.h"

void initPanelButtons(struct aqualinkdata *aqdata, bool rspda, int size, bool combo, bool dual);
void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int button);


char *name2label(char *str)
{
  int len = strlen(str);
 
  char *newst = malloc(sizeof *newst * (len+1));

  unsigned int i;
  for(i = 0; i < len; i++) {
    if ( str[i] == '_' )
      newst[i] = ' ';
    else
      newst[i] = str[i];
  }
  newst[len] = '\0';
  
  return newst;
}

// This has NOT been tested.
uint16_t getPanelSupport( char *rev_string, int rev_len)
{
  uint16_t supported = 0;
  
  char REV[5];

  if (! rsm_get_revision(REV, rev_string, rev_len) ) {
    LOG(PANL_LOG,LOG_ERR, "Couldn't get panel revision from '%s'\n",rev_string);
    return 0; // No point in continue
  } else if (REV[0] > 90 || REV[0] < 65) { // > Z or < A
    LOG(PANL_LOG,LOG_WARNING, "Panel revision is not understood '%s', please report this issue");
  }


  // Get the actual rev letter
  //if ( rsm_get_revision(REV, rev_string, rev_len) ) {
    // Rev >=I == one touch protocol
    // Rev >=O == VSP
    // Rev >=Q == iaqualink touch protocol.
    // REv >= P == chemlink
    // Rev >= I serial adapter.
    // Rev >= F == Dommer.  But need serial protocol so set to I
    // Rev >= L == JandyColors Smart Light Control
    // Rev >= MMM = 12V JandyColor Lights (also light dimmer)
    // Rev >= N Hayward ColorLogic LED Light
    // Rev >= O.1== Jandy WaterColors LED ( 9 colors )
    // Rev >= T.0.1 == limited color light
    // Rec >= T.2 == more color lights
    // Rev >= Q Aqualink Touch protocol
    // Rev >= R iAqualink (wifi adapter) protocol
    // Rev >= L PC Dock
    // Rev >= W pump label (not number)
    // Rev >= Yg Virtual Device called Label Auxiliraries

    if (REV[0] > 89 || ( REV[0] == 89 && REV[1] >= 103))
      supported |= RSP_SUP_VBTN;

    if (REV[0] >= 81) // Q in ascii
      supported |= RSP_SUP_AQLT;

    if (REV[0] >= 82) // R
      supported |= RSP_SUP_IAQL;
    
    if (REV[0] >= 80) // P in ascii
      supported |= RSP_SUP_CHEM;

    if (REV[0] >= 79) // O in ascii
      supported |= RSP_SUP_VSP;

    if (REV[0] >= 73){ // I in ascii
      supported |= RSP_SUP_ONET;
      supported |= RSP_SUP_RSSA;
      supported |= RSP_SUP_SWG;
    }
    
    if (REV[0] >= 76) // L in ascii
      supported |= RSP_SUP_CLIT;

    if (REV[0] >= 73) // I in ascii, dimmer came out in F, but we use the serial adapter to set, so use that as support
      supported |= RSP_SUP_DLIT;

    //if (REV[0] > 84 || (REV[0] == 84 && REV[1] == 64 && REV[2] >= 50) ) // T in ascii (or T and . and 2 )
    //  supported |= RSP_SUP_CLIT4;
    
  //}

  return supported;
}

void changePanelToMode_Only() {
  _aqconfig_.paneltype_mask |= RSP_SINGLE;
  _aqconfig_.paneltype_mask &= ~RSP_COMBO;
}

void changePanelToExtendedIDProgramming() {
  _aqconfig_.paneltype_mask |= RSP_EXT_PROG;
  LOG(PANL_LOG,LOG_NOTICE, "AqualinkD is using use %s mode for programming (where supported)\n",isONET_ENABLED?"ONETOUCH":"IAQ TOUCH");
}

void addPanelRSserialAdapterInterface() {
  _aqconfig_.paneltype_mask |= RSP_RSSA;
}
void addPanelOneTouchInterface() {
  _aqconfig_.paneltype_mask |= RSP_ONET;
  _aqconfig_.paneltype_mask &= ~RSP_IAQT;
}
void addPanelIAQTouchInterface() {
  _aqconfig_.paneltype_mask |= RSP_IAQT;
  _aqconfig_.paneltype_mask &= ~RSP_ONET;
}

int PANEL_SIZE() {
  if ((_aqconfig_.paneltype_mask & RSP_4) == RSP_4)
    return 4;
  else if ((_aqconfig_.paneltype_mask & RSP_6) == RSP_6)
    return 6;
  else if ((_aqconfig_.paneltype_mask & RSP_8) == RSP_8)
    return 8;
  else if ((_aqconfig_.paneltype_mask & RSP_12) == RSP_12)
    return 10;
  else if ((_aqconfig_.paneltype_mask & RSP_10) == RSP_10)
    return 12;
  else if ((_aqconfig_.paneltype_mask & RSP_14) == RSP_14)
    return 14;
  else if ((_aqconfig_.paneltype_mask & RSP_16) == RSP_16)
    return 16;
  
  LOG(PANL_LOG,LOG_ERR, "Internal error, panel size not set, using 8\n");
  return 8;
}
//bool setPanel(const char *str);
/*
void panneltest() {
setPanel("RS-16 Combo");
setPanel("PD-8 Only");
setPanel("PD-8 Combo");
setPanel("RS-2/14 Dual");
setPanel("RS-2/10 Dual");
setPanel("RS-16 Only");
setPanel("RS-12 Only");
setPanel("RS-16 Combo");
setPanel("RS-12 Combo");
setPanel("RS-2/6 Dual");
setPanel("RS-4 Only");
setPanel("RS-6 Only");
setPanel("RS-8 Only");
setPanel("RS-4 Combo");
setPanel("RS-6 Combo");
setPanel("RS-8 Combo");
}
*/

char _panelString[60];
char _panelStringShort[60];
void setPanelString()
{
  snprintf(_panelString, sizeof(_panelString), "%s%s-%s%d %s",
      isRS_PANEL?"RS":"",
      isPDA_PANEL?"PDA":"", // No need for both of these, but for error validation leave it in.
      isDUAL_EQPT_PANEL?"2/":"",
      PANEL_SIZE(),
      isDUAL_EQPT_PANEL?"Dual Equipment":(
        isCOMBO_PANEL?"Combo (Pool & Spa)":(isSINGLE_DEV_PANEL?"Only (Pool or Spa)":"")
      ));

  snprintf(_panelStringShort, sizeof(_panelString), "%s%s-%s%d %s",
      isRS_PANEL?"RS":"",
      isPDA_PANEL?"PDA":"", // No need for both of these, but for error validation leave it in.
      isDUAL_EQPT_PANEL?"2/":"",
      PANEL_SIZE(),
      isDUAL_EQPT_PANEL?"Dual":(
        isCOMBO_PANEL?"Combo":(isSINGLE_DEV_PANEL?"Only":"")
      ));
}
const char* getPanelString()
{
  return _panelString;
}

const char* getShortPanelString()
{
  return _panelStringShort;
}


//bool setPanelByName(const char *str) {

int setSizeMask(int size)
{
  int rtn_size = 0;

  if ( size > TOTAL_BUTTONS) {
    LOG(PANL_LOG,LOG_ERR, "Panel size is either invalid or too large for compiled parameters, ignoring %d using %d\n",size, TOTAL_BUTTONS);
    rtn_size = TOTAL_BUTTONS;
  }

  switch (size) {
    case 4:
      _aqconfig_.paneltype_mask |= RSP_4;
    break;
    case 6:
      _aqconfig_.paneltype_mask |= RSP_6;
    break;
    case 8:
      _aqconfig_.paneltype_mask |= RSP_8;
    break;
    case 10:
      _aqconfig_.paneltype_mask |= RSP_10;
    break;
    case 12:
      _aqconfig_.paneltype_mask |= RSP_12;
    break;
    case 14:
      _aqconfig_.paneltype_mask |= RSP_14;
    break;
    case 16:
      _aqconfig_.paneltype_mask |= RSP_16;
    break;
    default:
      LOG(PANL_LOG,LOG_ERR, "Didn't understand panel size, '%d' setting to size to 8\n",size);
      _aqconfig_.paneltype_mask |= RSP_8;
      rtn_size = 8;
    break;
  }

  if (rtn_size > 0)
    return rtn_size;

  return size;
}

void setPanel(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual)
{
  #ifndef AQ_PDA
    if (!rs) {
      LOG(PANL_LOG,LOG_ERR, "Can't use PDA mode, AqualinkD has not been compiled with PDA Enabled\n");
      rs = true;
    }
  #endif

  _aqconfig_.paneltype_mask = 0;

  int nsize = setSizeMask(size);

  if (rs)
    _aqconfig_.paneltype_mask |= RSP_RS;
  else
    _aqconfig_.paneltype_mask |= RSP_PDA;

  if (combo)
    _aqconfig_.paneltype_mask |= RSP_COMBO;
  else
    _aqconfig_.paneltype_mask |= RSP_SINGLE;

  if (dual) { // Dual are combo
    _aqconfig_.paneltype_mask |= RSP_DUAL_EQPT;
    _aqconfig_.paneltype_mask |= RSP_COMBO;
    _aqconfig_.paneltype_mask &= ~RSP_SINGLE;
  }

  initPanelButtons(aqdata, rs, nsize, combo, dual);
  
  setPanelString();
}

void setPanelByName(struct aqualinkdata *aqdata, const char *str)
{
  int i;
  int size = 0;
  bool rs = true;
  bool combo = true;
  bool dual = false;
  //int16_t panelbits = 0;
  
  if (str[0] == 'R' && str[1] == 'S') { // RS Panel
    rs = true;
    if (str[4] == '/')
      size = atoi(&str[5]);
    else
      size = atoi(&str[3]);
  } else if (str[0] == 'P' && str[1] == 'D') { // PDA Panel
    rs = false;
    if (str[2] == '-' || str[2] == ' ') // Account for PD-8
      size = atoi(&str[3]);
    else // Account for PDA-8
      size = atoi(&str[4]);
  } else {
    LOG(PANL_LOG,LOG_ERR, "Didn't understand panel type, '%.2s' from '%s' setting to size to RS-8\n",str,str);
    rs = true;
    size = 8;
  }

  size = setSizeMask(size);

  i=3;
  while(str[i] != ' ' && i < strlen(str)) {i++;}
  
  if (str[i+1] == 'O' || str[i+1] == 'o') {
    combo = false;
  } else if (str[i+1] == 'C' || str[i+1] == 'c') {
    combo = true;
  } else if (str[i+1] == 'D' || str[i+1] == 'd') {
    dual = true;
  } else {
    LOG(PANL_LOG,LOG_ERR, "Didn't understand panel type, '%s' from '%s' setting to Combo\n",&str[i+1],str);
    combo = true;
  }

  //setPanelSize(size, combo, dual, pda)
  setPanel(aqdata, rs, size, combo, dual);
}

aqkey *addVirtualButton(struct aqualinkdata *aqdata, char *label, int vindex) {
  if (aqdata->total_buttons + 1 >= TOTAL_BUTTONS) {
    return NULL;
  }
  if (aqdata->virtual_button_start <= 0) {
    aqdata->virtual_button_start = aqdata->total_buttons;
  }
  aqkey *button = &aqdata->aqbuttons[aqdata->total_buttons++];

  //aqdata->aqbuttons[index].led = ;
  //aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  //aqdata->aqbuttons[index].label = // copy label;
  //aqdata->aqbuttons[index].name = // aux_v?;  ? is vindex 
  //aqdata->aqbuttons[index].code = NUL;
  //aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  //aqdata->aqbuttons[index].special_mask = 0;

  //aqled *led = malloc(sizeof(aqled));
  //button->led = led;

  button->led = malloc(sizeof(aqled));
 
  char *name = malloc(sizeof(char*) * 10);
  snprintf(name, 9, "%s%d", BTN_VAUX, vindex);
  button->name = name; 

  if (label == NULL || strlen(label) <= 0) {
    //button->label = name; 
    setVirtualButtonLabel(button, name);
  } else {
    setVirtualButtonLabel(button, label);
  }
  /*
  if (strlen(label) <= 0) {
    button->label = name; 
  } else {
    button->label = label;
  }
  // These 3 vbuttons have a button code on iaqualink protocol, so use that for rssd_code.
  if (strncasecmp (button->label, "ALL OFF", 7) == 0) {
    button->rssd_code = IAQ_ALL_OFF;
  } else if (strncasecmp (button->label, "Spa Mode", 8) == 0) {
    button->rssd_code = IAQ_SPA_MODE;
  } else if (strncasecmp (button->label, "Clean Mode", 10) == 0) {
    button->rssd_code = IAQ_CLEAN_MODE;
  } else if (strncasecmp (button->label, "Day Party", 9) == 0) {
    button->rssd_code = IAQ_ONETOUCH_4;
  } else {
    button->rssd_code = NUL;
  }*/

  button->code = NUL;
  button->dz_idx = DZ_NULL_IDX;
  button->special_mask |= VIRTUAL_BUTTON; // Could change to special mask vbutton

  return button;  
}

bool setVirtualButtonLabel(aqkey *button, const char *label) {

  button->label = (char *)label;
  
  // These 3 vbuttons have a button code on iaqualink protocol, so use that for rssd_code.
  if (strncasecmp (button->label, "ALL OFF", 7) == 0) {
    button->rssd_code = IAQ_ALL_OFF;
  } else if (strncasecmp (button->label, "Spa Mode", 8) == 0) {
    button->rssd_code = IAQ_SPA_MODE;
  } else if (strncasecmp (button->label, "Clean Mode", 10) == 0) {
    button->rssd_code = IAQ_CLEAN_MODE;
  } else if (strncasecmp (button->label, "Day Party", 9) == 0) {
    button->rssd_code = IAQ_ONETOUCH_4;
  } else {
    button->rssd_code = NUL;
  }

  return true;
}

// So the 0-100% should be 600-3450 RPM and 15-130 GPM (ie 1% would = 600 & 0%=off)
// (value-600) / (3450-600) * 100   
// (value) / 100 * (3450-600) + 600

//{{ (((value | float(0) - %d) / %d) * 100) | int }} - minspeed, (maxspeed - minspeed),
//{{ ((value | float(0) / 100) * %d) + %d | int }} - (maxspeed - minspeed), minspeed)

int getPumpSpeedAsPercent(pump_detail *pump) {
  int pValue = pump->pumpType==VFPUMP?pump->gpm:pump->rpm;

  if (pValue < pump->minSpeed) {
      return 0;
  }

  // Below will return 0% if pump is at min, so return max (caculation, 1)
  return AQ_MAX( (int)((float)(pValue - pump->minSpeed) / (float)(pump->maxSpeed - pump->minSpeed) * 100) + 0.5, 1);
}

int convertPumpPercentToSpeed(pump_detail *pump, int pValue) {
  if (pValue >= 100)
    return pump->maxSpeed;
  else if (pValue <= 0)
    return pump->minSpeed;
  
  return ( ((float)(pValue / (float)100) * (pump->maxSpeed - pump->minSpeed) + pump->minSpeed)) + 0.5;
}

// 4,6,8,10,12,14
void initPanelButtons(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual) {

  // Since we are resetting all special buttons here (.special_mask), we need to clean out the lights and pumps.
  aqdata->num_lights = 0;
  aqdata->num_pumps = 0;

  int index = 0;
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[7-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_PUMP):BTN_PDA_PUMP;
  aqdata->aqbuttons[index].name = BTN_PUMP;
  aqdata->aqbuttons[index].code = KEY_PUMP;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_PUMP;
  index++;
  
  if (combo) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[6-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_SPA):BTN_PDA_SPA;
    aqdata->aqbuttons[index].name = BTN_SPA;
    aqdata->aqbuttons[index].code = KEY_SPA;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_SPA;
    index++;
  }
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX1):BTN_PDA_AUX1;
  aqdata->aqbuttons[index].name = BTN_AUX1;
  aqdata->aqbuttons[index].code = KEY_AUX1;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX1;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX2):BTN_PDA_AUX2;
  aqdata->aqbuttons[index].name = BTN_AUX2;
  aqdata->aqbuttons[index].code = KEY_AUX2;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX2;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX3):BTN_PDA_AUX3;
  aqdata->aqbuttons[index].name = BTN_AUX3;
  aqdata->aqbuttons[index].code = KEY_AUX3;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX3;
  index++;
  
  
  if (size >= 6) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[9-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX4):BTN_PDA_AUX4;
    aqdata->aqbuttons[index].name = BTN_AUX4;
    aqdata->aqbuttons[index].code = KEY_AUX4;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX4;
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[8-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX5):BTN_PDA_AUX5;
    aqdata->aqbuttons[index].name = BTN_AUX5;
    aqdata->aqbuttons[index].code = KEY_AUX5;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX5;
    index++; 
  }

  if (size >= 8) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX6):BTN_PDA_AUX6;
    aqdata->aqbuttons[index].name = BTN_AUX6;
    aqdata->aqbuttons[index].code = KEY_AUX6;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX6;
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX7):BTN_PDA_AUX7;
    aqdata->aqbuttons[index].name = BTN_AUX7;
    aqdata->aqbuttons[index].code = KEY_AUX7;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX7;
    index++;
  }
#ifdef AQ_RS16
  if (size >= 12) {// NSF This could be 10
    // AUX4 to AUX7 use different LED index & button key codes on RS12 & 16, so reset them
    aqdata->aqbuttons[index-4].led = &aqdata->aqualinkleds[2-1]; // Change
    aqdata->aqbuttons[index-4].code = KEY_RS16_AUX4;
    aqdata->aqbuttons[index-3].led = &aqdata->aqualinkleds[11-1]; // Change
    aqdata->aqbuttons[index-3].code = KEY_RS16_AUX5;
    aqdata->aqbuttons[index-2].led = &aqdata->aqualinkleds[10-1]; // Change
    aqdata->aqbuttons[index-2].code = KEY_RS16_AUX6;
    aqdata->aqbuttons[index-1].led = &aqdata->aqualinkleds[9-1]; // change
    aqdata->aqbuttons[index-1].code = KEY_RS16_AUX7;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[8-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB1); // AUX8
    aqdata->aqbuttons[index].name = BTN_AUXB1;
    aqdata->aqbuttons[index].code = KEY_AUXB1;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX8;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB2); // AUX9
    aqdata->aqbuttons[index].name = BTN_AUXB2;
    aqdata->aqbuttons[index].code = KEY_AUXB2;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX9;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB3);  // AUX10
    aqdata->aqbuttons[index].name = BTN_AUXB3;
    aqdata->aqbuttons[index].code = KEY_AUXB3;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX10;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[13-1];  
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB4);  // AUX11
    aqdata->aqbuttons[index].name = BTN_AUXB4;
    aqdata->aqbuttons[index].code = KEY_AUXB4;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX11;
    index++;
  }

  if (size >= 14) { // Actually RS 16 panel, but also 2/14 dual panel.
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[21-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF;  // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB5);
    aqdata->aqbuttons[index].name = BTN_AUXB5;
    aqdata->aqbuttons[index].code = KEY_AUXB5;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX12;
    index++;
 
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[22-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB6);
    aqdata->aqbuttons[index].name = BTN_AUXB6;
    aqdata->aqbuttons[index].code = KEY_AUXB6;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX13;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[23-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB7);
    aqdata->aqbuttons[index].name = BTN_AUXB7;
    aqdata->aqbuttons[index].code = KEY_AUXB7;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX14;
   index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[24-1]; // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB8);
    aqdata->aqbuttons[index].name = BTN_AUXB8;
    aqdata->aqbuttons[index].code = KEY_AUXB8;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX15;
    index++;
  }
#endif // AQ_RS16
  
  if (dual) {
    //Dual panel 2/6 has Aux6 so add that
    if (size == 6) {
      aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
      aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
      aqdata->aqbuttons[index].label = name2label(BTN_AUX6);
      aqdata->aqbuttons[index].name = BTN_AUX6;
      aqdata->aqbuttons[index].code = KEY_AUX6;
      aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
      aqdata->aqbuttons[index].special_mask = 0;
      aqdata->aqbuttons[index].rssd_code = RS_SA_AUX6;
      index++;
    }
    //Dual panels (2/10 & 2/14) have no AUX7, they go from AUX6 to AUXB1, but the keycodes are the same as other panels
    //i.e  Button AUX7 on normal panel is identical to AUX_B1 on Dual panel has same Keycode & LED bit but only the label is different.
    if (size > 6) {
      int i; // Dual panels are combo panels so we can start at index 8
      for(i=8; i < index; i++) {
        aqdata->aqbuttons[i].name = aqdata->aqbuttons[i+1].name;
        aqdata->aqbuttons[i].label = aqdata->aqbuttons[i+1].label;
      }
      index--;
    }
  }

  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[15-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(combo?BTN_POOL_HTR:BTN_TEMP1_HTR):BTN_PDA_POOL_HTR;
  aqdata->aqbuttons[index].name = BTN_POOL_HTR;
  aqdata->aqbuttons[index].code = KEY_POOL_HTR;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_POOLHT;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(combo?BTN_SPA_HTR:BTN_TEMP2_HTR):BTN_PDA_SPA_HTR;
  aqdata->aqbuttons[index].name = BTN_SPA_HTR;
  aqdata->aqbuttons[index].code = KEY_SPA_HTR;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_SPAHT;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_EXT_AUX):BTN_PDA_EXT_AUX;
  aqdata->aqbuttons[index].name = BTN_EXT_AUX;
  aqdata->aqbuttons[index].code = KEY_EXT_AUX;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;

  // Set the sizes for button index
  aqdata->total_buttons = index;
  aqdata->virtual_button_start = 0;

  #ifdef AQ_RS16
    aqdata->rs16_vbutton_start = 13 - (combo?0:1);
    aqdata->rs16_vbutton_end = 16 - (combo?0:1);
  #endif
  #ifdef AQ_PDA
    aqdata->pool_heater_index = index-3;
    aqdata->spa_heater_index = index-2;
    aqdata->solar_heater_index = index-1;

    // Reset all LED's to off since their is no off state in PDA.
    for(int i=0; i < aqdata->total_buttons; i++) {
        aqdata->aqbuttons[i].led->state = OFF;
    }

  #endif
}

const char* getRequestName(request_source source)
{
  switch(source) {
   case NET_MQTT:
      return "MQTT";
    break;
    case NET_API:
      return "API";
    break;
    case NET_WS:
      return "WebSocket";
    break;
    case NET_DZMQTT:
      return "Domoticz";
    break;
    case NET_TIMER:
      return "Timer";
    break;
    case UNACTION_TIMER:
      return "UnactionTimer";
    break;
  }

  static char buf[25];
  sprintf(buf, "Unknown %d", source);
  return buf;
}
const char* getActionName(action_type type)
{


  switch (type) {
    case ON_OFF:
      return "OnOff";
    break;
    case NO_ACTION:
      return "No Action";
    break;
    case POOL_HTR_SETPOINT:
      return "Pool Heater Setpoint";
    break;
    case SPA_HTR_SETPOINT:
      return "Spa Heater Setpoint";
    break;
    case CHILLER_SETPOINT:
      return "Chiller Setpoint";
    break;
    case FREEZE_SETPOINT:
      return "Freeze Protect Setpoint";
    break;
    case SWG_SETPOINT:
      return "SWG Percent";
    break;
    case SWG_BOOST:
      return "SWG Boost";
    break;
    case PUMP_RPM:
      return "VSP RPM/GPM";
    break;
    case PUMP_VSPROGRAM:
      return "VSP Program";
    break;
    case POOL_HTR_INCREMENT:
      return "Pool Heater Increment";
    break;
    case SPA_HTR_INCREMENT:
      return "Spa Heater Increment";
    break;
    case TIMER:
      return "Timer";
    break;
    case LIGHT_MODE:
      return "Light Mode";
    break;
    case DATE_TIME:
      return "Date Time";
    break;
    case LIGHT_BRIGHTNESS:
      return "Light Brightness";
    break;
  }

  static char buf[25];
  sprintf(buf, "Unknown %d", type);
  return buf;
}

//void create_PDA_on_off_request(aqkey *button, bool isON);
//bool create_panel_request(struct aqualinkdata *aqdata,  netRequest requester, int buttonIndex, int value, bool timer);
//void create_program_request(struct aqualinkdata *aqdata, netRequest requester, action_type type, int value, int id); // id is only valid for PUMP RPM

// Get Pool or Spa temp depending on what's on
int getWaterTemp(struct aqualinkdata *aqdata)
{
  if (isSINGLE_DEV_PANEL)
    return aqdata->pool_temp;

  // NSF Need to check if spa is on.
  if (aqdata->aqbuttons[1].led->state == OFF)
    return aqdata->pool_temp;
  else
    return aqdata->spa_temp;
}



//bool setDeviceState(aqkey *button, bool isON)
bool setDeviceState(struct aqualinkdata *aqdata, int deviceIndex, bool isON, request_source source)
{
  aqkey *button = &aqdata->aqbuttons[deviceIndex];

  //if ( button->special_mask & VIRTUAL_BUTTON  && button->special_mask & VS_PUMP) {
  if ( isVS_PUMP(button->special_mask) && isVBUTTON(button->special_mask)) {
    // Virtual Button with VSP is always on.
    LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', virtual pump is always on, ignoring", (isON == false ? "OFF" : "ON"), button->name);
    button->led->state = ON;
    return false;
  }

  if ((button->led->state == OFF && isON == false) ||
      (isON > 0 && (button->led->state == ON || button->led->state == FLASH ||
                     button->led->state == ENABLE))) {
    LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', already '%s', Ignoring\n", (isON == false ? "OFF" : "ON"), button->name, (isON == false ? "OFF" : "ON"));
    //return false;
  } else {
    LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', turning '%s'\n", (isON == false ? "OFF" : "ON"), button->name, (isON == false ? "OFF" : "ON"));
#ifdef AQ_PDA
    if (isPDA_PANEL) {
      if (button->special_mask & PROGRAM_LIGHT && isPDA_IAQT) {
        // AqualinkTouch in PDA mode, we can program light. (if turing off, use standard AQ_PDA_DEVICE_ON_OFF below)
        programDeviceLightMode(aqdata, (isON?0:-1), deviceIndex); // -1 means off 0 means use current light mode
      } else {
        // If we are using AqualinkTouch with iAqualink enabled, we can send button on/off much faster using that.
        if ( isPDA_IAQT && isIAQL_ACTIVE) {
          set_iaqualink_aux_state(button, isON);
        } else {
          char msg[PTHREAD_ARG];
          sprintf(msg, "%-5d%-5d", deviceIndex, (isON == false ? OFF : ON));
          aq_programmer(AQ_PDA_DEVICE_ON_OFF, msg, aqdata);
        }
      }
    } else
#endif
    {
      // Check for panel programmable light. if so simple ON isn't going to work well
      // Could also add "light mode" check, as this is only valid for panel configured light not aqualinkd configured light.
      if (isPLIGHT(button->special_mask) && button->led->state == OFF) {
        // OK Programable light, and no light mode selected. Now let's work out best way to turn it on. serial_adapter protocol will to it without questions,
        // all other will require programmig.
        if (isRSSA_ENABLED) {
          set_aqualink_rssadapter_aux_state(button, true);
        } else {
          //set_light_mode("0", deviceIndex); // 0 means use current light mode
          programDeviceLightMode(aqdata, 0, deviceIndex); // 0 means use current light mode
        }

        // If aqualinkd programmable light, it will come on at last state, so set that.
        if ( /*isPLIGHT(button->special_mask) &&*/ ((clight_detail *)button->special_mask_ptr)->lightType == LC_PROGRAMABLE ) {
          ((clight_detail *)button->special_mask_ptr)->currentValue = ((clight_detail *)button->special_mask_ptr)->lastValue;
        }
      } else if (isVBUTTON(button->special_mask)) {
        // Virtual buttons only supported with Aqualink Touch
        LOG(PANL_LOG, LOG_INFO, "Set state for Virtual Button %s code=0x%02hhx iAqualink2 enabled=%s\n",button->name, button->rssd_code, isIAQT_ENABLED?"Yes":"No");
        if (isIAQT_ENABLED) {
          // If it's one of the pre-defined onces & iaqualink is enabled, we can set it easile with button.
          
          if ( isIAQL_ACTIVE && button->rssd_code && button->rssd_code != NUL)
          {
            //LOG(PANL_LOG, LOG_NOTICE, "********** USE iaqualink2 ********\n");
            set_iaqualink_aux_state(button, isON);
          } else {
            char msg[PTHREAD_ARG];
            sprintf(msg, "%-5d%-5d", deviceIndex, (isON == false ? OFF : ON));
            //if (button->rssd_code != VBUTTON_RSSD) {
            //LOG(PANL_LOG, LOG_NOTICE, "********** USE AQ_SET_IAQTOUCH_DEVICE_ON_OFF ********\n");
              aq_programmer(AQ_SET_IAQTOUCH_DEVICE_ON_OFF, msg, aqdata);
            //} else if (button->rssd_code != VBUTTON_ONETOUCH_RSSD) {
            //  LOG(PANL_LOG, LOG_NOTICE, "********** USE AQ_SET_IAQTOUCH_ONETOUCH_ON_OFF ********\n");
            //  aq_programmer(AQ_SET_IAQTOUCH_ONETOUCH_ON_OFF, msg, aqdata);
            //} else {
            //  LOG(PANL_LOG, LOG_ERR, "Configuration! do not understand code for Virtual Buttons");
            //}
          } 
        } else {
          LOG(PANL_LOG, LOG_ERR, "Can only use Aqualink Touch protocol for Virtual Buttons");
        }
      } else if ( source == NET_DZMQTT && isRSSA_ENABLED ) {
        // Domoticz has a bad habbit of resending the same state back to us, when we use the PRESTATE_ONOFF option
        // since allbutton (default) is stateless, and rssaadapter is statefull, use rssaadapter for any domoricz requests
        set_aqualink_rssadapter_aux_state(button, isON);
      //} else if ( source == NET_TIMER && isRSSA_ENABLED ) {
        // Timer will sometimes send duplicate, so use RSSA since that protocol has on/off rather than toggle
      //  set_aqualink_rssadapter_aux_state(button, isON);
      } else if (isPLIGHT(button->special_mask) && isRSSA_ENABLED) {
        // If off and program light, use the RS serial adapter since that is overiding the state now.
        set_aqualink_rssadapter_aux_state(button, isON);
      } else {
        //set_iaqualink_aux_state(button, isON);
        //set_aqualink_rssadapter_aux_state(button, isON);
        aq_send_allb_cmd(button->code);
      }


      // If we turned off a aqualinkd programmable light, set the mode to 0
      if (isPLIGHT(button->special_mask) && ! isON && ((clight_detail *)button->special_mask_ptr)->lightType ==  LC_PROGRAMABLE ) {
        updateButtonLightProgram(aqdata, 0, deviceIndex);
      }

#ifdef CLIGHT_PANEL_FIX 
      if (isPLIGHT(button->special_mask) && isRSSA_ENABLED) {
        get_aqualink_rssadapter_button_status(button);
      }
#endif

// Pre set device to state, next status will correct if state didn't take, but this will stop multiple ON messages setting on/off
//#ifdef PRESTATE_ONOFF
      if (_aqconfig_.device_pre_state) {
        if ((button->code == KEY_POOL_HTR || button->code == KEY_SPA_HTR ||
             button->code == KEY_EXT_AUX) &&
            isON > 0) {
          button->led->state = ENABLE; // if heater and set to on, set pre-status to enable.
        //_aqualink_data->updated = true;
        } else if (isRSSA_ENABLED || ((button->special_mask & PROGRAM_LIGHT) != PROGRAM_LIGHT)) {
          button->led->state = (isON == false ? OFF : ON); // as long as it's not programmable light , pre-set to on/off
        //_aqualink_data->updated = true;
        }
      }
//#endif
    }
  }
  return TRUE;
}

bool programDeviceValue(struct aqualinkdata *aqdata, action_type type, int value, int id, bool expectMultiple) // id is only valid for PUMP RPM
{
  if (aqdata->unactioned.type != NO_ACTION && type != aqdata->unactioned.type)
    LOG(PANL_LOG,LOG_ERR, "about to overwrite unactioned panel program\n");

  if (type == POOL_HTR_SETPOINT || type == SPA_HTR_SETPOINT || type == FREEZE_SETPOINT || type == SWG_SETPOINT ) {
    aqdata->unactioned.value = setpoint_check(type, value, aqdata);
    if (value != aqdata->unactioned.value)
      LOG(PANL_LOG,LOG_NOTICE, "requested setpoint value %d is invalid, change to %d\n", value, aqdata->unactioned.value);
  } else if (type == CHILLER_SETPOINT) {
    if (isIAQT_ENABLED) {
      aqdata->unactioned.value = setpoint_check(type, value, aqdata);
      if (value != aqdata->unactioned.value)
        LOG(PANL_LOG,LOG_NOTICE, "requested setpoint value %d is invalid, change to %d\n", value, aqdata->unactioned.value);
    } else {
      LOG(PANL_LOG,LOG_ERR, "Chiller setpoint can only be set when `%s` is set to iAqualinkTouch procotol\n", CFG_N_extended_device_id);
      return false;
    }
  } else if (type == PUMP_RPM) {
    aqdata->unactioned.value = value;
  } else if (type == PUMP_VSPROGRAM) {
    LOG(PANL_LOG,LOG_ERR, "requested Pump vsp program is not implimented yet\n", value, aqdata->unactioned.value);
  } else {
    // SWG_BOOST & PUMP_RPM & SETPOINT incrment
    aqdata->unactioned.value = value;
  }

  aqdata->unactioned.type = type;
  aqdata->unactioned.id = id; // This is only valid for pump.

  // Should probably limit this to setpoint and no aq_serial protocol.
  if (expectMultiple) // We can get multiple MQTT requests from some, so this will wait for last one to come in.
    time(&aqdata->unactioned.requested);
  else
    aqdata->unactioned.requested = 0;

  return true;
}


void programDeviceLightBrightness(struct aqualinkdata *aqdata, int value, int button, bool expectMultiple) 
{
  clight_detail *light = getProgramableLight(aqdata, button);

  if (!isRSSA_ENABLED) {
    LOG(PANL_LOG,LOG_ERR, "Light mode brightness is only supported with `rssa_device_id` set\n");
    return;
  }

  if (light == NULL) {
    LOG(PANL_LOG,LOG_ERR, "Light mode control not configured for button %d\n",button);
    return;
  }

  // DIMMER is 0,25,50,100 DIMMER2 is range
  if (light->lightType == LC_DIMMER) {
    value = round(value / 25);
  }

  if (!expectMultiple) {
    programDeviceLightMode(aqdata, value, button);
    return;
  }

  time(&aqdata->unactioned.requested);
  aqdata->unactioned.value = value;
  aqdata->unactioned.type = LIGHT_MODE;
  aqdata->unactioned.id = button;

  return;
}


//void programDeviceLightMode(struct aqualinkdata *aqdata, char *value, int button) 
//void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int button) 
void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int button) 
{
#ifdef AQ_PDA
  if (isPDA_PANEL && !isPDA_IAQT) {
    LOG(PANL_LOG,LOG_ERR, "Light mode control not supported in PDA mode\n");
    return;
  }
#endif
  /*
  int i;
  clight_detail *light = NULL;
  for (i=0; i < aqdata->num_lights; i++) {
    if (&aqdata->aqbuttons[button] == aqdata->lights[i].button) {
      // Found the programmable light
      light = &aqdata->lights[i];
      break;
    }
  }*/

  clight_detail *light = getProgramableLight(aqdata, button);

  if (light == NULL) {
    LOG(PANL_LOG,LOG_ERR, "Light mode control not configured for button %d\n",button);
    return;
  }

  char buf[LIGHT_MODE_BUFER];

  if (light->lightType == LC_PROGRAMABLE ) {
    //sprintf(buf, "%-5s%-5d%-5d%-5d%.2f",value, 
    sprintf(buf, "%-5d%-5d%-5d%-5d%.2f",value, 
                                      button, 
                                      _aqconfig_.light_programming_initial_on,
                                      _aqconfig_.light_programming_initial_off,
                                      _aqconfig_.light_programming_mode );
    aq_programmer(AQ_SET_LIGHTPROGRAM_MODE, buf, aqdata);
  } else if (isRSSA_ENABLED) {
    // If we are using rs-serial then turn light on first.
    if (light->button->led->state != ON) {
      set_aqualink_rssadapter_aux_extended_state(light->button, RS_SA_ON);
    }
    if (light->lightType == LC_DIMMER) {
        // Value 1 = 25, 2 = 50, 3 = 75, 4 = 100 (need to convert value into binary)
      if (value >= 1 && value <= 4) {
        unsigned char rssd_value = value * 25;
        set_aqualink_rssadapter_aux_extended_state(light->button, rssd_value);
      } else {
        LOG(PANL_LOG,LOG_ERR, "Light mode %d is not valid for '%s'\n",value, light->button->label);
        set_aqualink_rssadapter_aux_extended_state(light->button, 100);
      }
    } else {
      // Dimmer or any color light can simply be set with value
      set_aqualink_rssadapter_aux_extended_state(light->button, value);
    }
  /*
  } else if (isRSSA_ENABLED && light->lightType == LC_DIMMER2) {
    // Dimmer needs to be turned on before you set dimmer level
    if (light->button->led->state != ON) {
      set_aqualink_rssadapter_aux_extended_state(light->button, RS_SA_ON);
    }
    set_aqualink_rssadapter_aux_extended_state(light->button, value);
  } else if (isRSSA_ENABLED && light->lightType == LC_DIMMER) {
    // Dimmer needs to be turned on first
    if (light->button->led->state != ON) {
      set_aqualink_rssadapter_aux_extended_state(light->button, RS_SA_ON);
    }
    // Value 1 = 25, 2 = 50, 3 = 75, 4 = 100 (need to convert value into binary)
    if (value >= 1 && value <= 4) {
      // If value is not on of those vales, then ignore
      unsigned char rssd_value = value * 25;
      set_aqualink_rssadapter_aux_extended_state(light->button, rssd_value);
    } else {
      LOG(PANL_LOG,LOG_ERR, "Light mode %d is not valid for '%s'\n",value, light->button->label);
    }
  } else if (isRSSA_ENABLED && light->lightType != LC_PROGRAMABLE) {
    // Any programmable COLOR light (that's programmed by panel)
    set_aqualink_rssadapter_aux_extended_state(light->button, value);*/
  } else {
    //sprintf(buf, "%-5s%-5d%-5d",value, button, light->lightType);
    sprintf(buf, "%-5d%-5d%-5d",value, button, light->lightType);
    aq_programmer(AQ_SET_LIGHTCOLOR_MODE, buf, aqdata);
  }

  // Use function so can be called from programming thread if we decide to in future.
  updateButtonLightProgram(aqdata, value, button);
}

/*
   deviceIndex = button index on button list (or pumpIndex for VSP action_types)
   value = value to set (0=off 1=on, or value for setpoints / rpm / timer) action_type will depend on this value 
   source = This will delay request to allow for multiple messages and only execute the last. (ie this stops multiple programming mode threads when setpoint changes are stepped)
*/
//bool panel_device_request(struct aqualinkdata *aqdata, action_type type, int deviceIndex, int value, int subIndex, bool fromMQTT)
bool panel_device_request(struct aqualinkdata *aqdata, action_type type, int deviceIndex, int value, request_source source)
{

  if (type == PUMP_RPM || type == PUMP_VSPROGRAM ){
    LOG(PANL_LOG,LOG_INFO, "Device request type '%s' for 'Pump#%d' of value %d from '%s'\n",
                          getActionName(type),
                          deviceIndex,
                          value,
                          getRequestName(source));
  } else {
    LOG(PANL_LOG,LOG_INFO, "Device request type '%s' for deviceindex %d '%s' of value %d from '%s'\n",
                          getActionName(type),
                          deviceIndex,aqdata->aqbuttons[deviceIndex].label, 
                          value, 
                          getRequestName(source));
  }

  switch (type) {
    case ON_OFF:
      //setDeviceState(&aqdata->aqbuttons[deviceIndex], value<=0?false:true, deviceIndex );
      setDeviceState(aqdata, deviceIndex, value<=0?false:true, source );
      // Clear timer if off request and timer is active
      if (value<=0 && (aqdata->aqbuttons[deviceIndex].special_mask & TIMER_ACTIVE) == TIMER_ACTIVE ) {
        //clear_timer(aqdata, &aqdata->aqbuttons[deviceIndex]);
        clear_timer(aqdata, deviceIndex);
      }
    break;
    case TIMER:
      //setDeviceState(&aqdata->aqbuttons[deviceIndex], true);
      setDeviceState(aqdata, deviceIndex, true, source);
      //start_timer(aqdata, &aqdata->aqbuttons[deviceIndex], deviceIndex, value);
      start_timer(aqdata, deviceIndex, value);
    break;
    case LIGHT_BRIGHTNESS:
      programDeviceLightBrightness(aqdata, value, deviceIndex, (source==NET_MQTT?true:false));
    break;
    case LIGHT_MODE:
      if (value <= 0) {
        // Consider this a bad/malformed request to turn the light off.
        panel_device_request(aqdata, ON_OFF, deviceIndex, 0, source);
      } else {
        programDeviceLightMode(aqdata, value, deviceIndex);
      }
    break;
    case POOL_HTR_SETPOINT:
    case SPA_HTR_SETPOINT:
    case CHILLER_SETPOINT:
    case FREEZE_SETPOINT:
    case SWG_SETPOINT:
    case SWG_BOOST:
    case PUMP_RPM:
    case PUMP_VSPROGRAM:
    case POOL_HTR_INCREMENT:
    case SPA_HTR_INCREMENT:
      programDeviceValue(aqdata, type, value, deviceIndex, (source==NET_MQTT?true:false) );
    break;
    case DATE_TIME:
      aq_programmer(AQ_SET_TIME, NULL, aqdata);
    break;
    default:
      LOG(PANL_LOG,LOG_ERR, "Unknown device request type %d for deviceindex %d\n",type,deviceIndex);
    break;
  }

  return TRUE;
}


// Programmable light has been updated, so update the status in AqualinkD
void updateButtonLightProgram(struct aqualinkdata *aqdata, int value, int button)
{
  /*
  int i;
  clight_detail *light = NULL;

  for (i=0; i < aqdata->num_lights; i++) {
    if (&aqdata->aqbuttons[button] == aqdata->lights[i].button) {
      // Found the programmable light
      light = &aqdata->lights[i];
      break;
    }
  }
  */
  clight_detail *light = getProgramableLight(aqdata, button);

  if (light == NULL) {
    LOG(PANL_LOG,LOG_ERR, "Button not found for light  button index=%d\n",button);
    return;
  }

  light->currentValue = value;
  if (value > 0)
    light->lastValue = value;
}

clight_detail *getProgramableLight(struct aqualinkdata *aqdata, int button) 
{
  if ( isPLIGHT(aqdata->aqbuttons[button].special_mask) ) {
    return (clight_detail *)aqdata->aqbuttons[button].special_mask_ptr;
  } 
  return NULL;
}

pump_detail *getPumpDetail(struct aqualinkdata *aqdata, int button)
{
  if ( isVS_PUMP(aqdata->aqbuttons[button].special_mask) ) {
    return (pump_detail *)aqdata->aqbuttons[button].special_mask_ptr;
  } 
  return NULL;
}

#ifdef DO_NOT_COMPILE


#ifdef AQ_RS16
void initButtons_RS16(struct aqualinkdata *aqdata);
#endif

/*
*  Link LED numbers to buttons, this is valid for RS8 and below, RS10 and above are different
* need to update this code in future.
*/

void initButtons(struct aqualinkdata *aqdata)
{

  aqdata->aqbuttons[0].led = &aqdata->aqualinkleds[7-1];
  aqdata->aqbuttons[0].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[0].label = name2label(BTN_PUMP);
  aqdata->aqbuttons[0].name = BTN_PUMP;
  aqdata->aqbuttons[0].code = KEY_PUMP;
  aqdata->aqbuttons[0].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[1].led = &aqdata->aqualinkleds[6-1];
  aqdata->aqbuttons[1].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[1].label = name2label(BTN_SPA);
  aqdata->aqbuttons[1].name = BTN_SPA;
  aqdata->aqbuttons[1].code = KEY_SPA;
  aqdata->aqbuttons[1].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[2].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[2].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[2].label = name2label(BTN_AUX1);
  aqdata->aqbuttons[2].name = BTN_AUX1;
  aqdata->aqbuttons[2].code = KEY_AUX1;
  aqdata->aqbuttons[2].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[3].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[3].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[3].label = name2label(BTN_AUX2);
  aqdata->aqbuttons[3].name = BTN_AUX2;
  aqdata->aqbuttons[3].code = KEY_AUX2;
  aqdata->aqbuttons[3].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[4].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[4].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[4].label = name2label(BTN_AUX3);
  aqdata->aqbuttons[4].name = BTN_AUX3;
  aqdata->aqbuttons[4].code = KEY_AUX3;
  aqdata->aqbuttons[4].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[5].led = &aqdata->aqualinkleds[9-1];
  aqdata->aqbuttons[5].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[5].label = name2label(BTN_AUX4);
  aqdata->aqbuttons[5].name = BTN_AUX4;
  aqdata->aqbuttons[5].code = KEY_AUX4;
  aqdata->aqbuttons[5].dz_idx = DZ_NULL_IDX;
 
  
  aqdata->aqbuttons[6].led = &aqdata->aqualinkleds[8-1];
  aqdata->aqbuttons[6].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[6].label = name2label(BTN_AUX5);
  aqdata->aqbuttons[6].name = BTN_AUX5;
  aqdata->aqbuttons[6].code = KEY_AUX5;
  aqdata->aqbuttons[6].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[7].led = &aqdata->aqualinkleds[12-1];
  aqdata->aqbuttons[7].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[7].label = name2label(BTN_AUX6);
  aqdata->aqbuttons[7].name = BTN_AUX6;
  aqdata->aqbuttons[7].code = KEY_AUX6;
  aqdata->aqbuttons[7].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[8].led = &aqdata->aqualinkleds[1-1];
  aqdata->aqbuttons[8].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[8].label = name2label(BTN_AUX7);
  aqdata->aqbuttons[8].name = BTN_AUX7;
  aqdata->aqbuttons[8].code = KEY_AUX7;
  aqdata->aqbuttons[8].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[9].led = &aqdata->aqualinkleds[15-1];
  aqdata->aqbuttons[9].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[9].label = name2label(BTN_POOL_HTR);
  aqdata->aqbuttons[9].name = BTN_POOL_HTR;
  aqdata->aqbuttons[9].code = KEY_POOL_HTR;
  aqdata->aqbuttons[9].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[10].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[10].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[10].label = name2label(BTN_SPA_HTR);
  aqdata->aqbuttons[10].name = BTN_SPA_HTR;
  aqdata->aqbuttons[10].code = KEY_SPA_HTR;
  aqdata->aqbuttons[10].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[11].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[11].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[11].label = name2label(BTN_SOLAR_HTR);
  aqdata->aqbuttons[11].name = BTN_SOLAR_HTR;
  aqdata->aqbuttons[11].code = KEY_SOLAR_HTR;
  aqdata->aqbuttons[11].dz_idx = DZ_NULL_IDX;
  
#ifdef AQ_PDA
  aqdata->aqbuttons[0].pda_label = BTN_PDA_PUMP;
  aqdata->aqbuttons[1].pda_label = BTN_PDA_SPA;
  aqdata->aqbuttons[2].pda_label = BTN_PDA_AUX1;
  aqdata->aqbuttons[3].pda_label = BTN_PDA_AUX2;
  aqdata->aqbuttons[4].pda_label = BTN_PDA_AUX3;
  aqdata->aqbuttons[5].pda_label = BTN_PDA_AUX4;
  aqdata->aqbuttons[6].pda_label = BTN_PDA_AUX5;
  aqdata->aqbuttons[7].pda_label = BTN_PDA_AUX6;
  aqdata->aqbuttons[7].pda_label = BTN_PDA_AUX6;
  aqdata->aqbuttons[8].pda_label = BTN_PDA_AUX7;
  aqdata->aqbuttons[9].pda_label = BTN_PDA_POOL_HTR;
  aqdata->aqbuttons[10].pda_label = BTN_PDA_SPA_HTR;
  aqdata->aqbuttons[11].pda_label = BTN_PDA_SOLAR_HTR;
#endif

}

#ifdef AQ_RS16

void initButtons_RS16(struct aqualinkdata *aqdata)
{
  // All buttons up to AUX4 are identical on all panels, so no need to change

  // AUX4 to AUX7 use different LED index & button key codes on RS12 & 16.
  aqdata->aqbuttons[5].led = &aqdata->aqualinkleds[2-1]; // Change
  aqdata->aqbuttons[5].code = KEY_RS16_AUX4;
  aqdata->aqbuttons[6].led = &aqdata->aqualinkleds[11-1]; // Change
  aqdata->aqbuttons[6].code = KEY_RS16_AUX5;
  aqdata->aqbuttons[7].led = &aqdata->aqualinkleds[10-1]; // Change
  aqdata->aqbuttons[7].code = KEY_RS16_AUX6;
  aqdata->aqbuttons[8].led = &aqdata->aqualinkleds[9-1]; // change
  aqdata->aqbuttons[8].code = KEY_RS16_AUX7;
  
  // AUX8 (B1) and beyone are either new or totally different on RS12 & 16

  aqdata->aqbuttons[9].led = &aqdata->aqualinkleds[8-1];
  aqdata->aqbuttons[9].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[9].label = name2label(BTN_AUXB1); // AUX8
  aqdata->aqbuttons[9].name = BTN_AUXB1;
  aqdata->aqbuttons[9].code = KEY_AUXB1;
  aqdata->aqbuttons[9].dz_idx = DZ_NULL_IDX;
 
  
  aqdata->aqbuttons[10].led = &aqdata->aqualinkleds[12-1];
  aqdata->aqbuttons[10].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[10].label = name2label(BTN_AUXB2); // AUX9
  aqdata->aqbuttons[10].name = BTN_AUXB2;
  aqdata->aqbuttons[10].code = KEY_AUXB2;
  aqdata->aqbuttons[10].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[11].led = &aqdata->aqualinkleds[1-1];
  aqdata->aqbuttons[11].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[11].label = name2label(BTN_AUXB3);  // AUX10
  aqdata->aqbuttons[11].name = BTN_AUXB3;
  aqdata->aqbuttons[11].code = KEY_AUXB3;
  aqdata->aqbuttons[11].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[12].led = &aqdata->aqualinkleds[13-1];
  aqdata->aqbuttons[12].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[12].label = name2label(BTN_AUXB4);  // AUX11
  aqdata->aqbuttons[12].name = BTN_AUXB4;
  aqdata->aqbuttons[12].code = KEY_AUXB4;
  aqdata->aqbuttons[12].dz_idx = DZ_NULL_IDX;
 
  
  aqdata->aqbuttons[13].led = &aqdata->aqualinkleds[21-1];  // doesn't actually exist
  aqdata->aqbuttons[13].led->state = OFF;  // Since there is no LED in data, set to off and allow messages to turn it on
  aqdata->aqbuttons[13].label = name2label(BTN_AUXB5);
  aqdata->aqbuttons[13].name = BTN_AUXB5;
  aqdata->aqbuttons[13].code = KEY_AUXB5;
  aqdata->aqbuttons[13].dz_idx = DZ_NULL_IDX;
 
  
  aqdata->aqbuttons[14].led = &aqdata->aqualinkleds[22-1];  // doesn't actually exist
  aqdata->aqbuttons[14].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
  aqdata->aqbuttons[14].label = name2label(BTN_AUXB6);
  aqdata->aqbuttons[14].name = BTN_AUXB6;
  aqdata->aqbuttons[14].code = KEY_AUXB6;
  aqdata->aqbuttons[14].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[15].led = &aqdata->aqualinkleds[23-1];  // doesn't actually exist
  aqdata->aqbuttons[15].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
  aqdata->aqbuttons[15].label = name2label(BTN_AUXB7);
  aqdata->aqbuttons[15].name = BTN_AUXB7;
  aqdata->aqbuttons[15].code = KEY_AUXB7;
  aqdata->aqbuttons[15].dz_idx = DZ_NULL_IDX;
 

  aqdata->aqbuttons[16].led = &aqdata->aqualinkleds[24-1]; // doesn't actually exist
  aqdata->aqbuttons[16].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
  aqdata->aqbuttons[16].label = name2label(BTN_AUXB8);
  aqdata->aqbuttons[16].name = BTN_AUXB8;
  aqdata->aqbuttons[16].code = KEY_AUXB8;
  aqdata->aqbuttons[16].dz_idx = DZ_NULL_IDX;
 

  aqdata->aqbuttons[17].led = &aqdata->aqualinkleds[15-1];
  aqdata->aqbuttons[17].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[17].label = name2label(BTN_POOL_HTR);
  aqdata->aqbuttons[17].name = BTN_POOL_HTR;
  aqdata->aqbuttons[17].code = KEY_POOL_HTR;
  aqdata->aqbuttons[17].dz_idx = DZ_NULL_IDX;
  
  
  aqdata->aqbuttons[18].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[18].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[18].label = name2label(BTN_SPA_HTR);
  aqdata->aqbuttons[18].name = BTN_SPA_HTR;
  aqdata->aqbuttons[18].code = KEY_SPA_HTR;
  aqdata->aqbuttons[18].dz_idx = DZ_NULL_IDX;
 
  
  aqdata->aqbuttons[19].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[19].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[19].label = name2label(BTN_SOLAR_HTR);
  aqdata->aqbuttons[19].name = BTN_SOLAR_HTR;
  aqdata->aqbuttons[19].code = KEY_SOLAR_HTR;
  aqdata->aqbuttons[19].dz_idx = DZ_NULL_IDX;
  

  #ifdef AQ_PDA
    aqdata->aqbuttons[9].pda_label = BTN_PDA_AUX1;
    aqdata->aqbuttons[10].pda_label = BTN_PDA_AUX2;
    aqdata->aqbuttons[11].pda_label = BTN_PDA_AUX3;
    aqdata->aqbuttons[12].pda_label = BTN_PDA_AUX4;
    aqdata->aqbuttons[13].pda_label = BTN_PDA_AUX5;
    aqdata->aqbuttons[14].pda_label = BTN_PDA_AUX6;
    aqdata->aqbuttons[15].pda_label = BTN_PDA_AUX7;
    aqdata->aqbuttons[16].pda_label = BTN_PDA_AUX7;
    aqdata->aqbuttons[17].pda_label = BTN_PDA_POOL_HTR;
    aqdata->aqbuttons[18].pda_label = BTN_PDA_SPA_HTR;
    aqdata->aqbuttons[19].pda_label = BTN_PDA_SOLAR_HTR;
  #endif
  
}

#endif













void initButtons_OLD_RS16(struct aqualinkdata *aqdata)
{
  aqdata->aqbuttons[0].led = &aqdata->aqualinkleds[7-1];
  aqdata->aqbuttons[0].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[0].label = name2label(BTN_PUMP);
  aqdata->aqbuttons[0].name = BTN_PUMP;
  aqdata->aqbuttons[0].code = KEY_PUMP;
  aqdata->aqbuttons[0].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[0].pda_label = BTN_PDA_PUMP;
  
  aqdata->aqbuttons[1].led = &aqdata->aqualinkleds[6-1];
  aqdata->aqbuttons[1].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[1].label = name2label(BTN_SPA);
  aqdata->aqbuttons[1].name = BTN_SPA;
  aqdata->aqbuttons[1].code = KEY_SPA;
  aqdata->aqbuttons[1].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[1].pda_label = BTN_PDA_SPA;
  
  aqdata->aqbuttons[2].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[2].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[2].label = name2label(BTN_AUX1);
  aqdata->aqbuttons[2].name = BTN_AUX1;
  aqdata->aqbuttons[2].code = KEY_AUX1;
  aqdata->aqbuttons[2].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[2].pda_label = BTN_PDA_AUX1;
  
  aqdata->aqbuttons[3].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[3].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[3].label = name2label(BTN_AUX2);
  aqdata->aqbuttons[3].name = BTN_AUX2;
  aqdata->aqbuttons[3].code = KEY_AUX2;
  aqdata->aqbuttons[3].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[3].pda_label = BTN_PDA_AUX2;
  
  aqdata->aqbuttons[4].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[4].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[4].label = BTN_AUX3;
  aqdata->aqbuttons[4].name = BTN_AUX3;
  aqdata->aqbuttons[4].code = KEY_AUX3;
  aqdata->aqbuttons[4].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[4].pda_label = BTN_PDA_AUX3;
  
  aqdata->aqbuttons[5].led = &aqdata->aqualinkleds[2-1]; // Change
  aqdata->aqbuttons[5].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[5].label = name2label(BTN_AUX4);
  aqdata->aqbuttons[5].name = BTN_AUX4;
  aqdata->aqbuttons[5].code = KEY_AUX4;
  aqdata->aqbuttons[5].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[5].pda_label = BTN_PDA_AUX4;
  
  aqdata->aqbuttons[6].led = &aqdata->aqualinkleds[11-1]; // Change
  aqdata->aqbuttons[6].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[6].label = name2label(BTN_AUX5);
  aqdata->aqbuttons[6].name = BTN_AUX5;
  aqdata->aqbuttons[6].code = KEY_AUX5;
  aqdata->aqbuttons[6].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[6].pda_label = BTN_PDA_AUX5;
  
  aqdata->aqbuttons[7].led = &aqdata->aqualinkleds[10-1]; // Change
  aqdata->aqbuttons[7].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[7].label = name2label(BTN_AUX6);
  aqdata->aqbuttons[7].name = BTN_AUX6;
  aqdata->aqbuttons[7].code = KEY_AUX6;
  aqdata->aqbuttons[7].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[7].pda_label = BTN_PDA_AUX6;
  
  aqdata->aqbuttons[8].led = &aqdata->aqualinkleds[9-1]; // change
  aqdata->aqbuttons[8].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[8].label = name2label(BTN_AUX7);
  aqdata->aqbuttons[8].name = BTN_AUX7;
  aqdata->aqbuttons[8].code = KEY_AUX7;
  aqdata->aqbuttons[8].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[8].pda_label = BTN_PDA_AUX7;





  aqdata->aqbuttons[9].led = &aqdata->aqualinkleds[8-1];
  aqdata->aqbuttons[9].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[9].label = name2label(BTN_AUXB1); // AUX8
  aqdata->aqbuttons[9].name = BTN_AUXB1;
  aqdata->aqbuttons[9].code = KEY_AUXB1;
  aqdata->aqbuttons[9].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[9].pda_label = BTN_PDA_AUX1;
  
  aqdata->aqbuttons[10].led = &aqdata->aqualinkleds[12-1];
  aqdata->aqbuttons[10].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[10].label = name2label(BTN_AUXB2); // AUX9
  aqdata->aqbuttons[10].name = BTN_AUXB2;
  aqdata->aqbuttons[10].code = KEY_AUXB2;
  aqdata->aqbuttons[10].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[10].pda_label = BTN_PDA_AUX2;
  
  aqdata->aqbuttons[11].led = &aqdata->aqualinkleds[1-1];
  aqdata->aqbuttons[11].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[11].label = name2label(BTN_AUXB3);  // AUX10
  aqdata->aqbuttons[11].name = BTN_AUXB3;
  aqdata->aqbuttons[11].code = KEY_AUXB3;
  aqdata->aqbuttons[11].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[11].pda_label = BTN_PDA_AUX3;
  
  aqdata->aqbuttons[12].led = &aqdata->aqualinkleds[13-1];
  aqdata->aqbuttons[12].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[12].label = name2label(BTN_AUXB4);  // AUX11
  aqdata->aqbuttons[12].name = BTN_AUXB4;
  aqdata->aqbuttons[12].code = KEY_AUXB4;
  aqdata->aqbuttons[12].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[12].pda_label = BTN_PDA_AUX4;
  
  aqdata->aqbuttons[13].led = &aqdata->aqualinkleds[21-1];  // doesn't actually exist
  aqdata->aqbuttons[13].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[13].label = name2label(BTN_AUXB5);
  aqdata->aqbuttons[13].name = BTN_AUXB5;
  aqdata->aqbuttons[13].code = KEY_AUXB5;
  aqdata->aqbuttons[13].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[13].pda_label = BTN_PDA_AUX5;
  
  aqdata->aqbuttons[14].led = &aqdata->aqualinkleds[22-1];  // doesn't actually exist
  aqdata->aqbuttons[14].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[14].label = name2label(BTN_AUXB6);
  aqdata->aqbuttons[14].name = BTN_AUXB6;
  aqdata->aqbuttons[14].code = KEY_AUXB6;
  aqdata->aqbuttons[14].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[14].pda_label = BTN_PDA_AUX6;
  
  aqdata->aqbuttons[15].led = &aqdata->aqualinkleds[23-1];  // doesn't actually exist
  aqdata->aqbuttons[15].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[15].label = name2label(BTN_AUXB7);
  aqdata->aqbuttons[15].name = BTN_AUXB7;
  aqdata->aqbuttons[15].code = KEY_AUXB7;
  aqdata->aqbuttons[15].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[15].pda_label = BTN_PDA_AUX7;

  aqdata->aqbuttons[16].led = &aqdata->aqualinkleds[24-1]; // doesn't actually exist
  aqdata->aqbuttons[16].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[16].label = name2label(BTN_AUXB8);
  aqdata->aqbuttons[16].name = BTN_AUXB8;
  aqdata->aqbuttons[16].code = KEY_AUXB8;
  aqdata->aqbuttons[16].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[16].pda_label = BTN_PDA_AUX7;






  
  aqdata->aqbuttons[17].led = &aqdata->aqualinkleds[15-1];
  aqdata->aqbuttons[17].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[17].label = name2label(BTN_POOL_HTR);
  aqdata->aqbuttons[17].name = BTN_POOL_HTR;
  aqdata->aqbuttons[17].code = KEY_POOL_HTR;
  aqdata->aqbuttons[17].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[17].pda_label = BTN_PDA_POOL_HTR;
  
  aqdata->aqbuttons[18].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[18].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[18].label = name2label(BTN_SPA_HTR);
  aqdata->aqbuttons[18].name = BTN_SPA_HTR;
  aqdata->aqbuttons[18].code = KEY_SPA_HTR;
  aqdata->aqbuttons[18].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[18].pda_label = BTN_PDA_SPA_HTR;
  
  aqdata->aqbuttons[19].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[19].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[19].label = name2label(BTN_SOLAR_HTR);
  aqdata->aqbuttons[19].name = BTN_SOLAR_HTR;
  aqdata->aqbuttons[19].code = KEY_SOLAR_HTR;
  aqdata->aqbuttons[19].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[19].pda_label = BTN_PDA_SOLAR_HTR;
  
}

#endif
