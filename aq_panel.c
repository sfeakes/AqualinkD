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
void setPanelString()
{
  snprintf(_panelString, sizeof(_panelString), "%s%s-%d %s%s%s",
      isRS_PANEL?"RS":"",
      isPDA_PANEL?"PDA":"", // No need for both of these, but for error validation leave it in.
      PANEL_SIZE(),
      isCOMBO_PANEL?"Combo Pool/Spa":"",
      isSINGLE_DEV_PANEL?"Pool/Spa Only":"",
      isDUAL_EQPT_PANEL?" Dual Equipment":"");
}
const char* getPanelString()
{
  return _panelString;
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

// 4,6,8,10,12,14
void initPanelButtons(struct aqualinkdata *aqdata, bool rs, int size, bool combo, bool dual) {

  int index = 0;
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[7-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_PUMP):BTN_PDA_PUMP;
  aqdata->aqbuttons[index].name = BTN_PUMP;
  aqdata->aqbuttons[index].code = KEY_PUMP;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;
  
  if (combo) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[6-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_SPA):BTN_PDA_SPA;
    aqdata->aqbuttons[index].name = BTN_SPA;
    aqdata->aqbuttons[index].code = KEY_SPA;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    index++;
  }
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX1):BTN_PDA_AUX1;
  aqdata->aqbuttons[index].name = BTN_AUX1;
  aqdata->aqbuttons[index].code = KEY_AUX1;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX2):BTN_PDA_AUX2;
  aqdata->aqbuttons[index].name = BTN_AUX2;
  aqdata->aqbuttons[index].code = KEY_AUX2;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX3):BTN_PDA_AUX3;
  aqdata->aqbuttons[index].name = BTN_AUX3;
  aqdata->aqbuttons[index].code = KEY_AUX3;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;
  
  
  if (size >= 6) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[9-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX4):BTN_PDA_AUX4;
    aqdata->aqbuttons[index].name = BTN_AUX4;
    aqdata->aqbuttons[index].code = KEY_AUX4;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[8-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX5):BTN_PDA_AUX5;
    aqdata->aqbuttons[index].name = BTN_AUX5;
    aqdata->aqbuttons[index].code = KEY_AUX5;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
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
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX7):BTN_PDA_AUX7;
    aqdata->aqbuttons[index].name = BTN_AUX7;
    aqdata->aqbuttons[index].code = KEY_AUX7;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
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
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB2); // AUX9
    aqdata->aqbuttons[index].name = BTN_AUXB2;
    aqdata->aqbuttons[index].code = KEY_AUXB2;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB3);  // AUX10
    aqdata->aqbuttons[index].name = BTN_AUXB3;
    aqdata->aqbuttons[index].code = KEY_AUXB3;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[13-1];  
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB4);  // AUX11
    aqdata->aqbuttons[index].name = BTN_AUXB4;
    aqdata->aqbuttons[index].code = KEY_AUXB4;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
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
    index++;
 
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[22-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB6);
    aqdata->aqbuttons[index].name = BTN_AUXB6;
    aqdata->aqbuttons[index].code = KEY_AUXB6;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[23-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB7);
    aqdata->aqbuttons[index].name = BTN_AUXB7;
    aqdata->aqbuttons[index].code = KEY_AUXB7;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
   index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[24-1]; // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB8);
    aqdata->aqbuttons[index].name = BTN_AUXB8;
    aqdata->aqbuttons[index].code = KEY_AUXB8;
    aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
    aqdata->aqbuttons[index].special_mask = 0;
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
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(combo?BTN_SPA_HTR:BTN_TEMP2_HTR):BTN_PDA_SPA_HTR;
  aqdata->aqbuttons[index].name = BTN_SPA_HTR;
  aqdata->aqbuttons[index].code = KEY_SPA_HTR;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_SOLAR_HTR):BTN_PDA_SOLAR_HTR;
  aqdata->aqbuttons[index].name = BTN_SOLAR_HTR;
  aqdata->aqbuttons[index].code = KEY_SOLAR_HTR;
  aqdata->aqbuttons[index].dz_idx = DZ_NULL_IDX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;

  aqdata->total_buttons = index;

  //aqdata->single_device = !combo;

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
    case POOL_HTR_SETOINT:
      return "Pool Heater Setpoint";
    break;
    case SPA_HTR_SETOINT:
      return "Spa Heater Setpoint";
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
  }

  static char buf[25];
  sprintf(buf, "Unknown %d", type);
  return buf;
}

