#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "onetouch.h"
#include "onetouch_aq_programmer.h"
#include "aq_serial.h"
#include "utils.h"
#include "aq_serial.h"
#include "packetLogger.h"
#include "aq_programmer.h"
#include "aqualink.h"
//#include "pda_menu.h"


int _ot_hlightindex = -1;
int _ot_hlightcharindexstart = -1;
int _ot_hlightcharindexstop = -1;
char _menu[ONETOUCH_LINES][AQ_MSGLEN+1];
struct ot_macro _macros[3];

void set_macro_status();
void pump_update(struct aqualinkdata *aq_data, int updated);

void print_onetouch_menu()
{
  int i;
  for (i=0; i < ONETOUCH_LINES; i++) {
    //printf("PDA Line %d = %s\n",i,_menu[i]);
    logMessage(LOG_INFO, "OneTouch Menu Line %d = %s\n",i,_menu[i]);
  }
  
  if (_ot_hlightcharindexstart > -1) {
    logMessage(LOG_INFO, "OneTouch Menu highlighted line = %d, '%s' hligh-char(s) '%.*s'\n",
                 _ot_hlightindex,_menu[_ot_hlightindex],
                 (_ot_hlightcharindexstart - _ot_hlightcharindexstop + 1), &_menu[_ot_hlightindex][_ot_hlightcharindexstart]);
  } else if (_ot_hlightindex > -1) {
    logMessage(LOG_INFO, "OneTouch Menu highlighted line = %d = %s\n",_ot_hlightindex,_menu[_ot_hlightindex]);
  } 
}

int onetouch_menu_hlightindex()
{
  return _ot_hlightindex;
}

char *onetouch_menu_hlight()
{
  return onetouch_menu_line(_ot_hlightindex);
}

char *onetouch_menu_hlightchars(int *len)
{
  *len = _ot_hlightcharindexstart - _ot_hlightcharindexstop + 1;
  return &_menu[_ot_hlightindex][_ot_hlightcharindexstart];
}

char *onetouch_menu_line(int index)
{
  if (index >= 0 && index < ONETOUCH_LINES)
    return _menu[index];
  else
    return "-"; // Just return something bad so I can use string comparison with no null check
  //  return NULL;
}

// Find exact menu item.
int onetouch_menu_find_index(char *text)
{
  int i;

  for (i = 0; i < ONETOUCH_LINES; i++) {
    if (ot_strcmp(onetouch_menu_line(i), text) == 0)
    //if (ot_strcmp(onetouch_menu_line(i), text, limit) == 0)
      return i;
  }

  return -1;
}


bool process_onetouch_menu_packet(unsigned char* packet, int length)
{
  bool rtn = true;
  signed char first_line;
  signed char last_line;
  signed char line_shift;
  signed char i;

  switch (packet[PKT_CMD]) {
    case CMD_PDA_CLEAR:
      _ot_hlightindex = -1;
      _ot_hlightcharindexstart = -1;
      _ot_hlightcharindexstart = -1;
      memset(_menu, 0, ONETOUCH_LINES * (AQ_MSGLEN+1));
    break;
    case CMD_MSG_LONG:
      if (packet[PKT_DATA] < ONETOUCH_LINES) {
        memset(_menu[(int)packet[PKT_DATA]], 0, AQ_MSGLEN);
        strncpy(_menu[(int)packet[PKT_DATA]], (char*)packet+PKT_DATA+1, AQ_MSGLEN);
        _menu[packet[PKT_DATA]][AQ_MSGLEN] = '\0';
      }
      //if (getLogLevel() >= LOG_DEBUG){print_onetouch_menu();}
    break;
    case CMD_PDA_HIGHLIGHT:
      // when switching from hlight to hlightchars index 255 is sent to turn off hlight
      if (packet[4] <= ONETOUCH_LINES) {
        _ot_hlightindex = packet[4];
        _ot_hlightcharindexstart = -1;
        _ot_hlightcharindexstart = -1;
      } else {
        _ot_hlightindex = -1;
        _ot_hlightcharindexstart = -1;
        _ot_hlightcharindexstart = -1;
      }
      logMessage(LOG_DEBUG, "OneTouch Menu highlighted line = %d = %s\n",_ot_hlightindex,_menu[_ot_hlightindex]);
      //if (getLogLevel() >= LOG_DEBUG){print_onetouch_menu();}
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      if (packet[4] <= ONETOUCH_LINES) {
        _ot_hlightindex = packet[4];
        _ot_hlightcharindexstart = packet[5];
        _ot_hlightcharindexstop = packet[6];
      } else {
        _ot_hlightindex = -1;
        _ot_hlightcharindexstart = -1;
        _ot_hlightcharindexstart = -1;
      }
      logMessage(LOG_DEBUG, "OneTouch Menu highlighted line = %d, '%s' chars '%.*s'\n",
                 _ot_hlightindex,_menu[_ot_hlightindex],
                 (_ot_hlightcharindexstart - _ot_hlightcharindexstop + 1), &_menu[_ot_hlightindex][_ot_hlightcharindexstart]);
      //if (getLogLevel() >= LOG_DEBUG){print_onetouch_menu();}
    break;
    case CMD_PDA_SHIFTLINES:
      /// press up from top - shift menu down by 1
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
       //if (getLogLevel() >= LOG_DEBUG){print_onetouch_menu();}
    break;   
  }

  return rtn;
}

