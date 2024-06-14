
#ifndef ONETOUCH_H_
#define ONETOUCH_H_

#include "aq_serial.h"
#include "aqualink.h"

#define ONETOUCH_LINES 12



#define KICKT_CMD  0
#define KICKT_MENU 1

typedef enum ot_menu_type {
  OTM_ONETOUCH,
  OTM_EQUIPTMENT_STATUS,
  OTM_SYSTEM,  // Default screen with date,time & temperature
  OTM_MAIN,
  OTM_EQUIPTMENT,
  OTM_EQUIPTMENT_ONOFF,
  OTM_SELECT_SPEED,
  OTM_MENUHELP,
  OTM_VERSION,
  OTM_SET_TEMP,
  OTM_SET_TIME,
  OTM_SYSTEM_SETUP,
  OTM_FREEZE_PROTECT,
  OTM_SET_AQUAPURE,
  OTM_BOOST,
  OTM_UNKNOWN
} ot_menu_type;

struct ot_macro {
  char name[AQ_MSGLEN];
  bool ison;
};

/*

Left Top Button
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x03|0x96|0x10|0x03|

Left Middle Button (Back)
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x02|0x95|0x10|0x03|

Left Botom Button
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x01|0x94|0x10|0x03|

Select Button
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x04|0x97|0x10|0x03|

Up Button
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x06|0x99|0x10|0x03|

Down Button
Ack | HEX: 0x10|0x02|0x00|0x01|0x80|0x05|0x98|0x10|0x03|


*/

bool process_onetouch_packet(unsigned char *packet, int length,struct aqualinkdata *aq_data);
ot_menu_type get_onetouch_menu_type();
unsigned char *last_onetouch_packet();
int thread_kick_type();

int onetouch_menu_hlightindex();
int onetouch_menu_hlightcharindex();
char *onetouch_menu_hlight();
char *onetouch_menu_line(int index);
char *onetouch_menu_hlightchars(int *len);
int onetouch_menu_find_index(char *text);
int ot_atoi(const char* str);
int ot_strcmp(const char *s1, const char *s2);
void set_onetouch_lastmsg(unsigned char msgtype);




#endif // ONETOUCH_H_