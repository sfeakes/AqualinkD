

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

#define _GNU_SOURCE 1 // for strcasestr
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pda_menu.h"
#include "aq_serial.h"
#include "utils.h"

static int _hlightindex = -1;
static int _hlightcharindexstart = -1;
static int _hlightcharindexstop = -1;
static char _menu[PDA_LINES][AQ_MSGLEN+1];

void print_menu()
{
  int i;
  for (i=0; i < PDA_LINES; i++) {
    //printf("PDA Line %d = %s\n",i,_menu[i]);
    LOG(PDA_LOG,LOG_INFO, "PDA Menu Line %d = %s\n",i,_menu[i]);
  }
  
  if (_hlightindex > -1) {
    //printf("PDA highlighted line = %d = %s\n",_hlightindex,_menu[_hlightindex]);
    LOG(PDA_LOG,LOG_DEBUG, "PDA Menu highlighted line#=%d line='%s'\n",_hlightindex,_menu[_hlightindex]);
  }

  if (_hlightcharindexstart > -1) {
    LOG(PDA_LOG,LOG_DEBUG, "PDA Menu highlighted characters line#=%d numer=%d start=%d end=%d actual='%.*s'\n",_hlightindex,(_hlightcharindexstop-_hlightcharindexstart),_hlightcharindexstart,_hlightcharindexstop,(_hlightcharindexstop-_hlightcharindexstart),&_menu[_hlightindex][_hlightcharindexstart+1]);
  }
}

int pda_m_hlightindex()
{
  return _hlightindex;
}

char *pda_m_hlight()
{
  return pda_m_line(_hlightindex);
}

char *pda_m_line(int index)
{
  if (index >= 0 && index < PDA_LINES)
    return _menu[index];
  else
    return "-"; // Just return something bad so I can use string comparison with no null check
  //  return NULL;
}

char *pda_m_hlightchars(int *len)
{
  if (_hlightindex <= -1) {
    *len = 0;
    return NULL;
  }
  *len = _hlightcharindexstop - _hlightcharindexstart;
  return &_menu[_hlightindex][_hlightcharindexstart];
}

// Find exact menu item
int pda_find_m_index(char *text)
{
  int i;

  for (i = 0; i < PDA_LINES; i++) {
    //printf ("+++ pda_find_m_index() Compare '%s' to '%s' index %d\n",text,pda_m_line(i),i);
    if (strncmp(pda_m_line(i), text, strlen(text)) == 0)
      return i;
  }

  return -1;
}

// Fine menu item case insensative
int pda_find_m_index_case(char *text, int limit)
{
  int i;

  for (i = 0; i < PDA_LINES; i++) {
    //printf ("+++ pda_find_m_index_case() Compare '%s' to '%s' index %d\n",text,pda_m_line(i),i);
    if (strncasecmp(pda_m_line(i), text, limit) == 0)
      return i;
  }

  return -1;
}