//void create_PDA_on_off_request(aqkey *button, bool isON);
//bool create_panel_request(struct aqualinkdata *aqdata,  netRequest requester, int buttonIndex, int value, bool timer);
//void create_program_request(struct aqualinkdata *aqdata, netRequest requester, action_type type, int value, int id); // id is only valid for PUMP RPM


//bool setDeviceState(aqkey *button, bool isON)
bool setDeviceState(struct aqualinkdata *aqdata, int deviceIndex, bool isON, request_source source)
{
  aqkey *button = &aqdata->aqbuttons[deviceIndex];

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
        char msg[PTHREAD_ARG];
        sprintf(msg, "%-5d%-5d", deviceIndex, (isON == false ? OFF : ON));
        aq_programmer(AQ_PDA_DEVICE_ON_OFF, msg, aqdata);
      }
    } else
#endif
    {
      // Check for panel programmable light. if so simple ON isn't going to work well
      // Could also add "light mode" check, as this is only valid for panel configured light not aqualinkd configured light.
      if ((button->special_mask & PROGRAM_LIGHT) == PROGRAM_LIGHT && button->led->state == OFF) {
        // OK Programable light, and no light mode selected. Now let's work out best way to turn it on. serial_adapter protocol will to it without questions,
        // all other will require programmig.
        if (isRSSA_ENABLED) {
          set_aqualink_rssadapter_aux_state(deviceIndex, true);
        } else {
          //set_light_mode("0", deviceIndex); // 0 means use current light mode
          programDeviceLightMode(aqdata, 0, deviceIndex); // 0 means use current light mode
        }
      } else if ( source == NET_DZMQTT && isRSSA_ENABLED ) {
        // Domoticz has a bad habbit of resending the same state back to us, when we use the PRESTATE_ONOFF option
        // since allbutton (default) is stateless, and rssaadapter is statefull, use rssaadapter for any domoricz requests
        set_aqualink_rssadapter_aux_state(deviceIndex, isON);
      } else {
        aq_send_allb_cmd(button->code);
      }
// Pre set device to state, next status will correct if state didn't take, but this will stop multiple ON messages setting on/off
//#ifdef PRESTATE_ONOFF
      if (_aqconfig_.device_pre_state) {
        if ((button->code == KEY_POOL_HTR || button->code == KEY_SPA_HTR ||
             button->code == KEY_SOLAR_HTR) &&
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

  if (type == POOL_HTR_SETOINT || type == SPA_HTR_SETOINT || type == FREEZE_SETPOINT || type == SWG_SETPOINT ) {
    aqdata->unactioned.value = setpoint_check(type, value, aqdata);
    if (value != aqdata->unactioned.value)
      LOG(PANL_LOG,LOG_NOTICE, "requested setpoint value %d is invalid, change to %d\n", value, aqdata->unactioned.value);
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

//void programDeviceLightMode(struct aqualinkdata *aqdata, char *value, int button) 
void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int button) 
{
  int i;
  clight_detail *light = NULL;
#ifdef AQ_PDA
  if (isPDA_PANEL && !isPDA_IAQT) {
    LOG(PANL_LOG,LOG_ERR, "Light mode control not supported in PDA mode\n");
    return;
  }
#endif
  for (i=0; i < aqdata->num_lights; i++) {
    if (&aqdata->aqbuttons[button] == aqdata->lights[i].button) {
      // Found the programmable light
      light = &aqdata->lights[i];
      break;
    }
  }

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
  } else {
    //sprintf(buf, "%-5s%-5d%-5d",value, button, light->lightType);
    sprintf(buf, "%-5d%-5d%-5d",value, button, light->lightType);
    aq_programmer(AQ_SET_LIGHTCOLOR_MODE, buf, aqdata);
  }
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
    case LIGHT_MODE:
      programDeviceLightMode(aqdata, value, deviceIndex);
    break;
    case POOL_HTR_SETOINT:
    case SPA_HTR_SETOINT:
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