void setUnits_ot(char *str, struct aqualinkdata *aq_data)
{
  // NSF This needs to use setUnits from aqualinkd.c
  if (aq_data->temp_units == UNKNOWN) {
    if (str[15] == 'F')
      aq_data->temp_units = FAHRENHEIT;
    else if (str[15] == 'C')
      aq_data->temp_units = CELSIUS;
    else
      aq_data->temp_units = UNKNOWN;

    logMessage(LOG_INFO, "Temp Units set to %d (F=0, C=1, Unknown=2)\n", aq_data->temp_units);
  }
}

bool log_heater_setpoints(struct aqualinkdata *aq_data)
{
  bool rtn = false;

  if (ot_strcmp(_menu[2], "Pool Heat") == 0)
    aq_data->pool_htr_set_point = ot_atoi(&_menu[2][10]);
  if (ot_strcmp(_menu[3], "Spa Heat") == 0 )
    aq_data->spa_htr_set_point = ot_atoi(&_menu[3][9]);
  
  setUnits_ot(_menu[2], aq_data);
  
  logMessage(LOG_DEBUG, "POOL HEATER SETPOINT %d\n",aq_data->pool_htr_set_point);
  logMessage(LOG_DEBUG, "SPA HEATER SETPOINT %d\n",aq_data->spa_htr_set_point);

  return rtn;
}