// Find menu item very loose
int pda_find_m_index_loose(char *text)
{
  int i;

  for (i = 0; i < PDA_LINES; i++) {
    //printf ("+++ pda_find_m_index_loose() Compare '%s' to '%s' index %d\n",text,pda_m_line(i),i);
    //if (strstr(pda_m_line(i), text) != NULL)
    if (strcasestr(pda_m_line(i), text) != NULL)
      return i;
  }

  return -1;
}
/*
// Same as above but strip whitespace from menu item (NOT text parameter)
int pda_find_m_index_swcase(char *text, int limit)
{
  int i;
  char *mi;

  for (i = 0; i < PDA_LINES; i++) {
    //printf ("+++ Compare '%s' to '%s' index %d\n",text,pda_m_line(i),i);
    mi = trimwhitespace(pda_m_line(i));
    if (strncasecmp(mi, text, limit) == 0)
      return i;
  }

  return -1;
}
*/
pda_menu_type pda_m_type()
{
  
  if (strncasecmp(_menu[1],"AIR  ", 5) == 0) {
    return PM_HOME;
  } else if (strncasecmp(_menu[0],"EQUIPMENT STATUS", 16) == 0) {
    return PM_EQUIPTMENT_STATUS;
  } else if (strncasecmp(_menu[0],"   EQUIPMENT    ", 16) == 0) {
    return PM_EQUIPTMENT_CONTROL;
  } else if (strncasecmp(_menu[0],"   MAIN MENU    ", 16) == 0) {
    return PM_MAIN;
  //else if ((_menu[0] == '\0' && _hlightindex == -1) || strncmp(_menu[4], "POOL MODE", 9) == 0 )// IF we are building the main menu this may be valid
  } else if (strncasecmp(_menu[4], "POOL MODE", 9) == 0 ||  // Will not see POOL MODE if single device config (pool vs pool&spa)
             strncasecmp(_menu[9], "EQUIPMENT ON/OFF", 16) == 0) {
    if (pda_m_hlightindex() == -1) {
      return PM_BUILDING_HOME;
    } else {
      return PM_HOME;
    }
  } else if (strncasecmp(_menu[0],"    SET TEMP    ", 16) == 0) {
    return PM_SET_TEMP;
  } else if (strncasecmp(_menu[0],"    SET TIME    ", 16) == 0) {
    return PM_SET_TIME;
  } else if (strncasecmp(_menu[0],"  SET AquaPure  ", 16) == 0) {
    return PM_AQUAPURE;
  } else if (strncasecmp(_menu[0],"    SPA HEAT    ", 16) == 0) {
    return PM_SPA_HEAT;
  } else if (strncasecmp(_menu[0],"   POOL HEAT    ", 16) == 0) {
    return PM_POOL_HEAT;
  } else if (strncasecmp(_menu[0],"  SYSTEM SETUP  ", 16) == 0) {
    return PM_SYSTEM_SETUP;
  } else if (strncasecmp(_menu[6],"Use ARROW KEYS  ", 16) == 0 &&
             strncasecmp(_menu[0]," FREEZE PROTECT ", 16) == 0) {
    return PM_FREEZE_PROTECT;
  } else if (strncasecmp(_menu[1],"    DEVICES     ", 16) == 0 &&
             strncasecmp(_menu[0]," FREEZE PROTECT ", 16) == 0) {
    return PM_FREEZE_PROTECT_DEVICES;
  } else if (strncasecmp(_menu[3],"Firmware Version", 16) == 0 ||
             strncasecmp(_menu[1],"    AquaPalm", 12) == 0 ||
             strncasecmp(_menu[1]," PDA-P", 6) == 0) {  // PDA-P4 Only -or- PDA-PS4 Combo
    return PM_FW_VERSION;
  } else if (strncasecmp(_menu[0],"   LABEL AUX    ", 16) == 0) {// Catch AUX and not AUX4
    return PM_AUX_LABEL;
  } else if (strncasecmp(_menu[0],"   LABEL AUX", 12) == 0 &&  // Will have number ie AUX4
             strncasecmp(_menu[2],"  CURRENT LABEL ", 16) == 0) {
    return PM_AUX_LABEL_DEVICE;
  } else if (strcasestr(_menu[0],"BOOST")) { // This is bad check, but PDA menus are BOOST | Boost | BOOST POOL | BOOST SPA, need to do better.
    return PM_BOOST;
  }
  return PM_UNKNOWN;
}


/*
--- Main Menu ---
Line 0 = 
Line 1 = AIR  
(Line 4 first, Line 2 last, Highligh when finished)
--- Equiptment Status  ---
Line 0 = EQUIPMENT STATUS
(Line 0 is first. No Highlight, everything in list is on)
--- Equiptment on/off menu --
Line 0 =    EQUIPMENT    
(Line 0 is first. Highlight when complete)
*/

