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
#include <ctype.h>

#define AQ_PANEL_C_
#include "rs_devices.h"
#include "config.h"
#include "aq_panel.h"
#include "serialadapter.h"
#include "aq_timer.h"
#include "allbutton_aq_programmer.h"
#include "rs_msg_utils.h"
#include "iaqualink.h"
#include "color_lights.h"


#define USE_LAST_VALUE 101

void initPanelButtons(struct aqualinkdata *aqdata, bool rspda, int size, bool combo, bool dual);

void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int button, bool expectMultiple, request_source source);
void programDeviceLightBrightness(struct aqualinkdata *aqdata, int value, int deviceIndex, bool expectMultiple, request_source source);
void programDeviceLightRGB(struct aqualinkdata *aqdata, int packed_rgb, int deviceIndex, request_source source);


void printPanelSupport(struct aqualinkdata *aqdata);
uint32_t setPanelSupport(struct aqualinkdata *aqdata);
uint16_t getPanelBitmaskFromName(const char *str);


void removePanelRSserialAdapterInterface();
void removePanelOneTouchInterface();
void removePanelIAQTouchInterface();

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

// Move to utils / rsm_string_utils.


/*
* Search string for consecutive numbers, ie 6520 (early revisions).  Return the char of first number and set out_length to length or numbers
*/
const char *count_consecutive_digits(const char *str, int length, int *out_len) {
    //int count = 0;
    char *sp = NULL;
    *out_len = 0;

    for (int i = 0; i < length; i++) {
        if (isdigit((unsigned char)str[i])) {
            if (sp == NULL) sp=(char *)str + i;
            *out_len += 1;
        } else if (*out_len > 0) {
            return sp;  // End of first digit run
        }
    }

    return sp; 
}

/*
* Search string for Letter then consecutive numbers, ie B0029221.  Return the letter and set out_length
*/
const char* find_letter_and_digits(const char *str, int length, int *out_len) {

    for (int i = 0; i < length; i++) {
        if (isalpha((unsigned char)str[i]) && isdigit((unsigned char)str[i+1]) ) {
            count_consecutive_digits((char*)&str[i+1], length-(i+1), out_len);
          if (*out_len >= 4) {
            *out_len += 1; // Count for the first letter.
            return &str[i];
          }
        }
    }
    const char *sp = count_consecutive_digits(str, length, out_len);
    if (*out_len >= 4 && sp != NULL) {
      return sp;
    } else {
      *out_len = 0;
    }

    return NULL;  // Pattern not found
}

/*
* Search for REV or REV. in string and return next char (missing space) 
*/
const char* find_rev_chars(const char *str, int length, int *out_len) {
    for (int i = 0; i < length; i++) {
      if ( str[i] == 'R' && str[i+1] == 'E' && str[i+2] == 'V' ) {
        i=i+3;
        while (str[i] == '.' || str[i] == ' ') i++;
        *out_len=i;
        // Could probably simply check for space here.
        while (isalpha(str[*out_len]) || isdigit(str[*out_len]) || str[*out_len] == '.') *out_len+=1;
        *out_len = *out_len - i;
        return str + i;
      } else if ( str[i] == 'P' && str[i+1] == 'D' && str[i+2] == 'A' && str[i+3] == ':') {
        i=i+4;
        while (str[i] == ':' || str[i] == ' ') i++;
        *out_len=i;
        while (isdigit(str[*out_len]) || str[*out_len] == '.') *out_len+=1;
        *out_len = *out_len - i;
        return str + i;
      }
    }

    return NULL;  // Pattern not found
}

