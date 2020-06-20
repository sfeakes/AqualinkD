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
#include <string.h>
#include "config.h"
#include "domoticz.h"

#ifdef AQ_RS16
void initButtons_RS16(struct aqualinkdata *aqdata);
#endif

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

#ifdef DO_NOT_COMPILE

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