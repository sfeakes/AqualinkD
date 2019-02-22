
#ifndef PDA_MENU_H_
#define PDA_MENU_H_


#define PDA_LINES    10 // There is only 9 lines, but add buffer to make shifting easier
#define PDA_MAX_MENU_DEPTH 4

typedef enum pda_menu_type {
  PM_UNKNOWN,
  PM_FW_VERSION,
  PM_MAIN,
  PM_SETTINGS,
  PM_EQUIPTMENT_CONTROL,
  PM_EQUIPTMENT_STATUS,
  PM_BUILDING_MAIN
} pda_menu_type;

// PDA Line 4 = POOL MODE    OFF
// PDA Line 5 = POOL HEATER  OFF
// PDA Line 6 = SPA MODE     OFF
// PDA Line 7 = SPA HEATER   OFF
// PDA Line 8 = MENU
// PDA Line 9 = EQUIPMENT ON/OFF

typedef enum pda_main_menu_item_type {
  PMMI_POOL_MODE,
  PMMI_POOL_HEATER,
  PMMI_SPA_MODE,
  PMMI_SPA_HEATER,
  PMMI_MENU,
  PMMI_EQUIPMENT_ON_OFF
} pda_main_menu_item_type;

bool pda_mode();
void set_pda_mode(bool val);
bool process_pda_menu_packet(unsigned char* packet, int length);
int pda_m_hlightindex();
char *pda_m_hlight();
char *pda_m_line(int index);
pda_menu_type pda_m_type();
bool pda_m_clear();
bool update_pda_menu_type();
bool wait_pda_m_hlightindex_update(struct timespec *max_wait);
bool wait_pda_m_hlightindex_change(struct timespec *max_wait);
int wait_pda_m_hlightindex(struct timespec *max_wait);
bool wait_pda_m_type_change(struct timespec *max_wait);
bool wait_pda_m_type(pda_menu_type menu, struct timespec *max_wait);
bool wait_pda_m_update_complete(struct timespec *max_wait);

#endif
