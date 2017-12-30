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

#include "config.h"
#include "domoticz.h"


/*
*  Link LED numbers to buttons, this is valid for RS8 and below, RS10 and above are different
* need to update this code in future.
*/
void initButtons(struct aqualinkdata *aqdata)
{
  aqdata->aqbuttons[0].led = &aqdata->aqualinkleds[7-1];
  aqdata->aqbuttons[0].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[0].label = BTN_PUMP;
  //aqdata->aqbuttons[0].label = "Filter Pump";
  aqdata->aqbuttons[0].name = BTN_PUMP;
  //aqdata->aqbuttons[0].code = (unsigned char *)KEY_PUMP;
  aqdata->aqbuttons[0].code = KEY_PUMP;
  aqdata->aqbuttons[0].dz_idx = 37;
  
  aqdata->aqbuttons[1].led = &aqdata->aqualinkleds[6-1];
  aqdata->aqbuttons[1].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[1].label = BTN_SPA;
  //aqdata->aqbuttons[1].label = "Spa Mode";
  aqdata->aqbuttons[1].name = BTN_SPA;
  //aqdata->aqbuttons[1].code = (unsigned char *)KEY_SPA;
  aqdata->aqbuttons[1].code = KEY_SPA;
  aqdata->aqbuttons[1].dz_idx = 38;
  
  aqdata->aqbuttons[2].led = &aqdata->aqualinkleds[5-1];
  aqdata->aqbuttons[2].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[2].label = BTN_AUX1;
  //aqdata->aqbuttons[2].label = "Cleaner";
  aqdata->aqbuttons[2].name = BTN_AUX1;
  //aqdata->aqbuttons[2].code = (unsigned char *)KEY_AUX1;
  aqdata->aqbuttons[2].code = KEY_AUX1;
  aqdata->aqbuttons[2].dz_idx = 39;
  
  aqdata->aqbuttons[3].led = &aqdata->aqualinkleds[4-1];
  aqdata->aqbuttons[3].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[3].label = BTN_AUX2;
  //aqdata->aqbuttons[3].label = "Waterfall";
  aqdata->aqbuttons[3].name = BTN_AUX2;
  //aqdata->aqbuttons[3].code = (unsigned char *)KEY_AUX2;
  aqdata->aqbuttons[3].code = KEY_AUX2;
  aqdata->aqbuttons[3].dz_idx = 40;
  
  aqdata->aqbuttons[4].led = &aqdata->aqualinkleds[3-1];
  aqdata->aqbuttons[4].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[4].label = BTN_AUX3;
  //aqdata->aqbuttons[4].label = "Spa Blower";
  aqdata->aqbuttons[4].name = BTN_AUX3;
  //aqdata->aqbuttons[4].code = (unsigned char *)KEY_AUX3;
  aqdata->aqbuttons[4].code = KEY_AUX3;
  aqdata->aqbuttons[4].dz_idx = 41;
  
  aqdata->aqbuttons[5].led = &aqdata->aqualinkleds[9-1];
  aqdata->aqbuttons[5].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[5].label = BTN_AUX4;
  //aqdata->aqbuttons[5].label = "Pool Light";
  aqdata->aqbuttons[5].name = BTN_AUX4;
  //aqdata->aqbuttons[5].code = (unsigned char *)KEY_AUX4;
  aqdata->aqbuttons[5].code = KEY_AUX4;
  aqdata->aqbuttons[5].dz_idx = 42;
  
  aqdata->aqbuttons[6].led = &aqdata->aqualinkleds[8-1];
  aqdata->aqbuttons[6].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[6].label = BTN_AUX5;
  //aqdata->aqbuttons[6].label = "Spa Light";
  aqdata->aqbuttons[6].name = BTN_AUX5;
  //aqdata->aqbuttons[6].code = (unsigned char *)KEY_AUX5;
  aqdata->aqbuttons[6].code = KEY_AUX5;
  aqdata->aqbuttons[6].dz_idx = 43;
  
  aqdata->aqbuttons[7].led = &aqdata->aqualinkleds[12-1];
  aqdata->aqbuttons[7].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[7].label = BTN_AUX6;
  aqdata->aqbuttons[7].name = BTN_AUX6;
  //aqdata->aqbuttons[7].code = (unsigned char *)KEY_AUX6;
  aqdata->aqbuttons[7].code = KEY_AUX6;
  aqdata->aqbuttons[7].dz_idx = DZ_NULL_IDX;
  
  aqdata->aqbuttons[8].led = &aqdata->aqualinkleds[1-1];
  aqdata->aqbuttons[8].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[8].label = BTN_AUX7;
  aqdata->aqbuttons[8].name = BTN_AUX7;
  //aqdata->aqbuttons[8].code = (unsigned char *)KEY_AUX7;
  aqdata->aqbuttons[8].code = KEY_AUX7;
  aqdata->aqbuttons[8].dz_idx = DZ_NULL_IDX;
  
  aqdata->aqbuttons[9].led = &aqdata->aqualinkleds[15-1];
  aqdata->aqbuttons[9].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[9].label = BTN_POOL_HTR;
  //aqdata->aqbuttons[9].label = "Heater";
  aqdata->aqbuttons[9].name = BTN_POOL_HTR;
  //aqdata->aqbuttons[9].code = (unsigned char *)KEY_POOL_HTR;
  aqdata->aqbuttons[9].code = KEY_POOL_HTR;
  aqdata->aqbuttons[9].dz_idx = 44;
  
  aqdata->aqbuttons[10].led = &aqdata->aqualinkleds[17-1];
  aqdata->aqbuttons[10].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[10].label = BTN_SPA_HTR;
  //aqdata->aqbuttons[10].label = "Heater";
  aqdata->aqbuttons[10].name = BTN_SPA_HTR;
  //aqdata->aqbuttons[10].code = (unsigned char *)KEY_SPA_HTR;
  aqdata->aqbuttons[10].code = KEY_SPA_HTR;
  aqdata->aqbuttons[10].dz_idx = 44;
  
  aqdata->aqbuttons[11].led = &aqdata->aqualinkleds[19-1];
  aqdata->aqbuttons[11].led->state = LED_S_UNKNOWN;
  aqdata->aqbuttons[11].label = BTN_SOLAR_HTR;
  //aqdata->aqbuttons[11].label = "Solar Heater";
  aqdata->aqbuttons[11].name = BTN_SOLAR_HTR;
  //aqdata->aqbuttons[11].code = (unsigned char *)KEY_SOLAR_HTR;
  aqdata->aqbuttons[11].code = KEY_SOLAR_HTR;
  aqdata->aqbuttons[11].dz_idx = DZ_NULL_IDX;
  
}