
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pda_menu.h"
#include "aq_serial.h"
#include "utils.h"

int _hlightindex = -1;
char _menu[PDA_LINES][AQ_MSGLEN+1];

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

char *pda_m_hlight()
{
  return pda_m_line(_hlightindex);
}

char *pda_m_line(int index)
{
  if (index >= 0 && index < PDA_LINES)
    return _menu[index];
  else
    return NULL;
}

pda_menu_type pda_m_type()
{
  if (strncmp(_menu[1],"AIR  ", 5) == 0)
    return PM_MAIN;
  else if (strncmp(_menu[0],"EQUIPMENT STATUS", 16) == 0)
    return PM_EQUIPTMENT_STATUS;
  else if (strncmp(_menu[0],"   EQUIPMENT    ", 16) == 0)
    return PM_EQUIPTMENT_CONTROL;
  else if (strncmp(_menu[0],"    MAIN MENU    ", 16) == 0)
    return PM_SETTINGS;
  else if ((_menu[0] == '\0' && _hlightindex == -1) || strncmp(_menu[4], "POOL MODE", 9) == 0 )// IF we are building the main menu this may be valid
    return PM_BUILDING_MAIN;

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

bool process_pda_menu_packet(unsigned char* packet, int length)
{
  bool rtn = true;
  switch (packet[PKT_CMD]) {
    case CMD_PDA_CLEAR:
      _hlightindex = -1;
      memset(_menu, 0, PDA_LINES * (AQ_MSGLEN+1));
    break;
    case CMD_MSG_LONG:
      if (packet[PKT_DATA] < 10) {
        memset(_menu[packet[PKT_DATA]], 0, AQ_MSGLEN);
        strncpy(_menu[packet[PKT_DATA]], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[packet[PKT_DATA]][AQ_MSGLEN] = '\0';
      }
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_HIGHLIGHT:
      _hlightindex = packet[4],_menu[packet[4]];
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
      memcpy(_menu[1], _menu[2], (PDA_LINES-1) * (AQ_MSGLEN+1) );
      if (getLogLevel() >= LOG_DEBUG){print_menu();}
    break;
    
  }

  return rtn;
}
