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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>


#include "aqualink.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "allbutton_aq_programmer.h"

#ifdef AQ_PDA
  #include "pda.h"
  #include "pda_menu.h"
  #include "pda_aq_programmer.h"
#endif

#include "aq_panel.h"
#include "onetouch_aq_programmer.h"
#include "iaqtouch_aq_programmer.h"
#include "serialadapter.h"
#include "color_lights.h"
#include "config.h"
#include "devices_jandy.h"

#ifdef AQ_DEBUG
  #include <time.h>
  #include "timespec_subtract.h"
#endif

void _aq_programmer(program_type r_type, char *args, struct aqualinkdata *aq_data, bool allowOveride);


// Lookup table for programming function to protocal we will use for programming panel
// Should be one function per program_type enum.
/*
  static void * (*_prog_functions[])(void *ptr) = {
*/
typedef void *(*func_ptr)(void *ptr);

const func_ptr _prog_functions[AQP_RSSADAPTER_MAX] = {
     [AQ_GET_POOL_SPA_HEATER_TEMPS]    = get_allbutton_pool_spa_heater_temps,
     [AQ_GET_FREEZE_PROTECT_TEMP]      = get_allbutton_freeze_protect_temp,
     [AQ_SET_TIME]                     = set_allbutton_time,
     [AQ_SET_POOL_HEATER_TEMP]         = set_allbutton_pool_heater_temps, 
     [AQ_SET_SPA_HEATER_TEMP]          = set_allbutton_spa_heater_temps, 
     [AQ_SET_FRZ_PROTECTION_TEMP]      = set_allbutton_freeze_heater_temps, 
     [AQ_GET_DIAGNOSTICS_MODEL]        = get_allbutton_diag_model, 
     [AQ_GET_PROGRAMS]                 = get_allbutton_programs, 
     [AQ_SET_LIGHTPROGRAM_MODE]        = set_allbutton_light_programmode, 
     [AQ_SET_LIGHTCOLOR_MODE]          = set_allbutton_light_colormode, 
     [AQ_SET_SWG_PERCENT]              = set_allbutton_SWG, 
     [AQ_GET_AUX_LABELS]               = get_allbutton_aux_labels, 
     [AQ_SET_BOOST]                    = set_allbutton_boost, 
     [AQ_SET_ONETOUCH_PUMP_RPM]        = set_aqualink_onetouch_pump_rpm, 
     [AQ_GET_ONETOUCH_FREEZEPROTECT]   = get_aqualink_onetouch_freezeprotect, 
     [AQ_GET_ONETOUCH_SETPOINTS]       = get_aqualink_onetouch_setpoints, 
     [AQ_SET_ONETOUCH_TIME]            = set_aqualink_onetouch_time, 
     [AQ_SET_ONETOUCH_BOOST]           = set_aqualink_onetouch_boost, 
     [AQ_SET_ONETOUCH_SWG_PERCENT]     = set_aqualink_onetouch_swg_percent, 
     [AQ_SET_ONETOUCH_POOL_HEATER_TEMP]= set_aqualink_onetouch_pool_heater_temp, 
     [AQ_SET_ONETOUCH_SPA_HEATER_TEMP] = set_aqualink_onetouch_spa_heater_temp, 
     [AQ_SET_ONETOUCH_FREEZEPROTECT]   = set_aqualink_onetouch_freezeprotect, 
     [AQ_SET_IAQTOUCH_PUMP_RPM]        = set_aqualink_iaqtouch_pump_rpm, 
     [AQ_GET_IAQTOUCH_VSP_ASSIGNMENT]  = set_aqualink_iaqtouch_vsp_assignments, 
     [AQ_GET_IAQTOUCH_SETPOINTS]       = get_aqualink_iaqtouch_setpoints, 
     [AQ_GET_IAQTOUCH_FREEZEPROTECT]   = get_aqualink_iaqtouch_freezeprotect, 
     [AQ_GET_IAQTOUCH_AUX_LABELS]      = get_aqualink_iaqtouch_aux_labels, 
     [AQ_SET_IAQTOUCH_SWG_PERCENT]     = set_aqualink_iaqtouch_swg_percent, 
     [AQ_SET_IAQTOUCH_SWG_BOOST]       = set_aqualink_iaqtouch_swg_boost,
     [AQ_SET_IAQTOUCH_POOL_HEATER_TEMP]= set_aqualink_iaqtouch_pool_heater_temp, 
     [AQ_SET_IAQTOUCH_SPA_HEATER_TEMP] = set_aqualink_iaqtouch_spa_heater_temp, 
     [AQ_SET_IAQTOUCH_SET_TIME]        = set_aqualink_iaqtouch_time, 
     [AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM] = set_aqualink_iaqtouch_pump_vs_program, 
     [AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE] = set_aqualink_iaqtouch_light_colormode,
     [AQ_SET_IAQTOUCH_DEVICE_ON_OFF]   = set_aqualink_iaqtouch_device_on_off,
     [AQ_PDA_INIT]                     = set_aqualink_PDA_init, 
     [AQ_PDA_WAKE_INIT]                = set_aqualink_PDA_wakeinit, 
     [AQ_PDA_DEVICE_STATUS]            = get_aqualink_PDA_device_status, 
     [AQ_PDA_DEVICE_ON_OFF]            = set_aqualink_PDA_device_on_off,
     [AQ_PDA_SET_BOOST]                = set_PDA_aqualink_boost,
     [AQ_PDA_SET_SWG_PERCENT]          = set_PDA_aqualink_SWG_setpoint,
     [AQ_PDA_GET_AUX_LABELS]           = get_PDA_aqualink_aux_labels,
     [AQ_PDA_SET_POOL_HEATER_TEMPS]    = set_aqualink_PDA_pool_heater_temps,
     [AQ_PDA_SET_SPA_HEATER_TEMPS]     = set_aqualink_PDA_spa_heater_temps,
     [AQ_PDA_SET_FREEZE_PROTECT_TEMP]  = set_aqualink_PDA_freeze_protectsetpoint,
     [AQ_PDA_SET_TIME]                 = set_PDA_aqualink_time,
     //[AQ_PDA_GET_POOL_SPA_HEATER_TEMPS]= get_aqualink_PDA_pool_spa_heater_temps,
     [AQ_PDA_GET_FREEZE_PROTECT_TEMP]  = get_PDA_aqualink_pool_spa_heater_temps
     /*
     [AQ_PDA_SET_BOOST]                = set_PDA_aqualink_boost
     [AQ_PDA_SET_SWG_PERCENT]          = set_PDA_aqualink_SWG_setpoint
     [AQ_PDA_GET_AUX_LABELS]           = get_PDA_aqualink_aux_labels
     */
     
};