bool process_pda_menu_packet(unsigned char* packet, int length, bool force_print_menu)
{
  bool rtn = true;
  signed char first_line;
  signed char last_line;
  signed char line_shift;
  signed char i;
  int index = 0;
  static bool printed_page = false;


  switch (packet[PKT_CMD]) {
    case CMD_PDA_CLEAR:
      _hlightindex = -1;
      _hlightcharindexstart = -1;
      _hlightcharindexstop = -1;
      memset(_menu, 0, PDA_LINES * (AQ_MSGLEN+1));
      printed_page = false;
    break;
    case CMD_STATUS:
      if ( printed_page == false && (getLogLevel(PDA_LOG) >= LOG_DEBUG)){
        print_menu();
        printed_page = true;
      }
    break;
    case CMD_MSG_LONG:
    /*
      if (packet[PKT_DATA] < 10) {
        memset(_menu[packet[PKT_DATA]], 0, AQ_MSGLEN);
        strncpy(_menu[packet[PKT_DATA]], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[packet[PKT_DATA]][AQ_MSGLEN] = '\0';
      }*/
      index = packet[PKT_DATA] & 0xF;
      if (index < 10) {
        memset(_menu[index], 0, AQ_MSGLEN);
        strncpy(_menu[index], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[index][AQ_MSGLEN] = '\0';
      }
      if ((getLogLevel(PDA_LOG) >= LOG_DEBUG) && force_print_menu ){
        print_menu();
        printed_page = true;
      } else {
        printed_page = false;
      }
    break;
    case CMD_PDA_HIGHLIGHT:
      // when switching from hlight to hlightchars index 255 is sent to turn off hlight
      if (packet[4] <= PDA_LINES) {
        _hlightindex = packet[4];
        _hlightcharindexstart = -1;
        _hlightcharindexstop = -1;
      } else {
        _hlightindex = -1;
        _hlightcharindexstart = -1;
        _hlightcharindexstop = -1;
      }
      //if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG && force_print_menu ){
        print_menu();
        printed_page = true;
      }
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      // pkt[4] = line, pkt[5] = startchar, pkt[6] = endchar, pkt[7] = clr/inv
      // highlight characters 10 to 15 on line 3 (from FREEZE PROTECT menu)
      //   PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x0a|0x0f|0x01|0xa1|0x10|0x03|
      // clear hlight chars 2 to 9 on line 2 and line 3 then hlight char 2 to 3 on line 3
      // (from SET TIME menu)
      //   PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x02|0x09|0x00|0x91|0x10|0x03|
      //   PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x03|0x02|0x09|0x00|0x92|0x10|0x03|
      //   PDA HlightChars | HEX: 0x10|0x02|0x62|0x10|0x02|0x02|0x03|0x01|0x8c|0x10|0x03|
      // https://github.com/ballle98/AqualinkD/issues/46
      // Character highlight should not update highlight index 
      if (packet[4] <= PDA_LINES) {
        _hlightindex = packet[4];
        _hlightcharindexstart = packet[5];
        _hlightcharindexstop = packet[6];
      } else {
        _hlightindex = -1;
        _hlightcharindexstart = -1;
        _hlightcharindexstop = -1;
      }
      //if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG && force_print_menu ){print_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
       // press up from top - shift menu down by 1
       //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0x01|0x8d|0x10|0x03|
       // press down from bottom - shift menu up by 1
       //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0xff|0x8b|0x10|0x03|
       first_line = (signed char)(packet[4]);
       last_line = (signed char)(packet[5]);
       line_shift = (signed char)(packet[6]);
       LOG(PDA_LOG,LOG_DEBUG, "\n");
       if (line_shift < 0) {
           for (i = first_line-line_shift; i <= last_line; i++) {
               memcpy(_menu[i+line_shift], _menu[i], AQ_MSGLEN+1);
           }
           _menu[last_line][0] = '\0';
       } else {
           for (i = last_line; i >= first_line+line_shift; i--) {
               memcpy(_menu[i], _menu[i-line_shift], AQ_MSGLEN+1);
           }
           _menu[first_line][0] = '\0';
       }
       if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
    break;   
  }

  return rtn;
}


#ifdef DO_NOT_COMPILE
bool NEW_process_pda_menu_packet_NEW(unsigned char* packet, int length)
{
  bool rtn = true;
  signed char first_line;
  signed char last_line;
  signed char line_shift;
  signed char i;

  pthread_mutex_lock(&_pda_menu_mutex);
  switch (packet[PKT_CMD]) {
    case CMD_STATUS:
      pthread_cond_signal(&_pda_menu_update_complete_cond);
    break;
    case CMD_PDA_CLEAR:
      rtn = pda_m_clear();
    break;
    case CMD_MSG_LONG:
      if (packet[PKT_DATA] < 10) {
        memset(_menu[packet[PKT_DATA]], 0, AQ_MSGLEN);
        strncpy(_menu[packet[PKT_DATA]], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[packet[PKT_DATA]][AQ_MSGLEN] = '\0';
      }
      if (packet[PKT_DATA] == _hlightindex) {
          LOG(PDA_LOG,LOG_DEBUG, "process_pda_menu_packet: hlight changed from shift or up/down value\n");
          pthread_cond_signal(&_pda_menu_hlight_change_cond);
      }
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
      update_pda_menu_type();
    break;
    case CMD_PDA_HIGHLIGHT:
      // when switching from hlight to hlightchars index 255 is sent to turn off hlight
      if (packet[4] <= PDA_LINES) {
        _hlightindex = packet[4];
      } else {
        _hlightindex = -1;
      }
      pthread_cond_signal(&_pda_menu_hlight_change_cond);
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      if (packet[4] <= PDA_LINES) {
        _hlightindex = packet[4];
      } else {
        _hlightindex = -1;
      }
      pthread_cond_signal(&_pda_menu_hlight_change_cond);
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
      // press up from top - shift menu down by 1
      //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0x01|0x8d|0x10|0x03|
      // press down from bottom - shift menu up by 1
      //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0xff|0x8b|0x10|0x03|
      first_line = (signed char)(packet[4]);
      last_line = (signed char)(packet[5]);
      line_shift = (signed char)(packet[6]);
      LOG(PDA_LOG,LOG_DEBUG, "\n");
      if (line_shift < 0) {
          for (i = first_line-line_shift; i <= last_line; i++) {
              memcpy(_menu[i+line_shift], _menu[i], AQ_MSGLEN+1);
          }
      } else {
          for (i = last_line; i >= first_line+line_shift; i--) {
              memcpy(_menu[i], _menu[i-line_shift], AQ_MSGLEN+1);
          }
      }
      if (getLogLevel(PDA_LOG) >= LOG_DEBUG){print_menu();}
    break;
    
  }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return rtn;
}
#endif