bool log_panelversion(struct aqualinkdata *aq_data)
{
  char *end;

  // It's already been set
  if (strlen(aq_data->version) > 0) {
    return false;
  }

  strcpy(aq_data->version, trimwhitespace(_menu[4]));
  // Trim trailing space
  end = aq_data->version + strlen(aq_data->version) - 1;
  while(end > aq_data->version && isspace(*end)) end--;

  strcpy(end+2, trimwhitespace(_menu[7]));
  // Trim trailing space
  end = aq_data->version + strlen(aq_data->version) - 1;
  while(end > aq_data->version && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  logMessage(LOG_DEBUG, "**** '%s' ****\n",aq_data->version);

  return true;
}

//Info:   OneTouch Menu Line 3 = Temp        38`F
bool log_freeze_setpoints(struct aqualinkdata *aq_data)
{
  bool rtn = false;

  if (ot_strcmp(_menu[3], "Temp") == 0)
    aq_data->frz_protect_set_point = ot_atoi(&_menu[3][11]);

  setUnits_ot(_menu[3], aq_data);

  logMessage(LOG_DEBUG, "FREEZE PROTECT SETPOINT %d\n",aq_data->frz_protect_set_point);

  return rtn;
}

bool log_qeuiptment_status(struct aqualinkdata *aq_data)
{
  bool rtn = false;

  if (ot_strcmp(_menu[2],"Intelliflo VS") == 0 ||
      ot_strcmp(_menu[2],"Intelliflo VF") == 0 ||
      ot_strcmp(_menu[2],"Jandy ePUMP") == 0) {
    rtn = true;
    int rpm = 0;
    int watts = 0;
    int gpm = 0;
    int pump_index = ot_atoi(&_menu[2][14]);
    // RPM displays differently depending on 3 or 4 digit rpm.
    if (ot_strcmp(_menu[3],"RPM:") == 0){
      rpm = ot_atoi(&_menu[3][10]);
      if (ot_strcmp(_menu[4],"Watts:") == 0) {
        watts = ot_atoi(&_menu[4][11]);
      }
      if (ot_strcmp(_menu[5],"GPM:") == 0) {
        gpm = ot_atoi(&_menu[5][11]);
      }
    } else if (ot_strcmp(_menu[3],"*** Priming ***") == 0){
      rpm = PUMP_PRIMING;
    } else if (ot_strcmp(_menu[3],"(Offline)") == 0){
      rpm = PUMP_OFFLINE;
    }

    logMessage(LOG_DEBUG, "OneTouch Pump %s, Index %d, RPM %d, Watts %d, GPM %d\n",_menu[2],pump_index,rpm,watts,gpm);

    int i;
    for (i=0; i < aq_data->num_pumps; i++) {
      if (aq_data->pumps[i].pumpIndex == pump_index) {
        //printf("**** FOUND PUMP %d at index %d *****\n",pump_index,i);
        //aq_data->pumps[i].updated = true;
        pump_update(aq_data, i);
        aq_data->pumps[i].rpm = rpm;
        aq_data->pumps[i].watts = watts;
        aq_data->pumps[i].gpm = gpm;
        if (aq_data->pumps[i].pumpType == PT_UNKNOWN){
          if (ot_strcmp(_menu[2],"Intelliflo VS") == 0)
            aq_data->pumps[i].pumpType = VSPUMP;
          else if (ot_strcmp(_menu[2],"Intelliflo VF") == 0)
            aq_data->pumps[i].pumpType = VFPUMP;
          else if (ot_strcmp(_menu[2],"Jandy ePUMP") == 0)
            aq_data->pumps[i].pumpType = EPUMP;
        }
        //printf ("Set Pump Type to %d\n",aq_data->pumps[i].pumpType);
      }
    }
  } else if (ot_strcmp(_menu[2],"AQUAPURE") == 0) {
    /* Info:   OneTouch Menu Line 0 = Equipment Status
       Info:   OneTouch Menu Line 1 = 
       Info:   OneTouch Menu Line 2 =   AQUAPURE 60%  
       Info:   OneTouch Menu Line 3 =  Salt 7600 PPM  */
    int swgp = atoi(&_menu[2][11]);
    if ( aq_data->swg_percent != swgp ) {
      aq_data->swg_percent = swgp;
      rtn = true;
    }
    logMessage(LOG_DEBUG, "OneTouch SWG = %d\n",swgp);

    if (ot_strcmp(_menu[3],"Salt") == 0) {
      int ppm = atoi(&_menu[3][6]);
      if ( aq_data->swg_ppm != ppm ) {
        aq_data->swg_ppm = ppm;
        rtn = true;
      }
      logMessage(LOG_DEBUG, "OneTouch PPM = %d\n",ppm);
    }
  } else if (ot_strcmp(_menu[2],"Chemlink") == 0) {
    /*   Info:   OneTouch Menu Line 0 = Equipment Status
         Info:   OneTouch Menu Line 1 = 
         Info:   OneTouch Menu Line 2 =    Chemlink 1   
         Info:   OneTouch Menu Line 3 =  ORP 750/PH 7.0  */
    if (ot_strcmp(_menu[3],"ORP") == 0) {
      int orp = atoi(&_menu[3][4]);
      char *indx = strchr(_menu[3], '/');
      float ph = atof(indx+3);
      if (aq_data->ph != ph || aq_data->orp != orp) {
         aq_data->ph = ph;
         aq_data->orp = orp;
        return true;
      }
      logMessage(LOG_INFO, "OneTouch Cemlink ORP = %d PH = %f\n",orp,ph);
    }
  }



  return rtn;
}

ot_menu_type get_onetouch_memu_type()
{
  if (ot_strcmp(_menu[11],"SYSTEM") == 0)
    return OTM_ONETOUCH;
  else if (ot_strcmp(_menu[0],"Jandy AquaLinkRS") == 0)
    return OTM_SYSTEM;
  else if (ot_strcmp(_menu[0],"EQUIPMENT STATUS") == 0)
    return OTM_EQUIPTMENT_STATUS;
  else if (ot_strcmp(_menu[0],"Select Speed") == 0)
    return OTM_SELECT_SPEED;
  else if (ot_strcmp(_menu[0],"Menu") == 0)
    return OTM_MENUHELP;
  else if (ot_strcmp(_menu[0],"Set Temp") == 0)
    return OTM_SET_TEMP;
  else if (ot_strcmp(_menu[0],"Set Time") == 0)
    return OTM_SET_TIME;
  else if (ot_strcmp(_menu[0],"System Setup") == 0)
    return OTM_SYSTEM_SETUP;
  else if (ot_strcmp(_menu[0],"Freeze Protect") == 0)
    return OTM_FREEZE_PROTECT;
  else if (ot_strcmp(_menu[0],"Boost Pool") == 0)
    return OTM_BOOST;
  else if (ot_strcmp(_menu[7],"REV ") == 0) // NSF Need a better check.
    return OTM_VERSION;

  return OTM_UNKNOWN;
}

void pump_update(struct aqualinkdata *aq_data, int updated) {
  const int bitmask[MAX_PUMPS] = {1,2,4,8};
  static unsigned char updates = '\0';
  int i;

  if (updated == -1) {
    for(i=0; i < MAX_PUMPS; i++) {
      if ((updates & bitmask[i]) != bitmask[i]) {
        aq_data->pumps[i].rpm = TEMP_UNKNOWN;
        aq_data->pumps[i].gpm = TEMP_UNKNOWN;
        aq_data->pumps[i].watts = TEMP_UNKNOWN;
      }
    }
    updates = '\0';
  } else if (updated >=0 && updated < MAX_PUMPS) {
     updates |= bitmask[updated];
  }
}

bool new_menu(struct aqualinkdata *aq_data)
{
  static bool initRS = false;
  bool rtn = false;
  static ot_menu_type last_menu_type = OTM_UNKNOWN;
  ot_menu_type menu_type = get_onetouch_memu_type();

  print_onetouch_menu();

  switch (menu_type) {
    case OTM_ONETOUCH:
      set_macro_status();
      break;
    case OTM_EQUIPTMENT_STATUS:
      if (initRS == false) {
        queueGetProgramData(ONETOUCH, aq_data);
        initRS = true;
      }
      rtn = log_qeuiptment_status(aq_data);
      // Hit select to get to next menu ASAP.
      if ( in_ot_programming_mode(aq_data) == false )
        ot_queue_cmd(KEY_ONET_SELECT);
      break;
    case OTM_SET_TEMP:
      rtn = log_heater_setpoints(aq_data);
    break;
    case OTM_FREEZE_PROTECT:
      rtn = log_freeze_setpoints(aq_data);
    break;
    case OTM_VERSION:
      rtn = log_panelversion(aq_data);
      logMessage(LOG_DEBUG, "**** ONETOUCH INIT ****");
      queueGetProgramData(ONETOUCH, aq_data);
      //set_aqualink_onetouch_pool_heater_temp()
      //aq_programmer(AQ_SET_ONETOUCH_POOL_HEATER_TEMP, "95", aq_data);
      //aq_programmer(AQ_SET_ONETOUCH_SPA_HEATER_TEMP, "94", aq_data);
      initRS = true;
    break;
    default:
      break;
  }

  if (last_menu_type == OTM_EQUIPTMENT_STATUS && menu_type != OTM_EQUIPTMENT_STATUS ) {
    // End of equiptment status chain of menus, reset any pump that wasn't listed in menus
    pump_update(aq_data, -1);
  }

  last_menu_type = menu_type;

  return rtn;
}

void set_macro_status()
{
  // OneTouch Menu Line 2 = SPA MODE     OFF
  // OneTouch Menu Line 5 = CLEAN MODE    ON
  // OneTouch Menu Line 8 = ONETOUCH 3   OFF
  if (get_onetouch_memu_type() == OTM_ONETOUCH) {
    strncpy(_macros[0].name, _menu[2], 13);
    chopwhitespace(_macros[0].name);
    _macros[0].ison = (_menu[2][15] == 'N'?true:false);

    strncpy(_macros[1].name, _menu[5], 13);
    chopwhitespace(_macros[1].name);
    _macros[1].ison = (_menu[5][15] == 'N'?true:false);

    strncpy(_macros[2].name, _menu[8], 13);
    chopwhitespace(_macros[2].name);
    _macros[2].ison = (_menu[8][15] == 'N'?true:false);

    logMessage(LOG_DEBUG, "Macro #1 '%s' is %s\n",_macros[0].name,_macros[0].ison?"On":"Off");
    logMessage(LOG_DEBUG, "Macro #2 '%s' is %s\n",_macros[1].name,_macros[1].ison?"On":"Off");
    logMessage(LOG_DEBUG, "Macro #3 '%s' is %s\n",_macros[2].name,_macros[2].ison?"On":"Off");

  }
}

unsigned char _last_msg_type = 0x00;
unsigned char _last_kick_type = -1;

int thread_kick_type()
{
  return _last_kick_type;
}

unsigned char *last_onetouch_packet()
{
  return &_last_msg_type;
}

bool process_onetouch_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data)
{
  static bool filling_menu = false;
  bool rtn = false;
  //int i;
  //char *msg;
  //static unsigned char last_msg_type = 0x00;
  //static bool init = false;

  //process_pda_packet(packet, length);

  //logMessage(LOG_DEBUG, "RS Received ONETOUCH 0x%02hhx\n", packet[PKT_CMD]);
  //debuglogPacket(packet, length);

  process_onetouch_menu_packet(packet, length);

  // Check for new menu.  
  // Usually PDA_CLEAR bunch of CMD_MSG_LONG then a CMD_PDA_HIGHLIGHT or CMD_PDA_HIGHLIGHTCHARS
  // When we hit page down, just CMD_MSG_LONG then a CMD_PDA_HIGHLIGHT. (not seen CMD_PDA_HIGHLIGHTCHARS yet)
  if ( (filling_menu == true && 
        (//packet[PKT_CMD] == CMD_PDA_HIGHLIGHTCHARS || 
         packet[PKT_CMD] == CMD_PDA_HIGHLIGHT ||
         packet[PKT_CMD] == CMD_STATUS) ) 
        ||
       ( _last_msg_type == CMD_MSG_LONG && packet[PKT_CMD] == CMD_PDA_HIGHLIGHT )
      )
  {
    filling_menu = false;
    rtn = new_menu(aq_data);
    _last_kick_type = KICKT_MENU;
  } else {
    _last_kick_type = KICKT_CMD;
  }

  if (packet[PKT_CMD] == CMD_PDA_CLEAR)
    filling_menu = true;

/*
  //if (_last_msg_type == CMD_MSG_LONG && packet[PKT_CMD] != CMD_MSG_LONG) {
  if (_last_msg_type == CMD_MSG_LONG && ( packet[PKT_CMD] != CMD_MSG_LONG && packet[PKT_CMD] != CMD_PDA_HIGHLIGHTCHARS) ) { // CMD_PDA_SHIFTLINES
    rtn = new_menu(aq_data);
    _last_kick_type = KICKT_MENU;
  } else {
    _last_kick_type = KICKT_CMD;
  }
*/
  _last_msg_type = packet[PKT_CMD];

  // Receive 0x04 for System menu (before 0x02)
  // Receive 0x04 for startup menu (before 0x02)
  // Receive 0x08 for Equiptment menu (before 0x02)
  
  // Receive 0x04 while building menu

  if ( packet[PKT_CMD] == CMD_MSG_LONG)
    logMessage(LOG_DEBUG, "RS received ONETOUCH packet of type %s length %d '%.*s'\n", get_packet_type(packet, length), length, AQ_MSGLEN, (char*)packet+PKT_DATA+1);
  else
    logMessage(LOG_DEBUG, "RS received ONETOUCH packet of type %s length %d\n", get_packet_type(packet, length), length);


  //debuglogPacket(packet, length);

  //if ( in_ot_programming_mode(aq_data) == true )
    kick_aq_program_thread(aq_data, ONETOUCH);
/*
  switch (packet[PKT_CMD])
  {
    case CMD_ACK:
      //logMessage(LOG_DEBUG, "RS Received ACK length %d.\n", length);
      break;
    
    //case CMD_PDA_HIGHLIGHT: // This doesn't work for end of menu, if menu is complete then get a change line, highlight isn't sent.
      //set_macro_status();
    //  break;
    
    case 0x04:
    case 0x08:
      logMessage(LOG_DEBUG, "RS Received MENU complete\n");
      set_macro_status();
      break;
    

    
    case CMD_MSG_LONG:
      msg = (char *)packet + PKT_DATA + 1;
      logMessage(LOG_DEBUG, "RS Received message data 0x%02hhx string '%s'\n",packet[PKT_DATA],msg);
      break;
    
    default:
     //logMessage(LOG_DEBUG, "RS Received 0x%02hhx\n", packet[PKT_CMD]);
     break;
  }
*/
  return rtn;
}