int roundTo(int num, int denominator) {
  return ((num + (denominator/2) ) / denominator )* denominator;
}

//(Intelliflo VF you set GPM, not RPM)
int RPM_check(pump_type type, int value, struct aqualinkdata *aqdata)
{
  int rtn = value;
  // RPM 3450 seems to be max
  // RPM 600 min
  // GPM 130 max
  // GPM 15 min
  if (type == VFPUMP) {
    if (rtn > 130)
      rtn = 130;
    else if (rtn < 15)
      rtn = 15;
    else
      rtn = roundTo(rtn, 5);
  } else {
    if (rtn > 3450)
      rtn = 3450;
    else if (rtn < 600)
      rtn = 600;
    else
      rtn = roundTo(rtn, 5);
  }

  return rtn;
}

int setpoint_check(int type, int value, struct aqualinkdata *aqdata)
{
  int rtn = value;
  int max = 0;
  int min = 0;
  char *type_msg;

  switch(type) {
    case POOL_HTR_SETOINT:
      type_msg = (isSINGLE_DEV_PANEL?"Temp1":"Pool");
      if ( aqdata->temp_units == CELSIUS ) {
        max = HEATER_MAX_C;
        min = (isSINGLE_DEV_PANEL?HEATER_MIN_C+1:HEATER_MIN_C);
      } else {
        max = HEATER_MAX_F;
        min = (isSINGLE_DEV_PANEL?HEATER_MIN_F+1:HEATER_MIN_F);
      }
      // if single device then TEMP1 & 2 (not pool & spa), TEMP1 must be set higher than TEMP2
      if (isSINGLE_DEV_PANEL && 
          aqdata->spa_htr_set_point != TEMP_UNKNOWN &&
          min <= aqdata->spa_htr_set_point) 
      {
        min = aqdata->spa_htr_set_point + 1;
      }
    break;
    case SPA_HTR_SETOINT:
      type_msg = (isSINGLE_DEV_PANEL?"Temp2":"Spa");
      if ( aqdata->temp_units == CELSIUS ) {
        max = (isSINGLE_DEV_PANEL?HEATER_MAX_C-1:HEATER_MAX_C);
        min = HEATER_MIN_C;
      } else {
        max = (isSINGLE_DEV_PANEL?HEATER_MAX_F-1:HEATER_MAX_F);
        min = HEATER_MIN_F;
      }
      // if single device then TEMP1 & 2 (not pool & spa), TEMP2 must be set lower than TEMP1
      if (isSINGLE_DEV_PANEL && 
          aqdata->pool_htr_set_point != TEMP_UNKNOWN &&
          max >= aqdata->pool_htr_set_point) 
      {
        max = aqdata->pool_htr_set_point - 1;
      }
    break;
    case FREEZE_SETPOINT:
      type_msg = "Freeze protect";
      if ( aqdata->temp_units == CELSIUS ) {
        max = FREEZE_PT_MAX_C;
        min = FREEZE_PT_MIN_C;
      } else {
        max = FREEZE_PT_MAX_F;
        min = FREEZE_PT_MIN_F;
      }
    break;
    case SWG_SETPOINT:
      type_msg = "Salt Water Generator";
      max = SWG_PERCENT_MAX;
      min = SWG_PERCENT_MIN;
    break;
    default:
      type_msg = "Unknown";
    break;
  }

  if (rtn > max)
    rtn = max;
  else if (rtn < min)
    rtn = min;

  // If SWG make sure it's 0,5,10,15,20......
  if (type == SWG_SETPOINT) {
    rtn = roundTo(rtn, 5);
  }
  
  if (rtn != value)
    LOG(PROG_LOG, LOG_WARNING, "Setpoint of %d for %s is outside range, using %d\n",value,type_msg,rtn);
  //else
  //  LOG(PROG_LOG, LOG_NOTICE, "Setting setpoint of %s to %d\n",type_msg,rtn);

  return rtn;
}

