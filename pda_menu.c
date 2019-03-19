
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "pda_menu.h"
#include "aq_serial.h"
#include "utils.h"

int _hlightindex = -1;
char _menu[PDA_LINES][AQ_MSGLEN+1];
static pda_menu_type _pda_m_type = PM_UNKNOWN;
static pthread_mutex_t _pda_menu_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _pda_menu_type_change_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t _pda_menu_hlight_change_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t _pda_menu_update_complete_cond = PTHREAD_COND_INITIALIZER;

void print_menu()
{
  int i;
  for (i=0; i < PDA_LINES; i++)
    printf("PDA Line %d = %s\n",i,_menu[i]);
  
  if (_hlightindex > -1)
    printf("PDA highlighted line = %d = %s\n",_hlightindex,_menu[_hlightindex]); 
}

int pda_m_hlightindex()
{
  return _hlightindex;
}

bool wait_pda_m_hlightindex_update(struct timespec *max_wait)
{
  int ret = 0;
  pthread_mutex_lock(&_pda_menu_mutex);
  if ((ret = pthread_cond_timedwait(&_pda_menu_hlight_change_cond,
                                    &_pda_menu_mutex, max_wait)))
    {
      logMessage (LOG_ERR, "wait_pda_m_hlightindex_update err %s\n",
                  strerror(ret));
    }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return ((ret == 0));
}

bool wait_pda_m_hlightindex_change(struct timespec *max_wait)
{
  int ret = 0;
  pthread_mutex_lock(&_pda_menu_mutex);
  int old_hlightindex  = _hlightindex;
  while (_hlightindex == old_hlightindex)
    {
      if ((ret = pthread_cond_timedwait(&_pda_menu_hlight_change_cond,
                                        &_pda_menu_mutex, max_wait)))
        {
          logMessage (LOG_ERR, "wait_pda_m_hlightindex_change err %s\n",
                      strerror(ret));
          break;
        }
    }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return ((_hlightindex != old_hlightindex));
}