// Check s2 exists in s1
int ot_strcmp(const char *s1, const char *s2)
{
  char *sp1 = (char *)s1;
  char *sp2 = (char *)s2;
  //int i=0;
  // Get rid of all padding
  while(isspace(*sp1)) sp1++;
  while(isspace(*sp2)) sp2++;

  // Need to write this myself for speed
  //logMessage(LOG_DEBUG, "OneTouch compare (reset)%d chars of '%s' to '%s'\n",strlen(sp2),sp1,sp2);
  return strncasecmp(sp1, sp2, strlen(sp2));
}



#define INT_MAX +2147483647
#define INT_MIN -2147483647

// atoi that can have blank start
int ot_atoi(const char* str) 
{ 
    int sign = 1, base = 0, i = 0; 
    // if whitespaces then ignore. 
    while (str[i] == ' ') { 
        i++; 
    } 

    // checking for valid input 
    while (str[i] >= '0' && str[i] <= '9') { 
        // handling overflow test case 
        if (base > INT_MAX / 10 || (base == INT_MAX / 10 && str[i] - '0' > 7)) { 
            if (sign == 1) 
                return INT_MAX; 
            else
                return INT_MIN; 
        } 
        base = 10 * base + (str[i++] - '0'); 
    } 
    return base * sign; 
}