/*
  Figure out the fastest way in get all needed startup data depending on what protocols
  are available and what's just called us
*/
void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aq_data)
{
   LOG(PROG_LOG, LOG_INFO, "Initial setup call from %s with RSSA=%s ONETouch=%s IAQTouch=%s ExtendedProgramming=%s\n",
      (source_type == ALLBUTTON)?"AllButton":((source_type == RSSADAPTER)?"RSSA":((source_type == ONETOUCH)?"OneTouch":((source_type == IAQTOUCH)?"IAQTouch":"PDA"))),
      (isRSSA_ENABLED)?"enabled":"disabled",
      (isONET_ENABLED)?"enabled":"disabled",
      (isIAQT_ENABLED)?"enabled":"disabled",
      (isEXTP_ENABLED)?"enabled":"disabled"
   );

  if (isRSSA_ENABLED && isEXTP_ENABLED == true) {
    // serial adapter enabled and extended programming
    if (source_type == RSSADAPTER) {
      _aq_programmer(AQ_GET_RSSADAPTER_SETPOINTS, NULL, aq_data, false);
    } else if (source_type == ONETOUCH && isEXTP_ENABLED) {
      //_aq_programmer(AQ_GET_ONETOUCH_FREEZEPROTECT, NULL, aq_data, false); // Add back and remove below once tested and working
      //_aq_programmer(AQ_GET_ONETOUCH_SETPOINTS, NULL, aq_data, false);
    } else if (source_type == IAQTOUCH && isEXTP_ENABLED) {
      //_aq_programmer(AQ_GET_IAQTOUCH_FREEZEPROTECT, NULL, aq_data, false); // Add back and remove below once tested and working
      //_aq_programmer(AQ_GET_IAQTOUCH_SETPOINTS, NULL, aq_data, false); // This get's freeze & heaters, we should just get freeze if isRSSA_ENABLED
    } else if (source_type == ALLBUTTON) {
      _aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, aq_data, false); // This is still quicker that IAQ or ONE Touch protocols at the moment.
      if (_aqconfig_.use_panel_aux_labels) {
        _aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data, false);
      }
    }
  } else if (isRSSA_ENABLED && isEXTP_ENABLED == false) {
    // serial adapter enabled with no extended programming
     if (source_type == RSSADAPTER) {
      _aq_programmer(AQ_GET_RSSADAPTER_SETPOINTS, NULL, aq_data, false);
    } else if (source_type == ALLBUTTON) {
      _aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, aq_data, false);
      if (_aqconfig_.use_panel_aux_labels) {
        _aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data, false);
      }
    }
  } else if (!isRSSA_ENABLED && isEXTP_ENABLED && isONET_ENABLED) {
    // One touch extended and no serial adapter
    if (source_type == ONETOUCH) {
      _aq_programmer(AQ_GET_ONETOUCH_SETPOINTS, NULL, aq_data, false);
    } else if (source_type == ALLBUTTON) {
      if (_aqconfig_.use_panel_aux_labels) {
        _aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data, false);
      }
    }
  } else if (!isRSSA_ENABLED && isEXTP_ENABLED && isIAQT_ENABLED) {
    // IAQ touch extended and no serial adapter
    if (source_type == IAQTOUCH) {
      _aq_programmer(AQ_GET_IAQTOUCH_SETPOINTS, NULL, aq_data, false);
    } else if (source_type == ALLBUTTON) {
      if (_aqconfig_.use_panel_aux_labels) {
        _aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data, false);
      }
    }
#ifdef AQ_PDA
  } else if ( isPDA_PANEL && source_type == AQUAPDA) {
    aq_programmer(AQ_PDA_INIT, NULL, aq_data);
  } else if ( isPDA_PANEL && source_type == IAQTOUCH) {
    //aq_programmer(AQ_PDA_INIT, NULL, aq_data);
    if (_aqconfig_.use_panel_aux_labels) {
      aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data);
    }
    aq_programmer(AQ_GET_IAQTOUCH_SETPOINTS, NULL, aq_data);
    
#endif
  } else { // Must be all button only
    aq_programmer(AQ_GET_POOL_SPA_HEATER_TEMPS, NULL, aq_data);
    aq_programmer(AQ_GET_FREEZE_PROTECT_TEMP, NULL, aq_data);
    if (_aqconfig_.use_panel_aux_labels) {
      aq_programmer(AQ_GET_AUX_LABELS, NULL, aq_data);
    }
  }
}


bool in_light_programming_mode(struct aqualinkdata *aq_data)
{
  if ( ( aq_data->active_thread.thread_id != 0 ) &&
       ( aq_data->active_thread.ptype == AQ_SET_LIGHTPROGRAM_MODE ||
         aq_data->active_thread.ptype == AQ_SET_LIGHTCOLOR_MODE ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE)
  ) {
    return true;
  }

  return false;
}

bool in_swg_programming_mode(struct aqualinkdata *aq_data)
{
  if ( ( aq_data->active_thread.thread_id != 0 ) &&
       ( aq_data->active_thread.ptype == AQ_SET_ONETOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_SWG_PERCENT ||
         aq_data->active_thread.ptype == AQ_SET_ONETOUCH_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_IAQTOUCH_SWG_BOOST ||
         aq_data->active_thread.ptype == AQ_SET_BOOST)
  ) {
    return true;
  }

  return false;
}

bool in_ot_programming_mode(struct aqualinkdata *aq_data)
{
  if ( aq_data->active_thread.thread_id != 0 &&
       aq_data->active_thread.ptype >= AQP_ONETOUCH_MIN && 
       aq_data->active_thread.ptype <= AQP_ONETOUCH_MAX) {
    return true;
  }
  return false;
}

