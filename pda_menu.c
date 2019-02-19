
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
    new_m_type = PM_MAIN;
  } else if (strncmp(_menu[0],"EQUIPMENT STATUS", 16) == 0) {
    new_m_type = PM_EQUIPTMENT_STATUS;
  } else if (strncmp(_menu[0],"   EQUIPMENT    ", 16) == 0) {
    new_m_type = PM_EQUIPTMENT_CONTROL;
  } else if (strncmp(_menu[0],"    MAIN MENU    ", 16) == 0) {
    new_m_type = PM_SETTINGS;
  } else if (strncmp(_menu[4], "POOL MODE", 9) == 0 ) {
    new_m_type = PM_BUILDING_MAIN;
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


bool process_pda_menu_packet(unsigned char* packet, int length)
{
  bool rtn = true;
  pthread_mutex_lock(&_pda_menu_mutex);
  switch (packet[PKT_CMD]) {
    case CMD_PDA_CLEAR:
      rtn = pda_m_clear();
    break;
    case CMD_MSG_LONG:
      if (packet[PKT_DATA] < 10) {
        memset(_menu[packet[PKT_DATA]], 0, AQ_MSGLEN);
        strncpy(_menu[packet[PKT_DATA]], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[packet[PKT_DATA]][AQ_MSGLEN] = '\0';
      }
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
      update_pda_menu_type();
    break;
    case CMD_PDA_HIGHLIGHT:
      _hlightindex = packet[4];
      pthread_cond_signal(&_pda_menu_hlight_change_cond);
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
      memcpy(_menu[1], _menu[2], (PDA_LINES-1) * (AQ_MSGLEN+1) );
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
  memset(_menu, 0, PDA_LINES * (AQ_MSGLEN+1));
  return true;
}