/*
Version something like
Info:   OneTouch Menu Line 0 = 
Info:   OneTouch Menu Line 1 = 
Info:   OneTouch Menu Line 2 = 
Info:   OneTouch Menu Line 3 = 
Info:   OneTouch Menu Line 4 =     B0029221    
Info:   OneTouch Menu Line 5 =    RS-8 Combo   
Info:   OneTouch Menu Line 6 = 
Info:   OneTouch Menu Line 7 =    REV T.0.1    
Info:   OneTouch Menu Line 8 = 
Info:   OneTouch Menu Line 9 = 
Info:   OneTouch Menu Line 10 = 
Info:   OneTouch Menu Line 11 = 
*/
/*
Info:   OneTouch Menu Line 0 =     Set Temp    
Info:   OneTouch Menu Line 1 = 
Info:   OneTouch Menu Line 2 = Pool Heat   90`F
Info:   OneTouch Menu Line 3 = Spa Heat   102`F
Info:   OneTouch Menu Line 4 = 
Info:   OneTouch Menu Line 5 = Maintain     OFF
Info:   OneTouch Menu Line 6 = Hours  12AM-12AM
Info:   OneTouch Menu Line 7 = 
Info:   OneTouch Menu Line 8 = Highlight an
Info:   OneTouch Menu Line 9 = item and press
Info:   OneTouch Menu Line 10 = Select
Info:   OneTouch Menu Line 11 =                 
*/
/*
nfo:   OneTouch Menu Line 0 =  Freeze Protect
Info:   OneTouch Menu Line 1 = 
Info:   OneTouch Menu Line 2 = 
Info:   OneTouch Menu Line 3 = Temp        38`F
Info:   OneTouch Menu Line 4 = 
Info:   OneTouch Menu Line 5 = 
Info:   OneTouch Menu Line 6 = Use Arrow Keys
Info:   OneTouch Menu Line 7 = to set value.
Info:   OneTouch Menu Line 8 = Press SELECT
Info:   OneTouch Menu Line 9 = to continue.
Info:   OneTouch Menu Line 10 = 
Info:   OneTouch Menu Line 11 = 
*/