void checkPanelConfig(struct aqualinkdata *aqdata) {

  // Check panel rev for common errors.

  // Aqualink Touch.
  if ( is_aqualink_touch_id(_aqconfig_.extended_device_id)) {
    if ( !isMASKSET(aqdata->panel_support_options, RSP_SUP_AQLT)) {
      LOG(PANL_LOG, LOG_ERR, "Panel REV %s does not support AqualinkTouch protocol, please change configuration option '%s'\n",aqdata->panel_rev, CFG_N_extended_device_id);
      LOG(PANL_LOG, LOG_WARNING, "Removing option '%s', please correct configuration\n",CFG_N_extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      removePanelIAQTouchInterface();
    }
  }

  // One Touch
  if ( is_onetouch_id(_aqconfig_.extended_device_id)) {
    if ( !isMASKSET(aqdata->panel_support_options, RSP_SUP_ONET)) {
      LOG(PANL_LOG, LOG_ERR, "Panel REV %s does not support OneTouch protocol, please change configuration option '%s'\n",aqdata->panel_rev, CFG_N_extended_device_id);
      LOG(PANL_LOG, LOG_WARNING, "Removing option '%s', please correct configuration\n",CFG_N_extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      removePanelOneTouchInterface();
    }
  }

  // Serial Adapter supported and set
  if ( is_rsserialadapter_id(_aqconfig_.rssa_device_id)) {
    if ( !isMASKSET(aqdata->panel_support_options, RSP_SUP_RSSA)) {
      LOG(PANL_LOG, LOG_ERR, "Panel REV %s does not support RS SerialAdapter protocol, please change configuration option '%s'\n",aqdata->panel_rev, CFG_N_rssa_device_id);
      LOG(PANL_LOG, LOG_WARNING, "Removing option '%s', please correct configuration\n",CFG_N_rssa_device_id);
      _aqconfig_.rssa_device_id = 0x00;
      removePanelRSserialAdapterInterface();
    }
  }


  if ( isMASKSET(aqdata->panel_support_options, RSP_SUP_RSSA)) {
    if ( ! is_rsserialadapter_id(_aqconfig_.rssa_device_id)) {
      LOG(PANL_LOG, LOG_WARNING, "Panel supports RS Serial Adapter, for better performance please set '%s=0x%02hhx' in config\n", CFG_N_rssa_device_id, RS_SERIAL_ADAPTER_MIN);
    }
  }

  // At present only need to set extended_device_id for certain things, no need to print warning as it only add to panel load
  // VSP / Chiller / VButtons etc
/* 
  if ( isMASKSET(aqdata->panel_support_options, RSP_SUP_AQLT)) {
    if ( ! is_aqualink_touch_id(_aqconfig_.extended_device_id)) {
      LOG(PANL_LOG, LOG_WARNING, "Panel supports iAqualink Touch, for better performance please set '%s' to an ID between 0x%02hhx & 0x%02hhx' in config\n", CFG_N_extended_device_id_programming, AQUALINKTOUCH_MIN, AQUALINKTOUCH_MAX);
    
    }
  } else if ( isMASKSET(aqdata->panel_support_options, RSP_SUP_ONET)) {
    if ( ! is_onetouch_id(_aqconfig_.extended_device_id) ) {
      LOG(PANL_LOG, LOG_WARNING, "Panel supports OneTouch, for better performance please set '%s' to an ID between 0x%02hhx & 0x%02hhx' in config\n", CFG_N_extended_device_id_programming, ONETOUCH_MIN, ONETOUCH_MAX);
    }
  }
*/

}

/*
pull board CPU, revision & panel string from strings like
   ' CPU p/n: B0029221'
   'B0029221 REV T.0.1'
   'E0260801 REV. O.2'
   '    REV. O.2    '
   'B0029221 REV T.0.1'
   '   REV T.0.1'
  'Control Panel version B0316823 REV Yg'
   '    REV. O.2    '
 '   6520 REV I   '
 '6520 REV I'
 'B0029221 REV T.0'
 'B0029221 REV T.0.1'
 'B0029222 REV T.2'
 'B0316823 REV Yg '
 'B0316823 REV Yg'
 'E0260801 REV. O.2'
 'AquaLink: REV T.0.1'
 'CPU p/n: B0029221'
 '. RS-6 Combo'
 '. PD-8 Only'

 ' PDA-PS4 Combo  '
 '  PPD: PDA 1.2  '

 PDA: 7.1.0
 PDA-P4 Only
*/

static inline bool isPanelRevKnown(const struct aqualinkdata *aqdata) {
  return aqdata->panel_rev[0] != '\0';
}

uint8_t setPanelInformationFromPanelMsg(struct aqualinkdata *aqdata, const char *input, uint8_t type, emulation_type source) {
    //const char *rev_pos = NULL;
    char *rev_pos = NULL;
    uint8_t rtn = 0;
//printf("Calculate panel from %s\n",input);
    //const char *rev_pos = strstr(input, "REV");  // Find the position of "REV"
    const char *sp;
    int length = 0;
    
    if ( isPanelRevKnown(aqdata) ) {
      // Check config, if it was set their, display warning if it doesn't match what the panel is reporting.
      if ( _aqconfig_.force_panel_rev == true ) {
        LOG(PANL_LOG, LOG_WARNING, "Config option '%s' was set to '%s', please delete line from config. The real value `%s` is being ignored\n", CFG_N_force_panel_rev, aqdata->panel_rev,input);
      }
    }

    LOG(PANL_LOG, LOG_DEBUG, "Decoding string '%s'\n",input);

    if (isMASK_SET(type, PANEL_REV)) {
      if (aqdata->panel_rev[0] == '\0') {

       // RS panels use REV Yg, some newer PDA panels use PDA: 7.x.x
        if ( (rev_pos = strstr(input, "REV")) == NULL) {  // Find the position of "REV"
          rev_pos = strstr(input, "PDA"); // Find the position of "PDA:"
          _aqconfig_.paneltype_mask |= RSP_PDA; // Set PDA type so we don't confuse versions
          _aqconfig_.paneltype_mask &= ~RSP_RS;
        }
        if ( rev_pos != NULL) {  // If found Find the position of "REV"
          length = 0;
          sp = find_rev_chars(rev_pos, strlen(input) - (rev_pos - input), &length);

          if (length>0 && sp != NULL) {
            strncpy(aqdata->panel_rev, sp, length);
            aqdata->panel_rev[length] = '\0';
            setMASK(rtn, PANEL_REV);
            LOG(PANL_LOG, LOG_NOTICE, "Panel REV %s from %s\n",aqdata->panel_rev,getJandyDeviceName(source));
            setPanelSupport(aqdata);
            printPanelSupport(aqdata);
            if (source == SIM_NONE) { 
              // We pass SIM_NONE when we are in auto_config mode, so reset the panel name so we get it again when we fully start
              aqdata->panel_rev[0] = '\0';
            } else {
               checkPanelConfig(aqdata);
            }
          } else {
            //printf("Failed to find rev or version #\n");
          }
        } else {
          //printf("Failed to find REV string, null\n");
        }
      } else {
        // Already set
      }
    }

    if (isMASK_SET(type, PANEL_CPU)) {
      if (aqdata->panel_cpu[0] == '\0') {
        if (rev_pos == NULL)
          sp = find_letter_and_digits(input, strlen(input), &length);
        else
          sp = find_letter_and_digits(input, (rev_pos - input), &length);
       
        if (length>0 && sp != NULL) {
            strncpy(aqdata->panel_cpu, sp, length);
            aqdata->panel_cpu[length] = '\0';
            setMASK(rtn,PANEL_CPU);
            LOG(PANL_LOG, LOG_NOTICE, "Panel CPU %s from %s\n",aqdata->panel_cpu,getJandyDeviceName(source));
        } else {
            //printf("Failed to find CPU\n");
        }
      } else {
        // already set
      }
    }

    if (isMASK_SET(type, PANEL_STRING)) {
      if (aqdata->panel_string[0] == '\0') {
      // Find first RS or PD letters
        sp = NULL;
        length = strlen(input);
        for (int i=0; i < length; i++) {
          if ( (input[i] == 'R' && input[i+1] == 'S') ||
               (input[i] == 'P' && input[i+1] == 'D')) 
          {
            sp = &input[i];
            break;
          }
        }
        
        if ( sp != NULL && *sp == 'P' ){
          // If PDA make sure to search for - as we can get 'PDA: 7.1.0' and 'PDA-P4 Only' (need to ignore version)
          if ( strstr(input, "-") == NULL ) {sp = NULL;}
        }

        if (sp != NULL) {
          // Strip trailing whitespace
          for(length=strlen(sp)-1; isspace(sp[length]); length--);
          length++;
          strncpy(aqdata->panel_string, sp, length);
          aqdata->panel_string[length] = '\0';
          setMASK(rtn,PANEL_STRING);
          LOG(PANL_LOG, LOG_NOTICE, "Panel %s from %s\n",aqdata->panel_string,getJandyDeviceName(source));
          if (source == SIM_NONE) { 
            // We pass SIM_NONE when we are in auto_config mode,so re-set the actual panel size
            setPanelByName(aqdata, aqdata->panel_string);
          } else {
            // Should check panel.
            uint16_t panelMask = getPanelBitmaskFromName(aqdata->panel_string);
            
            if ( (panelMask & PANEL_COMPARISON_MASK) != (_aqconfig_.paneltype_mask & PANEL_COMPARISON_MASK) ) {
              //printf("******** Config mismatch = %s vs %s\n",aqdata->panel_string, getPanelString());
              LOG(PANL_LOG, LOG_ERR, "Panel type mismatch, Panel returned = '%s', AqualinkD Config = '%s'\n",aqdata->panel_string, getPanelString());
              //PRINT_SET_BITS(panelMask);
              //PRINT_SET_BITS(_aqconfig_.paneltype_mask);
            } else {/*
              printf("******** Config mismatch = %s vs %s\n",aqdata->panel_string, getPanelString());
              PRINT_SET_BITS(panelMask);
              PRINT_SET_BITS(_aqconfig_.paneltype_mask);
              printf("Compared\n");
              PRINT_SET_BITS((panelMask & PANEL_COMPARISON_MASK));
              PRINT_SET_BITS((_aqconfig_.paneltype_mask & PANEL_COMPARISON_MASK));*/
            }
          }
        } else {
          // ERROR not in string.
          //LOG(PANL_LOG, LOG_ERR, "Did not understand panel string '%s'\n",input);
        }
      } else {
        //already set
      }
    }

    aqdata->is_dirty = true;
    return rtn;
}


uint16_t setPDAPanelSupport(struct aqualinkdata *aqdata)
{
  //LOG(PANL_LOG,LOG_NOTICE, "PDA Panel revision '%s' unknown support options\n", aqdata->panel_rev);

  if (aqdata->panel_rev[0] >= 49){ // 1 in ascii
    //aqdata->panel_support_options
  }
  if (aqdata->panel_rev[0] >= 50){ // 2 in ascii
    //aqdata->panel_support_options
  }  
  if (aqdata->panel_rev[0] >= 51){ // 3 in ascii
    aqdata->panel_support_options |= RSP_SUP_HPCHIL;
  }
  if (aqdata->panel_rev[0] >= 52){ // 4 in ascii
    aqdata->panel_support_options |= RSP_SUP_VSP;
  }
  if (aqdata->panel_rev[0] >= 53){ // 5 in ascii
    aqdata->panel_support_options |= RSP_SUP_CHEM;
  }
  if (aqdata->panel_rev[0] >= 54){ // 6 in ascii
    aqdata->panel_support_options  |= RSP_SUP_AQLT;
  }
  if (aqdata->panel_rev[0] >= 55){ // 7 in ascii
    //aqdata->panel_support_options
  }

  return aqdata->panel_support_options;
}

uint32_t setPanelSupport(struct aqualinkdata *aqdata)
{
  aqdata->panel_support_options = 0;

  if (!isalpha(aqdata->panel_rev[0]))
  {
    if (isPDA_PANEL)
    {
      return setPDAPanelSupport(aqdata);
    }
    else
    {
      LOG(PANL_LOG, LOG_WARNING, "Panel revision is not understood '%s', please report this issue\n", aqdata->panel_rev);
    }
  }

  // Rev >= F Dimmer.  But need serial protocol so set to I
  // Rev >= H (Think this was first RS485)
  // Rev >= HH - (some panels support serial adapter, some don't)
  // Rev >= I Serial Adapter.
  // Rev >= I One Touch protocol
  // Rev >= L JandyColors Smart Light Control
  // Rev >= L PC Dock / (Support stopped around REV Y)
  // Rev >= L SWG control for both Pool & Spa (not just one)
  // Rev >= M AquaPalm (PDA) - even in MMM this was not usefull
  // Rev >= MMM = 12V JandyColor Lights (also light dimmer)
  // Rev >= N Hayward ColorLogic LED Light
  // Rev >= N VersaTemp heatpump & chiller
  // Rev >= O Variable Speed Pump
  // Rev >- O One Touch (VSP) has different wat to display info vs REV T (Between REV O & T not sure when exact changed )
  // Rev >= O.1 == Jandy WaterColors LED ( 9 colors )
  // Rev >= O.2 ==
  // Rev >= P ChemLink (Chem feeder / replaced with TrueDose )
  // Rev >= Q Aqualink Touch protocol
  // Rev >= R iAqualink (wifi adapter) protocol
  // Rev >= S
  // Rev >= T.0.1 == limited color light
  // Rev >= T.2 == more color lights
  // Rev >= U
  // Rev >= V
  // Rev >= W pump label (not number)
  // Rev >= W iAqualink 3.0 (I think.  ie can set VSP rpm / SWG / Chill setpoint over the protocol )
  // Rev >= X
  // Rev >= Xg
  // Rev >= Y TruSense Water Chemistry Analyzer
  // Rev >= Yg Virtual Device called Label Auxiliraries

  const char rev = aqdata->panel_rev[0];
  const char rev2 = aqdata->panel_rev[1];

  if (rev >= 'I')
  {
    aqdata->panel_support_options |= RSP_SUP_ONET;
    aqdata->panel_support_options |= RSP_SUP_RSSA;
    aqdata->panel_support_options |= RSP_SUP_SWG;
  }

  if (rev >= 'I') // dimmer came out in F, but we use the serial adapter to set, so use that as support
    aqdata->panel_support_options |= RSP_SUP_DLIT;

  if (rev >= 'L')
  {
    aqdata->panel_support_options |= RSP_SUP_CLIT;
    aqdata->panel_support_options |= RSP_SUP_PCDOC;
    aqdata->panel_support_options |= RSP_SUP_SWG_PS;
  }

  if (rev >= 'N')
    aqdata->panel_support_options |= RSP_SUP_HPCHIL;

  if (rev >= 'O')
    aqdata->panel_support_options |= RSP_SUP_VSP;

  if (rev == 'O') // VERY SPECIFIC: OneTouch uses a different menu for VSP.
    aqdata->panel_support_options |= RSP_SUP_ONET_EARLY;

  if (rev >= 'P')
    aqdata->panel_support_options |= RSP_SUP_CHEM;

  if (rev >= 'Q')
    aqdata->panel_support_options |= RSP_SUP_AQLT;

  if (rev >= 'R')
    aqdata->panel_support_options |= RSP_SUP_IAQL;

  if (rev >= 'T')
    aqdata->panel_support_options |= RSP_SUP_CLIT_RSSA;

  if (rev >= 'W')
  {
    aqdata->panel_support_options |= RSP_SUP_PLAB;
    aqdata->panel_support_options |= RSP_SUP_IAQL3;
  }

  if (rev > 'Y' || (rev == 'Y' && rev2 >= 'g'))
  { // Y.g or later
    aqdata->panel_support_options |= RSP_SUP_VBTN;
    aqdata->panel_support_options |= RSP_SUP_TSCHEM;
    aqdata->panel_support_options &= ~RSP_SUP_PCDOC;
  }

  // if (REV[0] > 84 || (REV[0] == 84 && REV[1] == 64 && REV[2] >= 50) ) // T in ascii (or T and . and 2 )
  //   supported |= RSP_SUP_CLIT4;
    
  //}

  return aqdata->panel_support_options;
}

static const struct { uint32_t mask; const char *name; } s_rsp_sup_names[] = {
  { RSP_SUP_ONET,       "One Touch" },
  { RSP_SUP_AQLT,       "Aqualink Touch" },
  { RSP_SUP_ONET_EARLY, "One Touch (Early)" },
  { RSP_SUP_IAQL,       "iAqualink 1.0/2.0" },
  { RSP_SUP_IAQL3,      "iAqualink 3.0" },
  { RSP_SUP_RSSA,       "RS Serial Adapter" },
  { RSP_SUP_VSP,        "Variable Speed Pumps" },
  { RSP_SUP_CHEM,       "Chemical feeder" },
  { RSP_SUP_TSCHEM,     "True Sense Chemical Reader" },
  { RSP_SUP_SWG,        "Salt Water Generator" },
  { RSP_SUP_CLIT,       "Color Lights" },
  { RSP_SUP_DLIT,       "Dimmable Lights" },
  { RSP_SUP_VBTN,       "Virtual Button" },
  { RSP_SUP_PLAB,       "Variable Speed Pump (By Label & extended ID)" },
  { RSP_SUP_HPCHIL,     "Heat Pump / Chiller" },
  { RSP_SUP_PCDOC,      "PC Dock" },
  { RSP_SUP_CLIT_RSSA,  "Color Lights over Serial Adapter" },
  { RSP_SUP_SWG_PS,     "SWG setpoints for both pool and spa" },
};

void printPanelSupport(struct aqualinkdata *aqdata) {
  for (size_t i = 0; i < sizeof(s_rsp_sup_names) / sizeof(s_rsp_sup_names[0]); i++) {
    if (isMASK_SET(aqdata->panel_support_options, s_rsp_sup_names[i].mask))
      LOG(PANL_LOG, LOG_DEBUG, "Panel supports: %s\n", s_rsp_sup_names[i].name);
  }
}


panel_support_t doesPanelSupport(const struct aqualinkdata *aqdata, uint32_t mask)
{
  if (aqdata->panel_support_options == 0) {
    return RSP_UNKNOWN;
  }
  return (aqdata->panel_support_options & mask) == mask ? RSP_SUPPORTED : RSP_NOT_SUPPORTED;
}

/*
void printPanelSupport(struct aqualinkdata *aqdata) {
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_ONET )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: One Touch\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_AQLT )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Aqualink Touch\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_ONET_EARLY )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: One Touch (Early)\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_IAQL )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: iAqualink 1.0/2.0\n");
  }
   if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_IAQL3 )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: iAqualink 3.0\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_RSSA )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: RS Serial Adapter\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_VSP )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Variable Speed Pumps\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_CHEM )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Chemical feeder\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_TSCHEM )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: True Sense Chemical Reader\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_SWG )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Salt Water Generator\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_CLIT )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Color Lights\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_DLIT )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Dimmable Lights\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_VBTN )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Virtual Button\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_PLAB )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Variable Speed Pump (By Label & extended ID)\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_HPCHIL )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: Heat Pump / Chiller\n");
  }
  if (isMASK_SET(aqdata->panel_support_options, RSP_SUP_PCDOC )) {
    LOG(PANL_LOG,LOG_DEBUG, "Panel supports: PC Dock\n");
  }
}
*/



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

void removePanelRSserialAdapterInterface() {
  _aqconfig_.paneltype_mask &= ~RSP_RSSA;
}
void removePanelOneTouchInterface() {
  _aqconfig_.paneltype_mask &= ~RSP_ONET;
}
void removePanelIAQTouchInterface() {
  _aqconfig_.paneltype_mask &= ~RSP_IAQT;
}

int pSizeFromMask(uint16_t mask) {
  if ((mask & RSP_4) == RSP_4)
    return 4;
  else if ((mask & RSP_6) == RSP_6)
    return 6;
  else if ((mask & RSP_8) == RSP_8)
    return 8;
  else if ((mask & RSP_12) == RSP_12)
    return 10;
  else if ((mask & RSP_10) == RSP_10)
    return 12;
  else if ((mask & RSP_14) == RSP_14)
    return 14;
  else if ((mask & RSP_16) == RSP_16)
    return 16;
  
  LOG(PANL_LOG,LOG_ERR, "Internal error, panel size not set, using 8\n");
  return 8;
}

int PANEL_SIZE() {
  return pSizeFromMask(_aqconfig_.paneltype_mask);
  /*
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
  */
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
    combo = true;
    _aqconfig_.paneltype_mask |= RSP_DUAL_EQPT;
    _aqconfig_.paneltype_mask |= RSP_COMBO;
    _aqconfig_.paneltype_mask &= ~RSP_SINGLE;
  }

  initPanelButtons(aqdata, rs, nsize, combo, dual);
  
  setPanelString();
}

uint16_t getPanelBitmaskFromName(const char *str)
{
  uint16_t paneltype_mask = 0;
  int i;
  int size = 0;
  //bool rs = true;
  //bool combo = true;
  //bool dual = false;
  //int16_t panelbits = 0;
  
  if (str[0] == 'R' && str[1] == 'S') { // RS Panel
    paneltype_mask |= RSP_RS;
    if (str[4] == '/')
      size = atoi(&str[5]);
    else
      size = atoi(&str[3]);
  } else if (str[0] == 'P' && str[1] == 'D') { // PDA Panel
    paneltype_mask |= RSP_PDA;
    //printf("Char at 0=%c 1=%c 2=%c 3=%c 4=%c 5=%d 6=%c\n", str[0], str[1], str[2], str[3], str[4], str[5], str[6]);
    if (str[2] == '-' || str[2] == ' ') // Account for PD-8
      size = atoi(&str[3]);
    else if (str[3] == '-' && str[4] == 'P' && str[5] == 'S') // PDA-PS4 Combo
      size = atoi(&str[6]);
    else if (str[3] == '-' && str[4] == 'P') // PDA-P6 Only
      size = atoi(&str[5]);
    else // Account for PDA-8
      size = atoi(&str[4]);
  } else {
    LOG(PANL_LOG,LOG_ERR, "Didn't understand panel type, '%.2s' from '%s' setting to size to RS-8\n",str,str);
  }

  switch (size) {
    case 4:
      paneltype_mask |= RSP_4;
    break;
    case 6:
      paneltype_mask |= RSP_6;
    break;
    case 8:
      paneltype_mask |= RSP_8;
    break;
    case 10:
      paneltype_mask |= RSP_10;
    break;
    case 12:
      paneltype_mask |= RSP_12;
    break;
    case 14:
      paneltype_mask |= RSP_14;
    break;
    case 16:
      paneltype_mask |= RSP_16;
    break;
    default:
      LOG(PANL_LOG,LOG_ERR, "Didn't understand panel size, '%d' setting to size to 8\n",size);
    break;
  }

  i=3;
  while(str[i] != ' ' && i < strlen(str)) {i++;}
  
  if (str[i+1] == 'O' || str[i+1] == 'o') {
    paneltype_mask |= RSP_SINGLE;
  } else if (str[i+1] == 'C' || str[i+1] == 'c') {
    paneltype_mask |= RSP_COMBO;
  } else if (str[i+1] == 'D' || str[i+1] == 'd') {
    paneltype_mask |= RSP_DUAL_EQPT;
  } else {
    LOG(PANL_LOG,LOG_ERR, "Didn't understand panel type, '%s' from '%s' setting to Combo\n",&str[i+1],str);
  }

  return paneltype_mask;
}

void setPanelByName(struct aqualinkdata *aqdata, const char *str)
{
  /*
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
    //printf("Char at 0=%c 1=%c 2=%c 3=%c 4=%c 5=%d 6=%c\n", str[0], str[1], str[2], str[3], str[4], str[5], str[6]);
    if (str[2] == '-' || str[2] == ' ') // Account for PD-8
      size = atoi(&str[3]);
    else if (str[3] == '-' && str[4] == 'P' && str[5] == 'S') // PDA-PS4 Combo
      size = atoi(&str[6]);
    else if (str[3] == '-' && str[4] == 'P') // PDA-P6 Only
      size = atoi(&str[5]);
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
  */

  uint16_t panelMask = getPanelBitmaskFromName(str);

  setPanel(aqdata,
          ((panelMask & RSP_RS) == RSP_RS),
          pSizeFromMask(panelMask),
          ((panelMask & RSP_COMBO) == RSP_COMBO),
          ((panelMask & RSP_DUAL_EQPT) == RSP_DUAL_EQPT));

}

aqkey *addVirtualButton(struct aqualinkdata *aqdata, char *label, int vindex) {
  if (aqdata->total_buttons /*+ 1*/ >= TOTAL_BUTTONS) {
    return NULL;
  }

  int index = vindex;

  
  // vindex 0 means find first index.
  if (vindex == 0) {
    //printf(" TOTAL=%d VSTART=%d\n",aqdata->total_buttons,aqdata->virtual_button_start);
    //index = aqdata->total_buttons - aqdata->virtual_button_start + 1;
    if (aqdata->virtual_button_start <= 0) {
      // Their are no vbuttons, so start at 1.
      index = 1;
    } else {
      index = aqdata->total_buttons - aqdata->virtual_button_start + 1;
    }
    //printf("TOTAL=%d VSTART=%d INDEXNAME=%d\n",aqdata->total_buttons,aqdata->virtual_button_start,index);
  }
  

  if (aqdata->virtual_button_start <= 0) {
    aqdata->virtual_button_start = aqdata->total_buttons;
  }
  aqkey *button = &aqdata->aqbuttons[aqdata->total_buttons++];

  button->led = malloc(sizeof(aqled));
 
  char *name = malloc(sizeof(char*) * 10);
  snprintf(name, 9, "%s%d", BTN_VAUX, index);
  button->name = name;

  // NSF THIS NEEDS TO BE REMOVED
  //button->special_mask_ptr = malloc(sizeof(altlabel_detail));
  //((altlabel_detail *)button->special_mask_ptr)->altlabel = NUL;

  if (label == NULL || strlen(label) <= 0) {
    //button->label = name; 
    setVirtualButtonLabel(button, name);
  } else {
    setVirtualButtonLabel(button, label);
  }

  button->code = NUL;
  setButtonSpecialMask(button, VIRTUAL_BUTTON);
  //button->special_mask |= VIRTUAL_BUTTON; // Could change to special mask vbutton
  button->led->state = OFF;

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


bool setVirtualButtonAltLabel(aqkey *button, char *label) {
  if (label == NULL )
    return false;

  if (! isMASK_SET(button->special_mask, VIRTUAL_BUTTON_ALT_LABEL) ) {
    button->special_mask_ptr = malloc(sizeof(altlabel_detail));
    ((altlabel_detail *)button->special_mask_ptr)->altlabel = cleanalloc(label);
    setButtonSpecialMask(button, VIRTUAL_BUTTON_ALT_LABEL);
  }

  ((altlabel_detail *)button->special_mask_ptr)->altlabel = (char *)label;
  ((altlabel_detail *)button->special_mask_ptr)->in_alt_mode = false;
  
  //setButtonSpecialMask(button, VIRTUAL_BUTTON_ALT_LABEL);
  //button->special_mask |= VIRTUAL_BUTTON_ALT_LABEL;

  return true;
}


int getPumpDefaultSpeed(pump_detail *pump, bool max)
{
  if (pump == NULL)
    return AQ_UNKNOWN;

  if (max)
    return pump->pumpType==VFPUMP?PUMP_GPM_MAX:PUMP_RPM_MAX;
  else
    return pump->pumpType==VFPUMP?PUMP_GPM_MIN:PUMP_RPM_MIN;
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
  aqdata->aqbuttons[index].label = rs?name2label(BTN_PUMP):cleanalloc(BTN_PDA_PUMP);
  aqdata->aqbuttons[index].name = BTN_PUMP;
  aqdata->aqbuttons[index].code = KEY_PUMP;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_PUMP;
  index++;
  
  if (combo) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[6-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_SPA):cleanalloc(BTN_PDA_SPA);
    aqdata->aqbuttons[index].name = BTN_SPA;
    aqdata->aqbuttons[index].code = KEY_SPA;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_SPA;
    index++;
  }
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX1):cleanalloc(BTN_PDA_AUX1);
  aqdata->aqbuttons[index].name = BTN_AUX1;
  aqdata->aqbuttons[index].code = KEY_AUX1;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX1;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX2):cleanalloc(BTN_PDA_AUX2);
  aqdata->aqbuttons[index].name = BTN_AUX2;
  aqdata->aqbuttons[index].code = KEY_AUX2;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX2;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX3):cleanalloc(BTN_PDA_AUX3);
  aqdata->aqbuttons[index].name = BTN_AUX3;
  aqdata->aqbuttons[index].code = KEY_AUX3;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_AUX3;
  index++;
  
  
  if (size >= 6) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[9-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX4):cleanalloc(BTN_PDA_AUX4);
    aqdata->aqbuttons[index].name = BTN_AUX4;
    aqdata->aqbuttons[index].code = KEY_AUX4;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX4;
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[8-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX5):cleanalloc(BTN_PDA_AUX5);
    aqdata->aqbuttons[index].name = BTN_AUX5;
    aqdata->aqbuttons[index].code = KEY_AUX5;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX5;
    index++; 
  }

  if (size >= 8) {
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX6):cleanalloc(BTN_PDA_AUX6);
    aqdata->aqbuttons[index].name = BTN_AUX6;
    aqdata->aqbuttons[index].code = KEY_AUX6;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX6;
    index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = rs?name2label(BTN_AUX7):cleanalloc(BTN_PDA_AUX7);
    aqdata->aqbuttons[index].name = BTN_AUX7;
    aqdata->aqbuttons[index].code = KEY_AUX7;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX7;
    index++;
  }

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
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX8;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB2); // AUX9
    aqdata->aqbuttons[index].name = BTN_AUXB2;
    aqdata->aqbuttons[index].code = KEY_AUXB2;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX9;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[1-1];
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB3);  // AUX10
    aqdata->aqbuttons[index].name = BTN_AUXB3;
    aqdata->aqbuttons[index].code = KEY_AUXB3;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX10;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[13-1];  
    aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB4);  // AUX11
    aqdata->aqbuttons[index].name = BTN_AUXB4;
    aqdata->aqbuttons[index].code = KEY_AUXB4;
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
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX12;
    index++;
 
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[22-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB6);
    aqdata->aqbuttons[index].name = BTN_AUXB6;
    aqdata->aqbuttons[index].code = KEY_AUXB6;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX13;
    index++;
  
    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[23-1];  // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB7);
    aqdata->aqbuttons[index].name = BTN_AUXB7;
    aqdata->aqbuttons[index].code = KEY_AUXB7;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX14;
   index++;

    aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[24-1]; // doesn't actually exist
    aqdata->aqbuttons[index].led->state = OFF; // Since there is no LED in data, set to off and allow messages to turn it on
    aqdata->aqbuttons[index].label = name2label(BTN_AUXB8);
    aqdata->aqbuttons[index].name = BTN_AUXB8;
    aqdata->aqbuttons[index].code = KEY_AUXB8;
    aqdata->aqbuttons[index].special_mask = 0;
    aqdata->aqbuttons[index].rssd_code = RS_SA_AUX15;
    index++;
  }

  
  if (dual) {
    //Dual panel 2/6 has Aux6 so add that
    if (size == 6) {
      aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[12-1];
      aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
      aqdata->aqbuttons[index].label = name2label(BTN_AUX6);
      aqdata->aqbuttons[index].name = BTN_AUX6;
      aqdata->aqbuttons[index].code = KEY_AUX6;
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
  aqdata->aqbuttons[index].label = rs?name2label(combo?BTN_POOL_HTR:BTN_TEMP1_HTR):cleanalloc(BTN_PDA_POOL_HTR);
  aqdata->aqbuttons[index].name = BTN_POOL_HTR;
  aqdata->aqbuttons[index].code = KEY_POOL_HTR;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_POOLHT;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(combo?BTN_SPA_HTR:BTN_TEMP2_HTR):cleanalloc(BTN_PDA_SPA_HTR);
  aqdata->aqbuttons[index].name = BTN_SPA_HTR;
  aqdata->aqbuttons[index].code = KEY_SPA_HTR;
  aqdata->aqbuttons[index].special_mask = 0;
  aqdata->aqbuttons[index].rssd_code = RS_SA_SPAHT;
  index++;
  
  aqdata->aqbuttons[index].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[index].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[index].label = rs?name2label(BTN_EXT_AUX):cleanalloc(BTN_PDA_EXT_AUX);
  aqdata->aqbuttons[index].name = BTN_EXT_AUX;
  aqdata->aqbuttons[index].code = KEY_EXT_AUX;
  aqdata->aqbuttons[index].special_mask = 0;
  index++;

  // Set the sizes for button index
  aqdata->total_buttons = index;
  aqdata->virtual_button_start = 0;

  aqdata->rs16_vbutton_start = 13 - (combo?0:1);
  aqdata->rs16_vbutton_end = 16 - (combo?0:1);

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
    case TIMER_SEC:
      return "Timer (Seconds)";
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
    case LIGHT_RGB:
      return "Light RGB";
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



/*
   Basic programming that are not too involved, ie no Jandy bugs to overcome or aq_programmer can make decision on protocol to use.
   Simply set them all as unactioned, and let the delayed_request code pick them up and pass to aq_programmer 
*/
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

  if (type == PUMP_RPM || type == PUMP_VSPROGRAM) {
    for (int i=0; i < aqdata->num_pumps; i++) {
      if (aqdata->pumps[i].pumpIndex == value) {
        if (aqdata->pumps[i].pumpType == PT_UNKNOWN) {
          LOG(ONET_LOG,LOG_ERR, "Can't set Pump RPM/GPM until type is known\n");
        }
        aqdata->unactioned.button = aqdata->pumps[i].button;
        break;
      }
    }
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


bool setDeviceState(struct aqualinkdata *aqdata, int deviceIndex, bool isON, request_source source)
{
  aqkey *button = &aqdata->aqbuttons[deviceIndex];
  bool set_pre_state = true;

  /* NSF in future Might limit to Rev Yg - Allow a Vbutton that's a pump to be turned on/off.
  if ( isVS_PUMP(button->special_mask) && isVBUTTON(button->special_mask)) {
    // Virtual Button with VSP is always on.
    LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', virtual pump is always on, ignoring", (isON == false ? "OFF" : "ON"), button->name);
    button->led->state = ON;
    return false;
  }
  */

  if ((button->led->state == OFF && isON == false) ||
      (isON > 0 && (button->led->state == ON || button->led->state == FLASH ||
                     button->led->state == ENABLE))) {
    LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', already '%s', Ignoring\n", (isON == false ? "OFF" : "ON"), button->name, (isON == false ? "OFF" : "ON"));
    return false;
  }
    
  LOG(PANL_LOG, LOG_INFO, "received '%s' for '%s', turning '%s'\n", (isON == false ? "OFF" : "ON"), button->name, (isON == false ? "OFF" : "ON"));

  if ( button->runtime_sec > 0) {
      LOG(PANL_LOG, LOG_INFO, "'%s' is has runtime %u seconds, setting timer\n", button->name, button->runtime_sec);
      start_timer(aqdata, deviceIndex, 0, button->runtime_sec);
  }

  #ifdef AQ_PDA
  if (isPDA_PANEL) {
     set_pre_state = false;
      if (button->special_mask & PROGRAM_LIGHT/* && isPDA_IAQT*/) {
        //if ( isPDA_IAQT && isIAQL_ACTIVE) {
        // AqualinkTouch in PDA mode, we can program light. (if turing off, use standard AQ_PDA_DEVICE_ON_OFF below)
        //  programDeviceLightMode(aqdata, (isON?USE_LAST_VALUE:0), deviceIndex, (source==NET_MQTT?true:false), source);
        //} else {
          aq_programmer(AQ_SET_LIGHTCOLOR_MODE, button, (isON == false ? 0 : 4), true, aqdata);
        //}
      } else {
        // If we are using AqualinkTouch with iAqualink enabled, we can send button on/off much faster using that.
        if ( isPDA_IAQT && isIAQL_ACTIVE) {
          set_iaqualink_aux_state(button, isON);
        } else {
          aq_programmer(AQ_PDA_DEVICE_ON_OFF, button, (isON == false ? OFF : ON), deviceIndex, aqdata);
        }
      }
  } else
#endif
  {
    if (isPLIGHT(button->special_mask)) {    
        //programDeviceLightMode_(aqdata, (isON?USE_LAST_VALUE:0), deviceIndex ,(source==NET_MQTT?true:false), source);
        // NSF we could let programDeviceLightMode() handle ALL these cases.

        if (isMASK_SET(button->special_mask, VIRTUAL_BUTTON)) {
          // No quick way to turn on or off and virtual button light.
          programDeviceLightMode(aqdata, (isON?USE_LAST_VALUE:0), deviceIndex ,(source==NET_MQTT?true:false), source);
        }
        else if ( ((clight_detail *)button->special_mask_ptr)->lightType == LC_PROGRAMABLE ) {
          LOG(PANL_LOG,LOG_DEBUG, "Turning light %s %s with allbutton key\n", button->label, isON?"On":"Off");
          aq_send_allb_cmd(button->code);
          if (isON) { // Will come back on in old state.
            updateButtonLightProgram(aqdata, ((clight_detail *)button->special_mask_ptr)->lastValue, deviceIndex);
          } else {
            updateButtonLightProgram(aqdata, 0, deviceIndex);
          }
          set_pre_state = false;
        } else if ( ((clight_detail *)button->special_mask_ptr)->lightType == LC_DIMMER2 ||
                    ((clight_detail *)button->special_mask_ptr)->lightType == LC_DIMMER) {
          if (isON) { // Dimmer light can get stuck turning on from RSSA, so use allbutton
            LOG(PANL_LOG,LOG_DEBUG, "Turning light %s On with allbutton dimmer program\n", button->label);
            aq_program(AQ_SET_ALLB_LIGHTDIMMER, button, 4, true, aqdata); // 4 = 100% since it uses light mode name
            set_pre_state = false;
          } else {
            LOG(PANL_LOG,LOG_DEBUG, "Turning light %s Off with allbutton key\n", button->label);
            aq_send_allb_cmd(button->code);
          }
        } else if (isRSSA_ENABLED) {
          LOG(PANL_LOG,LOG_DEBUG, "Turning light %s %s with all RS serial on/off key\n", button->label, isON?"On":"Off");
          set_aqualink_rssadapter_aux_state(button, isON);
        } else {
          if (isON) {
            LOG(PANL_LOG,LOG_DEBUG, "Turning light %s On with allbutton light mode program\n", button->label);
            aq_programmer(AQ_SET_LIGHTCOLOR_MODE, button, 1, true, aqdata);
            set_pre_state = false;
          } else {
            LOG(PANL_LOG,LOG_DEBUG, "Turning light %s Off with allbutton key\n", button->label);
            aq_send_allb_cmd(button->code);
          }
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
            aq_programmer(AQ_SET_IAQTOUCH_DEVICE_ON_OFF, button, (isON == false ? OFF : ON), deviceIndex, aqdata);
            set_pre_state = false;
          }
        } else if (isONET_ENABLED){
          aq_programmer(AQ_SET_ONETOUCH_MACRO, button, (isON == false ? OFF : ON), deviceIndex, aqdata);
          set_pre_state = false;
        } else {
          LOG(PANL_LOG, LOG_ERR, "Can only use Aqualink Touch for Virtual Buttons");
        }
    } else {
        // Everything else, simply send the button code.
        //set_iaqualink_aux_state(button, isON);
        //set_aqualink_rssadapter_aux_state(button, isON);
      aq_send_allb_cmd(button->code);
    }
  }

#ifdef CLIGHT_PANEL_FIX 
  if (isPLIGHT(button->special_mask) && isRSSA_ENABLED) {
    get_aqualink_rssadapter_button_status(button);
  }
#endif

// Pre set device to state, next status will correct if state didn't take, but this will stop multiple ON messages setting on/off
//#ifdef PRESTATE_ONOFF
  if (_aqconfig_.device_pre_state && set_pre_state) {
        if ((button->code == KEY_POOL_HTR || button->code == KEY_SPA_HTR ||
             button->code == KEY_EXT_AUX) &&
            isON > 0) {
          button->led->state = ENABLE; // if heater and set to on, set pre-status to enable.
          LOG(PANL_LOG, LOG_INFO, "Pre-set state of %s to enable\n",button->label);
        //_aqualink_data->updated = true;
        //} else if (isRSSA_ENABLED || ((button->special_mask & PROGRAM_LIGHT) != PROGRAM_LIGHT)) {
        } else if (isRSSA_ENABLED || !isPLIGHT(button->special_mask)) {
          button->led->state = (isON == false ? OFF : ON); // as long as it's not programmable light , pre-set to on/off
          LOG(PANL_LOG, LOG_INFO, "Pre-set state of %s to %s\n",button->label,(isON == false ? "Off" : "On"));
        //_aqualink_data->updated = true;
        }
  }
//#endif
    
  
  return TRUE;
}


/*
   value 0 = off
   value 101/USE_LAST_VALUE = On use default mode if you can
   value is % for LC_DIMMER and LC_DIMMER2
*/
void programDeviceLightBrightness(struct aqualinkdata *aqdata, int value, int deviceIndex, bool expectMultiple, request_source source)
{
  //int extra_value = false;
  
  clight_detail *light = getProgramableLight(aqdata, deviceIndex);

  if (value < 0 || value > 100) {
    LOG(PANL_LOG,LOG_ERR, "Dimmer value %d is not valid, using %d\n",value,AQ_CLAMP(value,0,100));
    value = AQ_CLAMP(value,0,100);
  }

  if (light->lightType == LC_DIMMER && value !=0 && value != 25&& value != 50 && value != 75 && value != 100) {
    // Make sure we have 0/25/50/75 for LC_DIMMER
    LOG(PANL_LOG,LOG_DEBUG, "Dimmer value %d is not valid, rounding to nearest 25!\n",value);
    value = dimmer_mode_to_percent(dimmer_percent_to_mode_index(value));
  }
    
  if (expectMultiple) {
    // Queue up a request, this will call us back through with expectMultiple=false
    time(&aqdata->unactioned.requested);
    aqdata->unactioned.value = value;
    aqdata->unactioned.type = LIGHT_BRIGHTNESS;
    aqdata->unactioned.id = deviceIndex;
    return;
  }

  if  (light == NULL || (light->lightType != LC_DIMMER2 && light->lightType != LC_DIMMER && light->lightType != LC_JANDYINFINATE)) {
    LOG(PANL_LOG,LOG_ERR, "Can not set light brightness on device '%s'\n",aqdata->aqbuttons[deviceIndex].label);
    return;
  }

  if (!isRSSA_ENABLED && light->lightType == LC_DIMMER2) {
    LOG(PANL_LOG,LOG_ERR, "Light mode brightness 11 is only supported when `rssa_device_id` is set\n");
    return;
  }

  if (light->lightType == LC_JANDYINFINATE) {
    if (!isIAQL_ACTIVE) {
      LOG(PANL_LOG, LOG_ERR, "Brightness for Jandy Infinite light requires iAqualink (enable_iaqualink=yes)\n");
      return;
    }
    aq_programmer(AQ_SET_IAQLINK_LIGHT_BRIGHTNESS, light->button, value, 0, aqdata);
    return;
  }

  if (isPDA_PANEL) {
    aq_programmer(AQ_SET_LIGHTCOLOR_MODE, light->button, dimmer_percent_to_mode_index(value), false, aqdata);
    return;
  }

  if (value == 0) {
    // We simply need to turn the light off at this point, so use allbutton key as it's the quickest.
    // but can't turn off a virtual light.
    if (light->button->led->state == ON && !isMASK_SET(light->button->special_mask, VIRTUAL_BUTTON)) {
      LOG(PANL_LOG,LOG_DEBUG, "Turning light %s Off with allbutton key\n", light->button->label);
      aq_send_allb_cmd(light->button->code);
      // Could also check isRSSA_ENABLED and use set_aqualink_rssadapter_aux_state(light->button, FALSE);
      return;
    } else if (light->button->led->state != ON ) {
      LOG(PANL_LOG,LOG_WARNING, "Request to turn Light brightness '%s' to 0, already off, ignoring!\n",light->button->label);
      return;
    }
  }

  /*
    logic.   
    set brightness through RSSA.  (RSSA only protocol supports full dimmer range)
    RSSA has bug / issue. Sometimes panel will lock device forcing a panel delete/re-add of light.
    to overcome this seems to be turn it on with allbutton, if it's on then use RSSA to set the %.

    if off turn on with allbutton.

  */

  
  if (aqdata->aqbuttons[deviceIndex].led->state == ON) {
    // Light is on, Simple set brightness through RSSD if we can
    if (isRSSA_ENABLED) {
      LOG(PANL_LOG,LOG_DEBUG, "Turning light %s to %d with RS Serial extended state\n", light->button->label, value);
      set_aqualink_rssadapter_aux_extended_state(light->button, value + RSSD_COLOR_LIGHT_OFFSET_WRITE);
    } else {
      LOG(PANL_LOG,LOG_DEBUG, "Turning light %s to %d with allbutton dimmer program\n", light->button->label, value);
      aq_program(AQ_SET_ALLB_LIGHTDIMMER, &aqdata->aqbuttons[deviceIndex], dimmer_percent_to_mode_index(value), false, aqdata);
    }
  } else {
    // Light is off, turn on with allbutton then reset the %.
    if (value == USE_LAST_VALUE) {
      // 101 is used last value, param #4 of true in ap_program means use default value if can, otherwise use param #3 = 4 = 100%
      LOG(PANL_LOG,LOG_DEBUG, "Turning light %s to Last mode with allbutton dimmer program\n", light->button->label);
      aq_program(AQ_SET_ALLB_LIGHTDIMMER, &aqdata->aqbuttons[deviceIndex], 4, true, aqdata); // 4 = 100% since it uses light mode name
    } else {
      int calVal = dimmer_percent_to_mode_index(value);
      LOG(PANL_LOG,LOG_INFO, "Rounded dimmer value to %d for on command\n",calVal * 25);
      LOG(PANL_LOG,LOG_DEBUG, "Turning light %s to %d with allbutton dimmer program\n", light->button->label, calVal);
      aq_program(AQ_SET_ALLB_LIGHTDIMMER, &aqdata->aqbuttons[deviceIndex], calVal, false, aqdata);
      if (value != 25 && value !=50 && value !=75 && value != 100 && isRSSA_ENABLED) {
        // Setup the rssd to set the light to the right value. 
        time(&aqdata->unactioned.requested);
        aqdata->unactioned.requested += 5; // This should give enough time for the allbutton to finish
        aqdata->unactioned.value = value;
        aqdata->unactioned.type = LIGHT_BRIGHTNESS;
        aqdata->unactioned.id = deviceIndex;
      }
      
    }
  }

  return;
}

void programDeviceLightRGB(struct aqualinkdata *aqdata, int packed_rgb, int deviceIndex, request_source source)
{
  clight_detail *light = getProgramableLight(aqdata, deviceIndex);

  if (light == NULL || light->lightType != LC_JANDYINFINATE) {
    LOG(PANL_LOG, LOG_ERR, "RGB color only supported on Jandy Infinite light\n");
    return;
  }

  if (!isIAQL_ACTIVE) {
    LOG(PANL_LOG, LOG_ERR, "RGB for Jandy Infinite light requires iAqualink (enable_iaqualink=yes)\n");
    return;
  }

  aq_programmer(AQ_SET_IAQLINK_LIGHT_RGB, light->button, packed_rgb, 0, aqdata);

  light->red   = (packed_rgb >> 16) & 0xFF;
  light->green = (packed_rgb >>  8) & 0xFF;
  light->blue  =  packed_rgb        & 0xFF;
}

/*
   value 0 = off
   value 101/USE_LAST_VALUE = On use default mode if you can
*/
void programDeviceLightMode(struct aqualinkdata *aqdata, int value, int deviceIndex, bool expectMultiple, request_source source)
{
  int extra_value=false;
  /*. We should never get multiple requests for mode, only brightness uses slider.  But leave here incase we need it in the future
  if (expectMultiple) {
    // Queue up a request, this will call us back through with expectMultiple=false
    time(&aqdata->unactioned.requested);
    aqdata->unactioned.value = value;
    aqdata->unactioned.type = LIGHT_MODE;
    aqdata->unactioned.id = deviceIndex;
    return;
  }
  */

  clight_detail *light = getProgramableLight(aqdata, deviceIndex);

  if (light == NULL) {
    LOG(PANL_LOG,LOG_ERR, "Light mode control not configured for button %d\n", deviceIndex);
    return;
  }

  if (value != USE_LAST_VALUE && ! is_valid_light_mode(light->lightType, value)) {
    LOG(PANL_LOG,LOG_ERR, "Light mode '%d' is not valid for light '%s', %s\n", value, lightTypeName(light->lightType), light->button->label);
    return;
  }

  if (light->lightType == LC_DIMMER2 || light->lightType == LC_DIMMER) {// DIMMER
    programDeviceLightBrightness(aqdata, (light->lightType== LC_DIMMER?dimmer_mode_to_percent(value):value), deviceIndex, expectMultiple, source);
    return;
  }

  // If it's a PDA panel,
  if (isPDA_PANEL) {
    if (value == USE_LAST_VALUE) {
      extra_value = true;
      value = 1;
    }
    aq_programmer(AQ_SET_LIGHTCOLOR_MODE, light->button, value, extra_value, aqdata);
    return;
  }

  // Turn on a light with no mode set.
  if (value == USE_LAST_VALUE) {
    extra_value = true;
    value = 1; // Do we need to reset this?
    // Anything but VIRTUAL_BUTTON we can turn on with no mode simply.
    if (!isMASK_SET(light->button->special_mask, VIRTUAL_BUTTON)) {
      // for LC_PROGRAMMABLE is a simple ON command, so send it and be done.
      if (light->lightType == LC_PROGRAMABLE) {
        aq_send_allb_cmd(light->button->code);
        light->currentValue = light->lastValue; // will come back on in last mode, so set that.
      // easiest way is to turn on with RSSA then no questions asked.
      } else if (isRSSA_ENABLED) {
        set_aqualink_rssadapter_aux_state(light->button, true);
      } else {
        aq_programmer(AQ_SET_LIGHTCOLOR_MODE, light->button, 1, extra_value, aqdata);
      }
      return;
    } else if (isIAQL_ACTIVE && light->lightType == LC_JANDYINFINATE) {
      aq_programmer(AQ_SET_IAQLINK_JANDYINFINATE_ONOFF, light->button, 1, 0, aqdata);
      return;
    } else if (isIAQT_ENABLED) {
      aq_programmer(AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE, light->button, 0, extra_value, aqdata);
      return;
    }
  } else if (value == 0) {
    // We simply need to turn the light off at this point, so use allbutton key as it's the quickest.
    // but can't turn off a virtual light.
    if (isIAQL_ACTIVE && light->lightType == LC_JANDYINFINATE) {
      aq_programmer(AQ_SET_IAQLINK_JANDYINFINATE_ONOFF, light->button, 0, 0, aqdata);
      return;
    } else if (light->button->led->state == ON && !isMASK_SET(light->button->special_mask, VIRTUAL_BUTTON)) {
      //DPRINTF("allbutton off");
      aq_send_allb_cmd(light->button->code);
      // Could also check isRSSA_ENABLED and use set_aqualink_rssadapter_aux_state(light->button, FALSE);
      return;
    } else if (light->button->led->state != ON ) {
      LOG(PANL_LOG,LOG_WARNING, "Request to turn off Light mode '%s' to off, already off, ignoring!\n",light->button->label);
      return;
    }
  }

  // Logic, select one of 3 programming options, RSSD / AllButton / AqualinkTouch
  // virtual button light can only be done with AqualinkTouch
  // LC_PROGRAMABLE can only be done with AllButton
  // user can set a default
  // dimmer light NEEDS to be turned on with allbutton.

  // Check Virtual Button requires IAQT protocol
  if (isMASK_SET(light->button->special_mask, VIRTUAL_BUTTON) && !isIAQT_ENABLED)
  {
    // Log an error and stop execution if a Virtual Button is used without IAQT.
    LOG(PANL_LOG, LOG_ERR, "Light mode on virtual button needs AqualinkTouch protocol\n");
    return;
  }
  //  Use allbutton if LC_PROGRAMABLE light, or no other protocol options
  else if (light->lightType == LC_PROGRAMABLE)
  {
    //DPRINTF("AQ_SET_LIGHTPROGRAM_MODE");
    aq_programmer(AQ_SET_LIGHTPROGRAM_MODE, light->button, value, extra_value, aqdata);
  }
  //  Use allbutton if explicitly set, or no other protocol options
  else if ((!isRSSA_ENABLED && !isIAQT_ENABLED) ||
           (_aqconfig_.light_programming_interface == LIGHT_PROTOCOL_ALLB))
  {
    //DPRINTF("AQ_SET_ALLB_LIGHTCOLOR_MODE");
    aq_programmer(AQ_SET_ALLB_LIGHTCOLOR_MODE, light->button, value, extra_value, aqdata);
  }
  // Use iAqualink direct protocol for Jandy Infinite Water Colors when enabled
  else if (isIAQL_ACTIVE && light->lightType == LC_JANDYINFINATE)
  {
    aq_programmer(AQ_SET_IAQLINK_LIGHTCOLOR_MODE, light->button, value, extra_value, aqdata);
  }
  // Use AqualinkTouch if explicitly set or virtual button.
  else if (isIAQT_ENABLED &&
           (_aqconfig_.light_programming_interface == LIGHT_PROTOCOL_AQLT ||
            isMASK_SET(light->button->special_mask, VIRTUAL_BUTTON)))
  {
    //DPRINTF("AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE");
    // This depends on the button label being accurate with panel.  
    aq_programmer(AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE, light->button, value, extra_value, aqdata);
  }
  // Use RS-Serial Adapter protocol
  else if (isRSSA_ENABLED)
  {
    //DPRINTF("set_aqualink_rssadapter_aux_state");
    unsigned char rssd_value = value + RSSD_COLOR_LIGHT_OFFSET_WRITE;
    // If light is off, turn it on first
    if (light->button->led->state != ON)
    {
      set_aqualink_rssadapter_aux_state(light->button, TRUE);
    }
    // Adjust the value for Dimmer lights (map color index to full range %).
    if (light->lightType == LC_DIMMER || light->lightType == LC_DIMMER2)
    {
      rssd_value = value * 25; // Dimmer is full range % on RSSD
    }
    set_aqualink_rssadapter_aux_extended_state(light->button, rssd_value);
  }
  // Default Fallback (Should rarely be reached)
  else {
    //DPRINTF("AQ_SET_LIGHTCOLOR_MODE");
    aq_programmer(AQ_SET_LIGHTCOLOR_MODE, light->button, value, extra_value, aqdata);
  }


  // Use function so can be called from programming thread if we decide to in future.
  if (light->lightType != LC_PROGRAMABLE ) {
    // Only update if a panel programed light.  If AqualinkD programs, the programmer needs to know the last mode.
    updateButtonLightProgram(aqdata, value, deviceIndex);
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
  } else if (type == ON_OFF || type == TIMER ||type == TIMER_SEC || type == LIGHT_BRIGHTNESS || type == LIGHT_RGB || type == LIGHT_MODE){
    LOG(PANL_LOG,LOG_INFO, "Device request type '%s' for deviceindex %d '%s' of value %d from '%s'\n",
                          getActionName(type),
                          deviceIndex,
                          aqdata->aqbuttons[deviceIndex].label, 
                          value, 
                          getRequestName(source));
  } else {
    LOG(PANL_LOG,LOG_INFO, "Device request type '%s' of value %d from '%s'\n",
                          getActionName(type),
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
      setDeviceState(aqdata, deviceIndex, true, source);
      start_timer(aqdata, deviceIndex, value, 0);
    break;
    case TIMER_SEC:
      setDeviceState(aqdata, deviceIndex, true, source);
      start_timer(aqdata, deviceIndex, 0, value);
    break;
    case LIGHT_BRIGHTNESS:
      // Allow value=0 here (unlike LIGHT_MODE) since we could get multiple requests from a slider. (aka HomeKit)
      programDeviceLightBrightness(aqdata, value, deviceIndex, (source==NET_MQTT?true:false), source);
    break;
    case LIGHT_RGB:
      programDeviceLightRGB(aqdata, value, deviceIndex, source);
    break;
    case LIGHT_MODE:
      if (value <= 0) {
        // Consider this a bad/malformed request to turn the light off.
        panel_device_request(aqdata, ON_OFF, deviceIndex, 0, source);
      } else {
        programDeviceLightMode(aqdata, value, deviceIndex, (source==NET_MQTT?true:false), source);
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
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_TIME, NULL, AQP_NULL, AQP_NULL, aqdata);
#else
      aq_programmer(AQ_SET_TIME, NULL, aqdata);
#endif
    break;
    default:
      LOG(PANL_LOG,LOG_ERR, "Unknown device request type %d for deviceindex %d\n",type,deviceIndex);
    break;
  }

  return TRUE;
}


// Programmable light has been updated, so update the status in AqualinkD
void updateLightProgram(struct aqualinkdata *aqdata, int value, clight_detail *light)
{
  light->currentValue = value;
  if (value > 0 && light->lastValue != value) {
    light->lastValue = value;
    if (_aqconfig_.save_light_programming_value && light->lightType == LC_PROGRAMABLE ) {
      LOG(PANL_LOG,LOG_INFO, "Writing light programming value to config for %s\n",light->button->label);
      writeCfg(aqdata);
    }
  }
}

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

  updateLightProgram(aqdata, value, light);

  /*
  light->currentValue = value;
  if (value > 0 && light->lastValue != value) {
    light->lastValue = value;
    if (_aqconfig_.save_light_programming_value && light->lightType == LC_PROGRAMABLE ) {
      LOG(PANL_LOG,LOG_NOTICE, "Writing light programming value to config\n",button);
      writeCfg(aqdata);
    }
  }
    */
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


const char *getButtontSpecialMaskName(uint16_t mask) {
  switch(mask) {
    case VS_PUMP: 
      return "VSpump";
    break;
    case PROGRAM_LIGHT: 
      return "lightMode";
    break;
    case VIRTUAL_BUTTON_ALT_LABEL:
      return "altLabel";
    break;
    case VIRTUAL_BUTTON: 
      return "Virtual Button";
    break;
    case VIRTUAL_BUTTON_CHILLER:
      return "Virtual Button Chiller";
    default:
      return "unknown";
    break;
  }
}
void checkButtonSpecialMask(aqkey *button, uint16_t mask2remove) {
  if (isMASK_SET(button->special_mask,mask2remove)) {
    LOG(AQUA_LOG,LOG_ERR, "Config error can only have one type for button `%s`, removing `%s`\n",
                           button->name, 
                          getButtontSpecialMaskName(mask2remove));
    removeMASK(button->special_mask, mask2remove);
  }
}
void setButtonSpecialMask(aqkey *button, uint16_t mask2set) 
{
  switch(mask2set) {
    case VS_PUMP:                  // assign struct pump_detail
      checkButtonSpecialMask(button, PROGRAM_LIGHT);
      checkButtonSpecialMask(button, VIRTUAL_BUTTON_ALT_LABEL);
    break;
    case PROGRAM_LIGHT:            // assign struct clight_detail
      checkButtonSpecialMask(button, VIRTUAL_BUTTON_ALT_LABEL);
      checkButtonSpecialMask(button, VS_PUMP);
    break;
    case VIRTUAL_BUTTON_ALT_LABEL: // assign struct altlabel_detail (Maybe delete VIRTUAL_BUTTON)
      checkButtonSpecialMask(button, PROGRAM_LIGHT);
      checkButtonSpecialMask(button, VS_PUMP);
    break;
    case VIRTUAL_BUTTON:           // Type can be added to above
    break;
    case VIRTUAL_BUTTON_CHILLER:   // Type can be added to above
    break;
    default:
    break;
  }

  button->special_mask |= mask2set;
}

// How far below 100% to trigger an automatic console warning
#define EFFICIENCY_WARNING_THRESHOLD 90.0f 

float calculate_flow_efficiency_pct(pump_detail *pump) {
    // 1. Exit if not configured
    if (pump->efficiency.isConfigured == false || 
        pump->efficiency.baselineKLow <= 0.0f || 
        pump->efficiency.baselineKMed <= 0.0f || 
        pump->efficiency.baselineKHigh <= 0.0f) {
        return AQ_UNKNOWN; 
    }

    // 2. Guard against off states or invalid data
    if (pump->pStatus != PS_OK || pump->rpm < 1000 || pump->watts <= 0) {
        return 0.0f;
    }

    // Assigning shorthand names to match mathematical notation
    float x = (float)pump->rpm;
    float x0 = (float)pump->efficiency.baselineRpmLow;
    float x1 = (float)pump->efficiency.baselineRpmMed;
    float x2 = (float)pump->efficiency.baselineRpmHigh;

    float y0 = pump->efficiency.baselineKLow;
    float y1 = pump->efficiency.baselineKMed;
    float y2 = pump->efficiency.baselineKHigh;

    // 3. Calculate Lagrange terms to fit the quadratic curve
    float t0 = ((x - x1) * (x - x2)) / ((x0 - x1) * (x0 - x2));
    float t1 = ((x - x0) * (x - x2)) / ((x1 - x0) * (x1 - x2));
    float t2 = ((x - x0) * (x - x1)) / ((x2 - x0) * (x2 - x1));

    // Determine the dynamic target K for the current RPM
    float target_k = (y0 * t0) + (y1 * t1) + (y2 * t2);

    // 4. Calculate Live Scaled K
    float rpm_cubed = x * x * x;
    float current_scaled_k = ((float)pump->watts / rpm_cubed) * 1000000000.0f;
    
    // 5. Final percentage calculation
    float health_pct = (current_scaled_k / target_k) * 100.0f;

    if (pump->prclType == PENTAIR) {
      LOG(DPEN_LOG, LOG_DEBUG, "Pump %s efficiency, RPM=%d, Watts=%d, TargetK=%.2f, LiveK=%.2f, Eff=%.0f\n",
            pump->button->name, pump->rpm, pump->watts, target_k, current_scaled_k, health_pct);
    } else {
      LOG(DJAN_LOG, LOG_DEBUG, "Pump %s efficiency, RPM=%d, Watts=%d, TargetK=%.2f, LiveK=%.2f, Eff=%.0f\n",
            pump->button->name, pump->rpm, pump->watts, target_k, current_scaled_k, health_pct);
    }

    return health_pct;
}