int wait_pda_m_hlightindex(struct timespec *max_wait)
{
  if (_hlightindex == -1)
    {
      wait_pda_m_hlightindex_change(max_wait);
    }
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

/*
--- FW Version ---
Line 0 =
Line 1 =  PDA-PS4 Combo
Line 2 =
Line 3 = Firmware Version
Line 4 =
Line 5 =   PPD: PDA 1.2
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

/* This function is called when CMD_STATUS is received, indicating menu is complete.
   Return true is menu type changed. */
bool update_pda_menu_type()
{
  bool ret = false;
  pda_menu_type new_m_type = PM_UNKNOWN;

  if (strncmp(_menu[1]," PDA-PS4 Combo", 14) == 0) {
    new_m_type = PM_FW_VERSION;
  } else if (strncmp(_menu[1],"AIR  ", 5) == 0) {
    new_m_type = PM_HOME;
  } else if (strncmp(_menu[0],"EQUIPMENT STATUS", 16) == 0) {
    new_m_type = PM_EQUIPTMENT_STATUS;
  } else if (strncmp(_menu[0],"   EQUIPMENT    ", 16) == 0) {
    new_m_type = PM_EQUIPTMENT_CONTROL;
  } else if (strncmp(_menu[0],"   MAIN MENU    ", 16) == 0) {
    new_m_type = PM_MAIN_MENU;
  } else if (strncmp(_menu[4], "POOL MODE", 9) == 0 ) {
    new_m_type = PM_BUILDING_HOME;
  } else if (strncmp(_menu[0],"  SYSTEM SETUP  ", 16) == 0) {
    new_m_type = PM_SYSTEM_SETUP;
  } else if (strncmp(_menu[0],"    SET TEMP    ", 16) == 0) {
    new_m_type = PM_SET_TEMP;
  } else if (strncmp(_menu[0],"    SPA HEAT    ", 16) == 0) {
    new_m_type = PM_SPA_HEAT;
  } else if (strncmp(_menu[0],"   POOL HEAT    ", 16) == 0) {
    new_m_type = PM_POOL_HEAT;
  } else if ((strncmp(_menu[6],"Use ARROW KEYS  ", 16) == 0) &&
      (strncmp(_menu[0]," FREEZE PROTECT ", 16) == 0)) {
    new_m_type = PM_FREEZE_PROTECT;
  } else if ((strncmp(_menu[1],"    DEVICES     ", 16) == 0) &&
    (strncmp(_menu[0]," FREEZE PROTECT ", 16) == 0)) {
  new_m_type = PM_FREEZE_PROTECT_DEVICES;
  }

  if (new_m_type != _pda_m_type) {
    ret = true;
    _pda_m_type = new_m_type;
    logMessage(LOG_DEBUG, "PDA menu type now %d\n", _pda_m_type);
    pthread_cond_signal(&_pda_menu_type_change_cond);
  }

  return ret;
}

pda_menu_type pda_m_type()
{
  return _pda_m_type;
}

bool wait_pda_m_type_change(struct timespec *max_wait)
{
  int ret = 0;
  pthread_mutex_lock(&_pda_menu_mutex);
  pda_menu_type old_m_type  = _pda_m_type;
  while (_pda_m_type == old_m_type)
    {
      if ((ret = pthread_cond_timedwait(&_pda_menu_type_change_cond,
                                        &_pda_menu_mutex, max_wait)))
        {
          logMessage (LOG_ERR, "wait_pda_m_type_change err %s\n",
                      strerror(ret));
          break;
        }
    }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return ((_pda_m_type != old_m_type));
}

bool wait_pda_m_type(pda_menu_type menu, struct timespec *max_wait)
{
  while (_pda_m_type != menu)
    {
      if (! wait_pda_m_type_change(max_wait))
        {
          break;
        }
    }
  return ((_pda_m_type == menu));
}

// menu update is complete when highlight or status is received
// currently this waiting until status is received for menus that have
// highlight is is better to use wait_m_hightindex_update()
bool wait_pda_m_update_complete(struct timespec *max_wait)
{
  int ret = 0;
  pthread_mutex_lock(&_pda_menu_mutex);

  if ((ret = pthread_cond_timedwait(&_pda_menu_update_complete_cond,
                                    &_pda_menu_mutex, max_wait)))
    {
      logMessage (LOG_ERR, "wait_pda_m_update_complete err %s\n",
                  strerror(ret));
    }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return ((ret == 0));
}

bool process_pda_menu_packet(unsigned char* packet, int length)
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
          logMessage(LOG_DEBUG, "process_pda_menu_packet: hlight changed from shift or up/down value\n");
          pthread_cond_signal(&_pda_menu_hlight_change_cond);
      }
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
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
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      if (packet[4] <= PDA_LINES) {
        _hlightindex = packet[4];
      } else {
        _hlightindex = -1;
      }
      pthread_cond_signal(&_pda_menu_hlight_change_cond);
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
      // press up from top - shift menu down by 1
      //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0x01|0x8d|0x10|0x03|
      // press down from bottom - shift menu up by 1
      //   PDA Shif | HEX: 0x10|0x02|0x62|0x0f|0x01|0x08|0xff|0x8b|0x10|0x03|
      first_line = (signed char)(packet[4]);
      last_line = (signed char)(packet[5]);
      line_shift = (signed char)(packet[6]);
      logMessage(LOG_DEBUG, "\n");
      if (line_shift < 0) {
          for (i = first_line-line_shift; i <= last_line; i++) {
              memcpy(_menu[i+line_shift], _menu[i], AQ_MSGLEN+1);
          }
      } else {
          for (i = last_line; i >= first_line+line_shift; i--) {
              memcpy(_menu[i], _menu[i-line_shift], AQ_MSGLEN+1);
          }
      }
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    
  }
  pthread_mutex_unlock(&_pda_menu_mutex);

  return rtn;
}

/*
clear the menu
*/
bool pda_m_clear()
{
  logMessage(LOG_DEBUG, "PDA menu clear\n");

  _hlightindex = -1;
  _pda_m_type = PM_UNKNOWN;
  memset(_menu, 0, PDA_LINES * (AQ_MSGLEN+1));
  return true;
}