/*
Pump Stuff  Use Intelliflo|Jandy & last number of line.
(Intelliflo VF you set GPM, not RPM)

Debug:  OneTouch Menu Line 0 = Equipment Status
Debug:  OneTouch Menu Line 1 = 
Debug:  OneTouch Menu Line 2 = Intelliflo VS 3 
Debug:  OneTouch Menu Line 3 =  *** Priming ***
Debug:  OneTouch Menu Line 4 =     Watts: 100  
Debug:  OneTouch Menu Line 5 = 
Debug:  OneTouch Menu Line 6 = 
Debug:  OneTouch Menu Line 7 = 
Debug:  OneTouch Menu Line 8 = 
Debug:  OneTouch Menu Line 9 = 
Debug:  OneTouch Menu Line 10 = 
Debug:  OneTouch Menu Line 11 = 

Debug:  OneTouch Menu Line 0 = Equipment Status
Debug:  OneTouch Menu Line 1 = 
Debug:  OneTouch Menu Line 2 = Intelliflo VS 3 
Debug:  OneTouch Menu Line 3 =      RPM: 2750  
Debug:  OneTouch Menu Line 3 =       RPM: 600  // Option for 3 digit RPM
Debug:  OneTouch Menu Line 4 =     Watts: 55   
Debug:  OneTouch Menu Line 5 = 
Debug:  OneTouch Menu Line 6 = 
Debug:  OneTouch Menu Line 7 = 
Debug:  OneTouch Menu Line 8 = 
Debug:  OneTouch Menu Line 9 = 
Debug:  OneTouch Menu Line 10 = 
Debug:  OneTouch Menu Line 11 = 

Debug:  OneTouch Menu Line 0 = Equipment Status
Debug:  OneTouch Menu Line 1 = 
Debug:  OneTouch Menu Line 2 = Intelliflo VF 2 
Debug:  OneTouch Menu Line 3 =    (Offline)    
Debug:  OneTouch Menu Line 4 = 
Debug:  OneTouch Menu Line 5 = 
Debug:  OneTouch Menu Line 6 = 
Debug:  OneTouch Menu Line 7 = 
Debug:  OneTouch Menu Line 8 = 
Debug:  OneTouch Menu Line 9 = 
Debug:  OneTouch Menu Line 10 = 
Debug:  OneTouch Menu Line 11 = 

Debug:  OneTouch Menu Line 0 = Equipment Status
Debug:  OneTouch Menu Line 1 = 
Debug:  OneTouch Menu Line 2 = Intelliflo VF 2 
Debug:  OneTouch Menu Line 3 =      RPM: 2250  
Debug:  OneTouch Menu Line 4 =     Watts: 55   
Debug:  OneTouch Menu Line 5 =       GPM: 80   
Debug:  OneTouch Menu Line 6 = 
Debug:  OneTouch Menu Line 7 = 
Debug:  OneTouch Menu Line 8 = 
Debug:  OneTouch Menu Line 9 = 
Debug:  OneTouch Menu Line 10 = 
Debug:  OneTouch Menu Line 11 = 

Debug:  OneTouch Menu Line 0 = Equipment Status
Debug:  OneTouch Menu Line 1 = 
Debug:  OneTouch Menu Line 2 = Jandy ePUMP   1 
Debug:  OneTouch Menu Line 3 =      RPM: 1750  
Debug:  OneTouch Menu Line 4 =     Watts: 43   
Debug:  OneTouch Menu Line 5 = 
Debug:  OneTouch Menu Line 6 = 
Debug:  OneTouch Menu Line 7 = 
Debug:  OneTouch Menu Line 8 = 
Debug:  OneTouch Menu Line 9 = 
Debug:  OneTouch Menu Line 10 = 
Debug:  OneTouch Menu Line 11 = 
*/
