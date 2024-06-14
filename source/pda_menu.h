
#ifndef PDA_MENU_H_
#define PDA_MENU_H_


#define PDA_LINES    10 // There is only 9 lines, but add buffer to make shifting easier

typedef enum pda_menu_type {
  PM_UNKNOWN,
  PM_FW_VERSION,
  PM_HOME,
  PM_BUILDING_HOME,
  PM_MAIN,
  PM_DIAGNOSTICS,
  PM_PROGRAM,
  PM_SET_TEMP,
  PM_SET_TIME,
  PM_POOL_HEAT,
  PM_SPA_HEAT,
  PM_AQUAPURE,
  PM_SYSTEM_SETUP,
  PM_AUX_LABEL,
  PM_AUX_LABEL_DEVICE,  // label a specific device
  PM_FREEZE_PROTECT,
  PM_FREEZE_PROTECT_DEVICES,
  PM_VSP,
  PM_SETTINGS,
  PM_EQUIPTMENT_CONTROL,
  PM_EQUIPTMENT_STATUS,
  PM_PALM_OPTIONS, // This seems to be only older revisions
  PM_BOOST
} pda_menu_type;

/*
typedef enum pda_home_menu_item {
  PMI_MAIN,
  PMI_EQUIPTMENT_CONTROL
} pda_home_menu_item;
*/

/*
typedef enum pda_menu_type {
  PM_UNKNOWN,
  PM_MAIN,
  PM_SETTINGS,
  PM_EQUIPTMENT_CONTROL,
  PM_EQUIPTMENT_STATUS,
  PM_BUILDING_MAIN
} pda_menu_type;
*/
/*
typedef enum pda_menu_type {
  PM_UNKNOWN,
  PM_FW_VERSION,
  PM_HOME,
  PM_MAIN_MENU,
  PM_EQUIPTMENT_CONTROL,
  PM_EQUIPTMENT_STATUS,
  PM_BUILDING_HOME,
  PM_SYSTEM_SETUP,
  PM_SET_TEMP,
  PM_POOL_HEAT,
  PM_SPA_HEAT,
  PM_FREEZE_PROTECT,
  PM_FREEZE_PROTECT_DEVICES,
  PM_SET_AQUAPURE
} pda_menu_type;
*/
//bool pda_mode();
//void set_pda_mode(bool val);
bool process_pda_menu_packet(unsigned char* packet, int length, bool force_print_menu);
int pda_m_hlightindex();
char *pda_m_hlight();
char *pda_m_line(int index);
pda_menu_type pda_m_type();
int pda_find_m_index(char *text);
int pda_find_m_index_case(char *text, int limit);
int pda_find_m_index_loose(char *text);
char *pda_m_hlightchars(int *len);
//int pda_find_m_index_swcase(char *text, int limit);

#endif