bool in_iaqt_programming_mode(struct aqualinkdata *aq_data)
{
  if ( aq_data->active_thread.thread_id != 0 &&
       aq_data->active_thread.ptype >= AQP_IAQTOUCH_MIN && 
       aq_data->active_thread.ptype <= AQP_IAQTOUCH_MAX) {
    return true;
  }
  return false;
}

bool in_allb_programming_mode(struct aqualinkdata *aq_data)
{
  if ( aq_data->active_thread.thread_id != 0 &&
       aq_data->active_thread.ptype >= AQP_ALLBUTTON_MIN && 
       aq_data->active_thread.ptype <= AQP_ALLBUTTONL_MAX) {
    return true;
  }
  return false;
}

// May need to re-think.  if we are in PDA mode and using IAQTOUCH should we
// return PAD or IAQTOUCH
emulation_type get_programming_mode(program_type type)
{
  if (type >= AQP_ALLBUTTON_MIN && 
      type <= AQP_ALLBUTTONL_MAX) {
     return ALLBUTTON;
  }
  if (type >= AQP_IAQTOUCH_MIN && 
      type <= AQP_IAQTOUCH_MAX) {
    return IAQTOUCH;
  }
  if (type >= AQP_ONETOUCH_MIN && 
      type <= AQP_ONETOUCH_MAX) {
    return ONETOUCH;
  }
  if (type >= AQP_PDA_MIN && 
      type <= AQP_PDA_MAX) {
    return AQUAPDA;
  }
  return SIM_NONE;
}

emulation_type get_current_programming_mode(struct aqualinkdata *aq_data)
{
  if ( !in_programming_mode(aq_data) ) {
     return SIM_NONE;
  }
  
  return get_programming_mode(aq_data->active_thread.ptype);
}


bool in_programming_mode(struct aqualinkdata *aq_data)
{
  if ( aq_data->active_thread.thread_id != 0 ) {
     return true;
  }

  return false;
}

