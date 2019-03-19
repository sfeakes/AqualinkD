
#ifndef PDA_MENU_H_
#define PDA_MENU_H_


#define PDA_LINES    10 // There is only 9 lines, but add buffer to make shifting easier
#define PDA_MAX_MENU_DEPTH 4

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
  PM_FREEZE_PROTECT_DEVICES
} pda_menu_type;

// PDA Line 4 = POOL MODE    OFF
// PDA Line 5 = POOL HEATER  OFF
// PDA Line 6 = SPA MODE     OFF
// PDA Line 7 = SPA HEATER   OFF
// PDA Line 8 = MENU
// PDA Line 9 = EQUIPMENT ON/OFF

typedef enum pda_home_menu_item_type {
  PHMI_POOL_MODE,
  PHMI_POOL_HEATER,
  PHMI_SPA_MODE,
  PHMI_SPA_HEATER,
  PHMI_MENU,
  PHMI_EQUIPMENT_ON_OFF
} pda_home_menu_item_type;

// PDA Line 0 =    MAIN MENU
// PDA Line 1 =
// PDA Line 2 = HELP           >
// PDA Line 3 = PROGRAM        >
// PDA Line 4 = SET TEMP       >
// PDA Line 5 = SET TIME       >
// PDA Line 6 = PDA OPTIONS    >
// PDA Line 7 = SYSTEM SETUP   >
// PDA Line 8 =
// PDA Line 9 =      BOOST

typedef enum pda_main_menu_item_type {
  PMMI_HELP,
  PMMI_PROGRAM,
  PMMI_SET_TEMP,
  PMMI_SET_TIME,
  PMMI_PDA_OPTIONS,
  PMMI_SYSTEM_SETUP,
  PMMI_BLANK_LINE_8,
  PMMI_BOOST
} pda_main_menu_item_type;

// PDA Line 0 =     SET TEMP
// PDA Line 1 = POOL HEAT   99`F
// PDA Line 2 = SPA HEAT    99`F
// PDA Line 3 =
// PDA Line 4 =
// PDA Line 5 =
// PDA Line 6 =
// PDA Line 7 = Highlight an
// PDA Line 8 = item and press
// PDA Line 9 = SELECT

typedef enum pda_set_temp_item_type {
  PSTI_POOL_HEAT,
  PSTI_SPA_HEAT
} pda_set_temp_item_type;

// PDA Line 0 =   SYSTEM SETUP
// PDA Line 1 = LABEL AUX      >
// PDA Line 2 = FREEZE PROTECT >
// PDA Line 3 = AIR TEMP       >
// PDA Line 4 = DEGREES C/F    >
// PDA Line 5 = TEMP CALIBRATE >
// PDA Line 6 = SOLAR PRIORITY >
// PDA Line 7 = PUMP LOCKOUT   >
// PDA Line 8 = ASSIGN JVAs    >
// PDA Line 9 =    ^^ MORE __
// PDA Line 5 = COLOR LIGHTS   >
// PDA Line 6 = SPA SWITCH     >
// PDA Line 7 = SERVICE INFO   >
// PDA Line 8 = CLEAR MEMORY   >

typedef enum pda_system_setup_item_type {
  PSSI_LABEL_AUX,
  PSSI_FREEZE_PROTECT,
  PSSI_AIR_TEMP,
  PSSI_DEGREES_C_F,
  PSSI_TEMP_CALIBRATE,
  PSSI_SOLAR_PRIORITY,
  PSSI_PUMP_LOCKOUT,
  PSSI_ASSIGN_JVAs,
  PSSI_COLOR_LIGHTS,
  PSSI_SPA_SWITCH,
  PSSI_SERVICE_INFO,
  PSSI_CLEAR_MEMORY
} pda_system_setup_item_type;

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
