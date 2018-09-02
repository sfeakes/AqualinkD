
#ifndef PDA_MENU_H_
#define PDA_MENU_H_


#define PDA_LINES    10 // There is only 9 lines, but add buffer to make shifting easier

typedef enum pda_menu_type {
  PM_UNKNOWN,
  PM_MAIN,
  PM_SETTINGS,
  PM_EQUIPTMENT_CONTROL,
  PM_EQUIPTMENT_STATUS
} pda_menu_type;

bool pda_mode();
void set_pda_mode(bool val);
bool process_pda_menu_packet(unsigned char* packet, int length);
int pda_m_hlightindex();
char *pda_m_hlight();
char *pda_m_line(int index);
pda_menu_type pda_m_type();

#endif