void kick_aq_program_thread(struct aqualinkdata *aq_data, emulation_type source_type)
{
  if ( aq_data->active_thread.thread_id != 0 ) {
    if ( (source_type == ONETOUCH) && in_ot_programming_mode(aq_data))
    {
      LOG(ONET_LOG, LOG_DEBUG, "Kicking OneTouch thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);   
    } 
    else if (source_type == IAQTOUCH && in_iaqt_programming_mode(aq_data)) {
      LOG(IAQT_LOG, LOG_DEBUG, "Kicking IAQ Touch thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
    else if (source_type == ALLBUTTON && !in_ot_programming_mode(aq_data) && !in_iaqt_programming_mode(aq_data)) {
      LOG(PROG_LOG, LOG_DEBUG, "Kicking RS Allbutton thread %d,%p message '%s'\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id,aq_data->last_message);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
#ifdef AQ_PDA
    else if (source_type == AQUAPDA && !in_ot_programming_mode(aq_data)) {
      LOG(PDA_LOG, LOG_DEBUG, "Kicking PDA thread %d,%p\n",aq_data->active_thread.ptype, aq_data->active_thread.thread_id);
      pthread_cond_broadcast(&aq_data->active_thread.thread_cond);  
    }
#endif     
  }
}


void aq_programmer(program_type r_type, char *args, struct aqualinkdata *aq_data){
  _aq_programmer(r_type, args, aq_data, true);
}
void _aq_programmer(program_type r_type, char *args, struct aqualinkdata *aq_data, bool allowOveride)
{
  struct programmingThreadCtrl *programmingthread = malloc(sizeof(struct programmingThreadCtrl));

  program_type type = r_type;

  // RS SerialAdapter is quickest for changing thermostat temps, so use that if enabeled.
  // VSP RPM can only be changed with oneTouch or iAquatouch so check / use those
  // VSP Program is only available with iAquatouch, so check / use that.
  if (isRSSA_ENABLED && (r_type == AQ_SET_POOL_HEATER_TEMP || r_type == AQ_SET_SPA_HEATER_TEMP)) {
    if (r_type == AQ_SET_POOL_HEATER_TEMP)
      type = AQ_SET_RSSADAPTER_POOL_HEATER_TEMP;
    else if (r_type == AQ_SET_SPA_HEATER_TEMP)
      type = AQ_SET_RSSADAPTER_SPA_HEATER_TEMP;
  } else if (r_type == AQ_SET_PUMP_RPM) {
    if (isONET_ENABLED || isPDA_IAQT)
      type = AQ_SET_ONETOUCH_PUMP_RPM;
    else if (isIAQT_ENABLED)
      type = AQ_SET_IAQTOUCH_PUMP_RPM;
    else {
      LOG(PROG_LOG, LOG_ERR, "Can only change pump RPM with an extended device id\n",type);
      return;
    }
  } else if (r_type == AQ_SET_PUMP_VS_PROGRAM) {
    if (isIAQT_ENABLED)
      type = AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM;
    else {
      LOG(PROG_LOG, LOG_ERR, "Can only change pump VS Program with an iAqualink Touch device id\n",type);
      return;
    }
  }
#ifdef AQ_ONETOUCH
  // reset any types if to onetouch if available and if one touch is quicker
  // At moment. onetouch is quicker for boost, and slower for heaters
  else if (isONET_ENABLED && isEXTP_ENABLED) {
    switch (r_type){
      case AQ_GET_POOL_SPA_HEATER_TEMPS:
      //case AQ_GET_FREEZE_PROTECT_TEMP:
        type = AQ_GET_ONETOUCH_SETPOINTS;
      break;
      case AQ_SET_POOL_HEATER_TEMP:
        type = AQ_SET_ONETOUCH_POOL_HEATER_TEMP;
      break;
      case AQ_SET_SPA_HEATER_TEMP:
        type = AQ_SET_ONETOUCH_SPA_HEATER_TEMP;
      break;
      case AQ_SET_SWG_PERCENT:
        type = AQ_SET_ONETOUCH_SWG_PERCENT;
      break;
      case AQ_SET_BOOST:
        type = AQ_SET_ONETOUCH_BOOST;
      break;
      // NSF  ONE TOUCH TIME IS NOT WORKING YET
      //case AQ_SET_TIME:
      //  type = AQ_SET_ONETOUCH_TIME;
      //break;
      default:
        type = r_type;
      break;
    }
  }
#endif
#ifdef AQ_IAQTOUCH
  else if ((isIAQT_ENABLED && isEXTP_ENABLED) || isPDA_IAQT) {
    // IAQ Touch programming modes that should overite standard ones.
    switch (r_type){
      case AQ_GET_POOL_SPA_HEATER_TEMPS:
      //case AQ_GET_FREEZE_PROTECT_TEMP:
        type = AQ_GET_IAQTOUCH_SETPOINTS;
      break;
      case AQ_SET_SWG_PERCENT:
        type = AQ_SET_IAQTOUCH_SWG_PERCENT;
      break;
      case AQ_SET_BOOST:
        type = AQ_SET_IAQTOUCH_SWG_BOOST;
      break;
      case AQ_SET_POOL_HEATER_TEMP:
        type = AQ_SET_IAQTOUCH_POOL_HEATER_TEMP;
      break;
      case AQ_SET_SPA_HEATER_TEMP:
        type = AQ_SET_IAQTOUCH_SPA_HEATER_TEMP;
      break;
      case AQ_SET_TIME:
        type = AQ_SET_IAQTOUCH_SET_TIME;
      break;
      case AQ_PDA_DEVICE_ON_OFF:
        if (isPDA_IAQT) {
          type = AQ_SET_IAQTOUCH_DEVICE_ON_OFF;
        }
      break;
      // This isn;t going to work outside of PDA mode, if the labels are incorrect.
      case AQ_SET_LIGHTCOLOR_MODE:
        if (isPDA_IAQT) {
          type = AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE;
        }
      break;
      default:
        type = r_type;
      break;
    }
  }
#endif



#ifdef AQ_PDA
  // Check we are doing something valid request
  if (isPDA_PANEL && !isPDA_IAQT)
  {
    switch (r_type){
      case AQ_SET_POOL_HEATER_TEMP:
        type = AQ_PDA_SET_POOL_HEATER_TEMPS;
      break;
      case AQ_SET_SPA_HEATER_TEMP:
        type = AQ_PDA_SET_SPA_HEATER_TEMPS;
      break;
      case AQ_SET_SWG_PERCENT:
        type = AQ_PDA_SET_SWG_PERCENT;
      break;
      case AQ_PDA_DEVICE_ON_OFF:
      break;
      case AQ_GET_POOL_SPA_HEATER_TEMPS:
        type = AQ_PDA_GET_POOL_SPA_HEATER_TEMPS;
      break;
      case AQ_GET_FREEZE_PROTECT_TEMP:
        type = AQ_PDA_GET_FREEZE_PROTECT_TEMP;
      break;
      case AQ_SET_FRZ_PROTECTION_TEMP:
        type = AQ_PDA_SET_FREEZE_PROTECT_TEMP;
      break;
      case AQ_SET_BOOST:
        type = AQ_PDA_SET_BOOST;
      break;
      case AQ_SET_TIME:
        type = AQ_PDA_SET_TIME;
      break;
#ifdef BETA_PDA_AUTOLABEL
      case AQ_GET_AUX_LABELS:
        type = AQ_PDA_AUX_LABELS:
      break;
#endif
      default:
         type = r_type;
      break;
    }
    if (get_programming_mode(type) != AQUAPDA ) {
      LOG(PROG_LOG, LOG_ERR, "Selected Programming mode '%s' '%d' not supported with PDA control panel\n",ptypeName(type),type);
      return;
    }
    pda_reset_sleep();
  } else if (isPDA_PANEL && isPDA_IAQT)
  {
    if ( get_programming_mode(type) != IAQTOUCH) {
      LOG(PROG_LOG, LOG_ERR, "Selected Programming mode '%s' '%d' not supported with PDA control panel in iAqualinkTouch mode\n",ptypeName(type),type);
      return;
    }
  }
#endif

  if (!allowOveride) {
    // Reset anything back from changes above.
    type = r_type;
  }


  LOG(PROG_LOG, LOG_NOTICE, "Starting programming thread '%s'\n",ptypeName(type));

  programmingthread->aq_data = aq_data;
  programmingthread->thread_id = 0;
  //programmingthread->thread_args = args;
  if (args != NULL /*&& type != AQ_SEND_CMD*/)
    strncpy(programmingthread->thread_args, args, sizeof(programmingthread->thread_args)-1);

  switch(type) {
    case AQ_GET_RSSADAPTER_SETPOINTS:
      get_aqualink_rssadapter_setpoints();
      return; // No need to create this as thread.
      break;
    case AQ_SET_RSSADAPTER_POOL_HEATER_TEMP:
      set_aqualink_rssadapter_pool_setpoint(args, aq_data);
      return; // No need to create this as thread.
      break;
    case AQ_SET_RSSADAPTER_SPA_HEATER_TEMP:
      set_aqualink_rssadapter_spa_setpoint(args, aq_data);
      return; // No need to create this as thread.
      break;
    case AQ_ADD_RSSADAPTER_POOL_HEATER_TEMP:
      increase_aqualink_rssadapter_pool_setpoint(args, aq_data);
      return; // No need to create this as thread.
      break;
    case AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP:
      increase_aqualink_rssadapter_spa_setpoint(args, aq_data);
      return; // No need to create this as thread.
      break;
    default:
      // Should check that _prog_functions[type] is valid.
      if( pthread_create( &programmingthread->thread_id , NULL ,  _prog_functions[type], (void*)programmingthread) < 0) {
        LOG(PROG_LOG, LOG_ERR, "could not create thread\n");
        return;
      }
    break;
    
  }
  
  if ( programmingthread->thread_id != 0 ) {
    //LOG(PROG_LOG, LOG_DEBUG, "********* DID pthread_detach %d\n",programmingthread->thread_id);
    pthread_detach(programmingthread->thread_id);
  } else {
    //LOG(PROG_LOG, LOG_DEBUG, "********* DID NOT pthread_detach\n");
  }
}


void waitForSingleThreadOrTerminate(struct programmingThreadCtrl *threadCtrl, program_type type)
{
  //static int tries = 120;
  int tries = 120;
  static int waitTime = 1;
  int i=0;
/*
  i = 0;
  while (get_aq_cmd_length() > 0 && ( i++ <= tries) ) {
    LOG(PROG_LOG, LOG_DEBUG, "Thread %p (%s) sleeping, waiting command queue to empty\n", &threadCtrl->thread_id, ptypeName(type));
    sleep(waitTime);
  }
  if (i >= tries) {
    LOG(PROG_LOG, LOG_ERR, "Thread %p (%s) timeout waiting, ending\n",&threadCtrl->thread_id,ptypeName(type));
    free(threadCtrl);
    pthread_exit(0);
  }
*/
  while ( (threadCtrl->aq_data->active_thread.thread_id != 0) && ( i++ <= tries) ) {
    //LOG(PROG_LOG, LOG_DEBUG, "Thread %d sleeping, waiting for thread %d to finish\n", threadCtrl->thread_id, threadCtrl->aq_data->active_thread.thread_id);
    LOG(PROG_LOG, LOG_DEBUG, "Thread %p (%s) sleeping, waiting for thread %p (%s) to finish\n",
                &threadCtrl->thread_id, ptypeName(type),
                threadCtrl->aq_data->active_thread.thread_id, ptypeName(threadCtrl->aq_data->active_thread.ptype));
    sleep(waitTime);
  }
  
  if (i >= tries) {
    //LOG(PROG_LOG, LOG_ERR, "Thread %d timeout waiting, ending\n",threadCtrl->thread_id);
    LOG(PROG_LOG, LOG_ERR, "Thread (%s) %p timeout waiting for thread (%s) %p to finish\n",
                ptypeName(type), &threadCtrl->thread_id, ptypeName(threadCtrl->aq_data->active_thread.ptype),
                threadCtrl->aq_data->active_thread.thread_id);
    free(threadCtrl);
    pthread_exit(0);
  }
 
  // Clear out any messages to the UI.
  threadCtrl->aq_data->last_display_message[0] = '\0';
  threadCtrl->aq_data->active_thread.thread_id = &threadCtrl->thread_id;
  threadCtrl->aq_data->active_thread.ptype = type;

  #ifdef AQ_DEBUG
    clock_gettime(CLOCK_REALTIME, &threadCtrl->aq_data->start_active_time);
  #endif

  LOG(PROG_LOG, LOG_INFO, "Programming: %s, %d\n", ptypeName(threadCtrl->aq_data->active_thread.ptype), threadCtrl->aq_data->active_thread.ptype);

  LOG(PROG_LOG, LOG_DEBUG, "Thread %d,%p is active (%s)\n",
              threadCtrl->aq_data->active_thread.ptype,
              threadCtrl->aq_data->active_thread.thread_id,
              ptypeName(threadCtrl->aq_data->active_thread.ptype));
}

void cleanAndTerminateThread(struct programmingThreadCtrl *threadCtrl)
{
  //waitfor_queue2empty();
  #ifndef AQ_DEBUG
  LOG(PROG_LOG, LOG_DEBUG, "Thread %d,%p (%s) finished\n",threadCtrl->aq_data->active_thread.ptype, threadCtrl->thread_id,ptypeName(threadCtrl->aq_data->active_thread.ptype));
  #else
  struct timespec elapsed;
  clock_gettime(CLOCK_REALTIME, &threadCtrl->aq_data->last_active_time);
  timespec_subtract(&elapsed, &threadCtrl->aq_data->last_active_time, &threadCtrl->aq_data->start_active_time);
  LOG(PROG_LOG, LOG_NOTICE, "Thread %d,%p (%s) finished in %d.%03ld sec\n",
             threadCtrl->aq_data->active_thread.ptype,
             threadCtrl->aq_data->active_thread.thread_id,
             ptypeName(threadCtrl->aq_data->active_thread.ptype),
             elapsed.tv_sec, elapsed.tv_nsec / 1000000L);
  #endif

  // Quick delay to allow for last message to be sent.
  delay(500);
  threadCtrl->aq_data->active_thread.thread_id = 0;
  threadCtrl->aq_data->active_thread.ptype = AQP_NULL;
  threadCtrl->thread_id = 0;
  // Force update, change display message
  threadCtrl->aq_data->updated = true;
  free(threadCtrl);
  pthread_exit(0);
}




const char *ptypeName(program_type type)
{
  switch (type) {
    case AQ_GET_POOL_SPA_HEATER_TEMPS:
      return "Get Heater setpoints";
    break;
    case AQ_GET_FREEZE_PROTECT_TEMP:
      return "Get Freeze protect";
    break;
    case AQ_SET_TIME:
      return "Set time";
    break;
    case AQ_SET_POOL_HEATER_TEMP:
      return "Set Pool heater setpoint";
    break;
    case AQ_SET_SPA_HEATER_TEMP:
      return "Set Spa heater setpoint";
    break;
    case AQ_SET_FRZ_PROTECTION_TEMP:
      return "Set Freeze protect setpoint";
    break;
    case AQ_GET_DIAGNOSTICS_MODEL:
      return "Get diagnostics";
    break;
    case AQ_GET_PROGRAMS:
      return "Get programs";
    break;
    case AQ_SET_LIGHTPROGRAM_MODE:
      return "Set light color (using AqualinkD)";
    break;
    case AQ_SET_LIGHTCOLOR_MODE:
      return "Set light color (using Panel)";
    break;
     case AQ_SET_SWG_PERCENT:
      return "Set SWG percent";
    break;
     case AQ_SET_BOOST:
      return "SWG Boost";
    break;
    case AQ_SET_PUMP_RPM:
      return "Set Pump RPM";
    break;
    case AQ_SET_PUMP_VS_PROGRAM:
      return "Set Pump VS Program";
    break;
    case AQ_GET_AUX_LABELS:
      return "Get AUX labels";
    break;
    case AQ_GET_RSSADAPTER_SETPOINTS:
      return "Get SerialAdapter setpoints";
    break;
    case AQ_SET_RSSADAPTER_POOL_HEATER_TEMP:
      return "Set SerialAdapter Pool heater setpoint";
    break;
    case AQ_SET_RSSADAPTER_SPA_HEATER_TEMP:
      return "Set SerialAdapter Spa heater setpoint";
    break;
#ifdef AQ_ONETOUCH
    case AQ_SET_ONETOUCH_PUMP_RPM:
      return "Set OneTouch Pump RPM";
    break;
    case AQ_SET_ONETOUCH_MACRO:
      return "Set OneTouch Macro";
    break;
    case AQ_GET_ONETOUCH_SETPOINTS:
      return "Get OneTouch setpoints";
    break;
     case AQ_GET_ONETOUCH_FREEZEPROTECT:
      return "Get OneTouch freezeprotect";
    break;
    case AQ_SET_ONETOUCH_TIME:
      return "Set OneTouch time";
    break;
    case AQ_SET_ONETOUCH_BOOST:
      return "Set OneTouch Boost";
    break;
    case AQ_SET_ONETOUCH_SWG_PERCENT:
      return "Set OneTouch SWG Percent";
    break;
    case AQ_SET_ONETOUCH_FREEZEPROTECT:
      return "Set OneTouch Freezeprotect";
    break;
    case AQ_SET_ONETOUCH_POOL_HEATER_TEMP:
      return "Set OneTouch Pool Heater Temp";
    break;
    case AQ_SET_ONETOUCH_SPA_HEATER_TEMP:
      return "Set OneTouch Spa Heater Temp";
    break;
#endif
#ifdef AQ_IAQTOUCH
    case AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM:
      return "Set iAqualink Touch Pump VS Program";
    break;
    case AQ_SET_IAQTOUCH_PUMP_RPM:
      return "Set iAqualink Touch Pump RPM";
    break;
    case AQ_GET_IAQTOUCH_VSP_ASSIGNMENT:
      return "Get iAqualink Touch Pump Assignment";
    break;
    case AQ_GET_IAQTOUCH_SETPOINTS:
      return "Get iAqualink Touch Setpoints";
    break;
    case AQ_GET_IAQTOUCH_FREEZEPROTECT:
      return "Get iAqualink Touch Freezeprotect";
    break;
    case AQ_GET_IAQTOUCH_AUX_LABELS:
      return "Get iAqualink AUX Labels";
    break;
    case AQ_SET_IAQTOUCH_SWG_PERCENT:
      return "Set iAqualink SWG Percent";
    break;
    case AQ_SET_IAQTOUCH_SWG_BOOST:
      return "Set iAqualink Boost";
    break;
    case AQ_SET_IAQTOUCH_SPA_HEATER_TEMP:
      return "Set iAqualink Spa Heater";
    break;
    case AQ_SET_IAQTOUCH_POOL_HEATER_TEMP:
      return "Set iAqualink Pool Heater";
    break;
    case AQ_SET_IAQTOUCH_SET_TIME:
      return "Set iAqualink Set Time";
    break;
    case AQ_SET_IAQTOUCH_DEVICE_ON_OFF:
      return "Set iAqualink Device On/Off";
    break;
    case AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE:
      return "Set iAqualink Light Color (using panel)";
    break;
#endif
#ifdef AQ_PDA
    case AQ_PDA_INIT:
      return "Init PDA";
    break;
    case AQ_PDA_DEVICE_STATUS:
      return "Get PDA Device status";
    break;
    case AQ_PDA_DEVICE_ON_OFF:
      return "Switch PDA device on/off";
    break;
    case AQ_PDA_WAKE_INIT:
      return "PDA init after wake";
    break;
    case AQ_PDA_SET_BOOST:
      return "Set PDA SWG Boost";
    break;
    case AQ_PDA_SET_SWG_PERCENT:
      return "Set PDA SWG Percent";
    break;
    case AQ_PDA_GET_AUX_LABELS:
      return "Get PDA Lebels";
    break;
    case AQ_PDA_SET_POOL_HEATER_TEMPS:
      return "Set PDA Pool Heater";
    break;
    case AQ_PDA_SET_SPA_HEATER_TEMPS:
      return "Set PDA Spa Heater";
    break;
    case AQ_PDA_SET_FREEZE_PROTECT_TEMP:
      return "Set PDA Freese protect";
    break;
    case AQ_PDA_SET_TIME:
      return "Set PDA time";
    break;
    case AQ_PDA_GET_POOL_SPA_HEATER_TEMPS:
      return "Get PDA heater setpoints";
    break;
    case AQ_PDA_GET_FREEZE_PROTECT_TEMP:
      return "Get PDA freeze protect";
    break;
#endif
    case AQP_NULL:
    default:
      return "Unknown";
    break;
  }
}

// Cleaner version of above for UI display purposes.
const char *programtypeDisplayName(program_type type)
{
  switch (type) {
    case AQ_GET_POOL_SPA_HEATER_TEMPS:
    case AQ_GET_ONETOUCH_SETPOINTS:
    case AQ_GET_FREEZE_PROTECT_TEMP:
    case AQ_GET_IAQTOUCH_SETPOINTS:
    case AQ_GET_RSSADAPTER_SETPOINTS:
    case AQ_GET_ONETOUCH_FREEZEPROTECT:
    case AQ_GET_IAQTOUCH_FREEZEPROTECT:
#ifdef AQ_PDA
    case AQ_PDA_INIT:
#endif
      return "Programming: retrieving setpoints";
    break;
    case AQ_SET_IAQTOUCH_SET_TIME:
    case AQ_SET_ONETOUCH_TIME:
    case AQ_SET_TIME:
      return "Programming: setting time";
    break;
    case AQ_SET_POOL_HEATER_TEMP:
    case AQ_SET_ONETOUCH_POOL_HEATER_TEMP:
    case AQ_SET_SPA_HEATER_TEMP:
    case AQ_SET_ONETOUCH_SPA_HEATER_TEMP:
    case AQ_SET_IAQTOUCH_SPA_HEATER_TEMP:
    case AQ_SET_IAQTOUCH_POOL_HEATER_TEMP:
    case AQ_SET_RSSADAPTER_POOL_HEATER_TEMP:
    case AQ_SET_RSSADAPTER_SPA_HEATER_TEMP:
      return "Programming: setting heater";
    break;
    case AQ_SET_FRZ_PROTECTION_TEMP:
    case AQ_SET_ONETOUCH_FREEZEPROTECT:
      return "Programming: setting Freeze protect";
    break;
    case AQ_GET_DIAGNOSTICS_MODEL:
      return "Programming: retrieving diagnostics";
    break;
    case AQ_GET_PROGRAMS:
      return "Programming: retrieving programs";
    break;
    case AQ_SET_LIGHTPROGRAM_MODE:
    case AQ_SET_LIGHTCOLOR_MODE:
    case AQ_SET_IAQTOUCH_LIGHTCOLOR_MODE:
      return "Programming: setting light color";
    break;
    case AQ_SET_SWG_PERCENT:
    case AQ_SET_ONETOUCH_SWG_PERCENT:
    case AQ_SET_IAQTOUCH_SWG_PERCENT:
      return "Programming: setting SWG percent";
    break;
    case AQ_GET_AUX_LABELS:
    case AQ_GET_IAQTOUCH_AUX_LABELS:
      return "Programming: retrieving AUX labels";
    break;
    case AQ_SET_BOOST:
    case AQ_SET_ONETOUCH_BOOST:
    case AQ_SET_IAQTOUCH_SWG_BOOST:
      return "Programming: setting SWG Boost";
    break;
    case AQ_SET_ONETOUCH_PUMP_RPM:
    case AQ_SET_IAQTOUCH_PUMP_RPM:
    case AQ_SET_PUMP_RPM:
    case AQ_SET_IAQTOUCH_PUMP_VS_PROGRAM:
    case AQ_SET_PUMP_VS_PROGRAM:
      return "Programming: setting Pump RPM";
    break;
    case AQ_SET_ONETOUCH_MACRO:
      return "Programming: setting OneTouch Macro";
    break;
    case AQ_GET_IAQTOUCH_VSP_ASSIGNMENT:
      return "Get Pump Assignment";
    break;
    case AQ_SET_IAQTOUCH_DEVICE_ON_OFF:
      return "Programming: setting device on/off";
    break;
#ifdef AQ_PDA
    case AQ_PDA_DEVICE_STATUS:
      return "Programming: retrieving PDA Device status";
    break;
    case AQ_PDA_DEVICE_ON_OFF:
      return "Programming: setting device on/off";
    break;
    case AQ_PDA_WAKE_INIT:
      return "Programming: PDA wakeup";
    break;
#endif
    default:
      return "Programming: please wait!";
    break;
  }
}
