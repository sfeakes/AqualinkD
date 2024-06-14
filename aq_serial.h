
#ifndef AQ_SERIAL_H_
#define AQ_SERIAL_H_

#include <termios.h>
#include <stdbool.h>

#include "aq_programmer.h" // Need this for function getJandyDeviceType due to enum defined their.
emulation_type getJandyDeviceType(unsigned char ID);
const char *getJandyDeviceName(emulation_type etype);

#define CONNECTION_ERROR "ERROR No connection to RS control panel"
#ifdef AQ_MANAGER
#define CONNECTION_RUNNING_SLOG "Running serial_logger, this will take some time"
#endif

#define SERIAL_BLOCKING_TIME 50 // (1 to 255) in 1/10th second so 1 = 0.1 sec, 255 = 25.5 sec

// Protocol types
#define PCOL_JANDY     0xFF
#define PCOL_PENTAIR   0xFE
#define PCOL_UNKNOWN   0xFD


// packet offsets
#define PKT_DEST        2
#define PKT_CMD         3
#define PKT_DATA        4

#define PKT_STATUS_BYTES 5


#define DEV_MASTER      0x00
#define SWG_DEV_ID      0x50
#define IAQ_DEV_ID      0x33

/* Few Device ID's in decimal for quick checking
#  Pentair pump ID's
#  0x60 to 0x6F (0x60, 0x61 0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F)
#  Jandy pump ID's
#  0x78, 0x79, 0x7A, 0x7B
*/
#define PENTAIR_DEC_PUMP_MIN   96   // 0x60
#define PENTAIR_DEC_PUMP_MAX  111   // 0x6F
#define JANDY_DEC_SWG_MIN      80   // 0x50
#define JANDY_DEC_SWG_MAX      83   // 0x53
#define JANDY_DEC_PUMP_MIN    120   // 0x78
#define JANDY_DEC_PUMP_MAX    123   // 0x7b
#define JANDY_DEC_JXI_MIN     104   // 0x68
#define JANDY_DEC_JXI_MAX     107   // 0x6B
#define JANDY_DEC_LX_MIN       56   // 0x38
#define JANDY_DEC_LX_MAX       59   // 0x3B 
#define JANDY_DEC_CHEM_MIN    128   // 0x80
#define JANDY_DEC_CHEM_MAX    131   // 0x83


// PACKET DEFINES Jandy
#define NUL  0x00
#define DLE  0x10
#define STX  0x02
#define ETX  0x03

// Pentair packet headder (first 4 bytes)
#define PP1 0xFF
#define PP2 0x00
#define PP3 0xFF
#define PP4 0xA5

#define PEN_DEV_MASTER 0x10

#define PEN_CMD_SPEED     0x01
#define PEN_CMD_REMOTECTL 0x04
#define PEN_CMD_POWER     0x06
#define PEN_CMD_STATUS    0x07



#define PEN_PKT_FROM 6
#define PEN_PKT_DEST 5
#define PEN_PKT_CMD 7

// Pentair VSP
#define PEN_MODE        10
#define PEN_DRIVE_STATE 11
#define PEN_HI_B_WAT    12
#define PEN_LO_B_WAT    13
#define PEN_HI_B_RPM    14
#define PEN_LO_B_RPM    15
#define PEN_FLOW        16
#define PEN_PPC         17  // Pump pressure curve
#define PEN_HI_B_STATUS 20  // The current status value of the pump. following values: ok, filterWarning, overCurrent, priming, systemBlocked, generalAlarm, powerOutage, overCurrent2, overVoltage, commLost
#define PEN_LO_B_STATUS 21
// END Pentair

#define AQ_MINPKTLEN    5
//#define AQ_MAXPKTLEN   64
#define AQ_MAXPKTLEN   128  // Max 79 bytes so far, so 128 is a guess at the moment, just seen large packets from iAqualink
#define AQ_PSTLEN       5
#define AQ_MSGLEN      16
#define AQ_MSGLONGLEN 128
#define AQ_TADLEN      13

/* COMMANDS */
#define CMD_PROBE       0x00
#define CMD_ACK         0x01
#define CMD_STATUS      0x02
#define CMD_MSG         0x03
#define CMD_MSG_LONG    0x04
#define CMD_MSG_LOOP_ST 0x08

/* ACK RETURN COMMANDS */
/*
#define ACK_NORMAL               0x00
#define ACK_SCREEN_BUSY_SCROLL   0x01 // Seems to be busy displaying last message, but can cache next message,
#define ACK_SCREEN_BUSY_BLOCK    0x03 // Seems to be don't send me any more shit.
*/
// Some keypads use 0x00 some 0x80 (think it's something to do with version, but need to figure it out)
// But if you use 0x80 for ack then you get a start loop cycle CMD_MSG_LOOP_ST
#define ACK_NORMAL               0x80
#define ACK_SCREEN_BUSY_SCROLL   0x81 // Seems to be busy displaying last message, but can cache next message,
#define ACK_SCREEN_BUSY_BLOCK    0x83 // Seems to be don't send me any more shit.



// Remove this and fix all compile errors when get time.
#define ACK_SCREEN_BUSY ACK_SCREEN_BUSY_SCROLL

#define ACK_IAQ_TOUCH            0x00
#define ACK_PDA                  0x40
#define ACK_ONETOUCH             0x80
#define ACK_ALLB_SIM             0x80 // Jandy's Allbutton simulator uses this and not ACK_NORMAL
#define ACK_ALLB_SIM_BUSY        0x81 // Jandy's Allbutton simulator uses this and not ACK_SCREEN_BUSY_SCROLL

/* ONE TOUCH KEYCODES */
#define KEY_ONET_UP              0x06
#define KEY_ONET_DOWN            0x05
#define KEY_ONET_SELECT          0x04
#define KEY_ONET_PAGE_UP         0x03  // Top
#define KEY_ONET_BACK            0x02  // Middle
#define KEY_ONET_PAGE_DN         0x01  // Bottom
#define KEY_ONET_SELECT_1        KEY_ONET_PAGE_UP  
#define KEY_ONET_SELECT_2        KEY_ONET_BACK  
#define KEY_ONET_SELECT_3        KEY_ONET_PAGE_DN  

/* AquaRite commands */
#define CMD_GETID       0x14  // May be remote control control
#define CMD_PERCENT     0x11  // Set Percent
#define CMD_PPM         0x16  // Received PPM

/* LXi Heater commands */
#define CMD_JXI_PING     0x0c
#define CMD_JXI_STATUS   0x0d

/* PDA KEY CODES */  // Just plating at the moment
#define KEY_PDA_UP     0x06
#define KEY_PDA_DOWN   0x05
#define KEY_PDA_BACK   0x02
#define KEY_PDA_SELECT 0x04
//#define KEY_PDA_PGUP   0x01 // Think these are hot key #1
//#define KEY_PDA_PGDN   0x03 // Think these are hot key #2

/* KEY/BUTTON CODES */
#define KEY_PUMP      0x02
#define KEY_SPA       0x01
#define KEY_AUX1      0x05
#define KEY_AUX2      0x0a
#define KEY_AUX3      0x0f
#define KEY_AUX4      0x06
#define KEY_AUX5      0x0b
#define KEY_AUX6      0x10
#define KEY_AUX7      0x15
#define KEY_POOL_HTR  0x12
#define KEY_SPA_HTR   0x17
#define KEY_SOLAR_HTR 0x1c
#define KEY_MENU      0x09
#define KEY_CANCEL    0x0e
#define KEY_LEFT      0x13
#define KEY_RIGHT     0x18
#define KEY_HOLD      0x19
#define KEY_OVERRIDE  0x1e
#define KEY_ENTER     0x1d

#ifdef AQ_RS16
//RS 12 & 16 are different from Aux4 to Aux7
#define KEY_RS16_AUX4      0x14
#define KEY_RS16_AUX5      0x03
#define KEY_RS16_AUX6      0x07
#define KEY_RS16_AUX7      0x06
// RS 12 & 16 have extra buttons
#define KEY_AUXB1     0x0b
#define KEY_AUXB2     0x10
#define KEY_AUXB3     0x15
#define KEY_AUXB4     0x1a
#define KEY_AUXB5     0x04
#define KEY_AUXB6     0x08
#define KEY_AUXB7     0x0d
#define KEY_AUXB8     0x0c
// End diff in RS12
#endif

#define BTN_PUMP      "Filter_Pump"
#define BTN_SPA       "Spa_Mode"
#define BTN_AUX1      "Aux_1"
#define BTN_AUX2      "Aux_2"
#define BTN_AUX3      "Aux_3"
#define BTN_AUX4      "Aux_4"
#define BTN_AUX5      "Aux_5"
#define BTN_AUX6      "Aux_6"
#define BTN_AUX7      "Aux_7"
#define BTN_POOL_HTR  "Pool_Heater"
#define BTN_SPA_HTR   "Spa_Heater"
#define BTN_SOLAR_HTR "Solar_Heater"

#define BTN_TEMP1_HTR  "Temp1_Heater"
#define BTN_TEMP2_HTR  "Temp2_Heater"

#ifdef AQ_RS16
#define BTN_AUXB1      "Aux_B1"
#define BTN_AUXB2      "Aux_B2"
#define BTN_AUXB3      "Aux_B3"
#define BTN_AUXB4      "Aux_B4"
#define BTN_AUXB5      "Aux_B5"
#define BTN_AUXB6      "Aux_B6"
#define BTN_AUXB7      "Aux_B7"
#define BTN_AUXB8      "Aux_B8"
#endif

#define BTN_PDA_PUMP      "FILTER PUMP"
#define BTN_PDA_SPA       "SPA"
#define BTN_PDA_AUX1      "AUX1"
#define BTN_PDA_AUX2      "AUX2"
#define BTN_PDA_AUX3      "AUX3"
#define BTN_PDA_AUX4      "AUX4"
#define BTN_PDA_AUX5      "AUX5"
#define BTN_PDA_AUX6      "AUX6"
#define BTN_PDA_AUX7      "AUX7"
#define BTN_PDA_POOL_HTR  "POOL HEAT"
#define BTN_PDA_SPA_HTR   "SPA HEAT"
#define BTN_PDA_SOLAR_HTR "EXTRA AUX"

#define BUTTON_LABEL_LENGTH 20

#ifndef AQ_RS16
#define TOTAL_LEDS          20
#else
#define TOTAL_LEDS          24  // Only 20 exist in control panel, but need space for the extra buttons on RS16 panel
#endif

// Index starting at 1
#define POOL_HTR_LED_INDEX   15
#define SPA_HTR_LED_INDEX    17
#define SOLAR_HTR_LED_INDEX  19

#define LNG_MSG_SERVICE_ACTIVE        "SERVICE MODE IS ACTIVE"
#define LNG_MSG_TIMEOUT_ACTIVE        "TIMEOUT MODE IS ACTIVE"
#define LNG_MSG_POOL_TEMP_SET         "POOL TEMP IS SET TO"
#define LNG_MSG_SPA_TEMP_SET          "SPA TEMP IS SET TO"
#define LNG_MSG_FREEZE_PROTECTION_SET "FREEZE PROTECTION IS SET TO"
#define LNG_MSG_CLEANER_DELAY         "CLEANER WILL TURN ON AFTER SAFETY DELAY"
#define LNG_MSG_BATTERY_LOW           "BATTERY LOW"
//#define LNG_MSG_FREEZE_PROTECTION_ACTIVATED "FREEZE PROTECTION ACTIVATED"
#define LNG_MSG_FREEZE_PROTECTION_ACTIVATED "FREEZE PROTECTION IS ACTIVATED"

// These are
#define LNG_MSG_CHEM_FEED_ON          "CHEM FEED ON"
#define LNG_MSG_CHEM_FEED_OFF         "CHEM FEED OFF"


#define MSG_AIR_TEMP   "AIR TEMP"
#define MSG_POOL_TEMP  "POOL TEMP"
#define MSG_SPA_TEMP   "SPA TEMP"
#define MSG_AIR_TEMP_LEN   8
#define MSG_POOL_TEMP_LEN  9
#define MSG_SPA_TEMP_LEN   8

// Will get water temp rather than pool in some cases. not sure if it's REV specific or device (ie no spa) specific yet
#define MSG_WATER_TEMP       "WATER TEMP"
#define MSG_WATER_TEMP_LEN   10
#define LNG_MSG_WATER_TEMP1_SET "TEMP1 (HIGH TEMP) IS SET TO"
#define LNG_MSG_WATER_TEMP2_SET "TEMP2 (LOW TEMP) IS SET TO"

/*
// All Messages listed in the manual, This is obviously not complete, but it's everything Jandy has published
BATTERY IS LOW, BATTERY LOCATED AT THE POWER CENTER
CLEANER CANNOT BE TURNED ON WHILE SPA IS ON 
CLEANER CANNOT BE TURNED ON WHILE SPILLOVER IS ON
FREEZE PROTECTION ACTIVATED
SENSOR OPENED
POOL HEATER ENABLED
PUMP WILL REMAIN ON WHILE SPILLOVER IS ON
PUMP WILL TURN OFF AFTER COOL DOWN CYCLE
PUMP WILL TURN ON AFTER DELAY 
SERVICE MODE IS ACTIVE
SENSOR SHORTED
SPA WILL TURN OFF AFTER COOL DOWN CYCLE
TIMED AUX ON, WILL TURN OFF AFTER 30 MINUTES
TIMEOUT MODE IS ACTIVE
SPILLOVER IS DISABLED WHILE SPA IS ON
*/

#define MSG_SWG_PCT   "AQUAPURE"  // AquaPure 55%
#define MSG_SWG_PPM   "SALT"  // Salt 3000 PPM 
#define MSG_SWG_PCT_LEN  8
#define MSG_SWG_PPM_LEN  4

#define MSG_SWG_NO_FLOW   "Check AQUAPURE No Flow"
#define MSG_SWG_LOW_SALT  "Check AQUAPURE Low Salt"
#define MSG_SWG_HIGH_SALT "Check AQUAPURE High Salt"
#define MSG_SWG_FAULT     "Check AQUAPURE General Fault"

#define MSG_PMP_RPM   "RPM:" 
#define MSG_PMP_WAT   "Watts:"  
#define MSG_PMP_GPM   "GPM:" 


/* AQUAPURE SWG */

// These are madeup.
//#define SWG_STATUS_OFF     0xFF
//#define SWG_STATUS_OFFLINE 0xFE

//#define SWG_STATUS_UNKNOWN -128 // Idiot.  unsigned char....Derr.
#define SWG_STATUS_OFF      0xFF // Documented this as off in API, so don't change.
#define SWG_STATUS_UNKNOWN  0xFE 
#define SWG_STATUS_GENFAULT 0xFD //This is displayed in the panel, so adding it
// These are actual from RS485

#define SWG_STATUS_ON           0x00
#define SWG_STATUS_NO_FLOW      0x01 // no flow 0x01
#define SWG_STATUS_LOW_SALT     0x02 // low salt 0x02
//#define SWG_STATUS_VLOW_SALT    0x04 // very low salt 0x04
#define SWG_STATUS_HI_SALT      0x04 // high salt 0x04
#define SWG_STATUS_CLEAN_CELL   0x08 // clean cell 0x10
#define SWG_STATUS_TURNING_OFF  0x09 // turning off 0x09
#define SWG_STATUS_HIGH_CURRENT 0x10 // high current 0x08
#define SWG_STATUS_LOW_VOLTS    0x20 // low voltage 0x20
#define SWG_STATUS_LOW_TEMP     0x40 // low watertemp 0x40
#define SWG_STATUS_CHECK_PCB    0x80 // check PCB 0x80
// Other SWG codes not deciphered yes 0x03 & 0x0b seem to be messages when salt is low and turning on / off

#define CMD_PDA_0x04           0x04 // No idea, might be building menu
#define CMD_PDA_0x05           0x05 // No idea
#define CMD_PDA_0x1B           0x1b
#define CMD_PDA_HIGHLIGHT      0x08
#define CMD_PDA_CLEAR          0x09
#define CMD_PDA_SHIFTLINES     0x0F
#define CMD_PDA_HIGHLIGHTCHARS 0x10

/* ePump */
#define CMD_EPUMP_STATUS       0x1F
#define CMD_EPUMP_RPM          0x44
#define CMD_EPUMP_WATTS        0x45
// One Touch commands
//#define CMD_PDA_0x04           0x04 // No idea, might be building menu

/* iAqualink */

#define CMD_IAQ_PAGE_MSG      0x25
#define CMD_IAQ_TABLE_MSG     0x26  // ??? Some form of table populate
#define CMD_IAQ_PAGE_BUTTON   0x24
#define CMD_IAQ_PAGE_START    0x23  // Start a new menu // wait for 0x28 before sending anything
#define CMD_IAQ_PAGE_END      0x28  // Some kind a finished
#define CMD_IAQ_STARTUP       0x29  //  Startup message
#define CMD_IAQ_POLL          0x30  // Poll message or ready to receive command
#define CMD_IAQ_CTRL_READY    0x31  // Get this when we can send big control command
#define CMD_IAQ_PAGE_CONTINUE 0x40  // Seems we get this on AUX device page when there is another page, keeps circuling through pages.
#define CMD_IAQ_TITLE_MESSAGE 0x2d  // This is what the product name is set to (Jandy RS) usually
//#define CMD_IAQ_VSP_ERROR     0x2c  // Error when setting speed too high
#define CMD_IAQ_MSG_LONG      0x2c  // This this is display popup message.  Next 2 bytes 0x00|0x01 = wait and then 0x00|0x00 clear
/*
#define CMD_IAQ_MSG_3         0x2d  // Equiptment status message??
#define CMD_IAQ_0x31          0x31 // Some pump speed info
*/
#define ACK_CMD_READY_CTRL      0x80 // Send this before sending big control command


#define KEY_IAQTCH_HOME          0x01
#define KEY_IAQTCH_MENU          0x02
#define KEY_IAQTCH_ONETOUCH      0x03
#define KEY_IAQTCH_HELP          0x04
#define KEY_IAQTCH_BACK          0x05
#define KEY_IAQTCH_STATUS        0x06
#define KEY_IAQTCH_PREV_PAGE     0x20
#define KEY_IAQTCH_NEXT_PAGE     0x21
#define KEY_IAQTCH_OK            0x01 //HEX: 0x10|0x02|0x00|0x01|0x00|0x01|0x14|0x10|0x03|. OK BUTTON

#define KEY_IAQTCH_PREV_PAGE_ALTERNATE   0x1d // System setup prev
#define KEY_IAQTCH_NEXT_PAGE_ALTERNATE   0x1e // System setup next

// PAGE1 (Horosontal keys) (These are duplicate so probable delete)
#define KEY_IAQTCH_HOMEP_KEY01   0x11
#define KEY_IAQTCH_HOMEP_KEY02   0x12
#define KEY_IAQTCH_HOMEP_KEY03   0x13
#define KEY_IAQTCH_HOMEP_KEY04   0x14
#define KEY_IAQTCH_HOMEP_KEY05   0x15
#define KEY_IAQTCH_HOMEP_KEY06   0x16
#define KEY_IAQTCH_HOMEP_KEY07   0x17
#define KEY_IAQTCH_HOMEP_KEY08   0x18 // Other Devices (may not be able to change)

// Numbering is colum then row.
#define KEY_IAQTCH_KEY01         0x11  // Column 1 row 1
#define KEY_IAQTCH_KEY02         0x12  // column 1 row 2
#define KEY_IAQTCH_KEY03         0x13  // column 1 row 3
#define KEY_IAQTCH_KEY04         0x14  // column 1 row 4
#define KEY_IAQTCH_KEY05         0x15  // column 1 row 5

#define KEY_IAQTCH_KEY06         0x16  // column 2 row 1
#define KEY_IAQTCH_KEY07         0x17  // column 2 row 2
#define KEY_IAQTCH_KEY08         0x18  // column 2 row 3
#define KEY_IAQTCH_KEY09         0x19  // column 2 row 4
#define KEY_IAQTCH_KEY10         0x1a  // column 2 row 5

#define KEY_IAQTCH_KEY11         0x1b  // column 3 row 1
#define KEY_IAQTCH_KEY12         0x1c  // column 3 row 2
#define KEY_IAQTCH_KEY13         0x1d  // column 3 row 3
#define KEY_IAQTCH_KEY14         0x1e  // column 3 row 4
#define KEY_IAQTCH_KEY15         0x1f  // column 3 row 5

#define IAQ_PAGE_HOME            0x01
#define IAQ_PAGE_STATUS          0x5b
#define IAQ_PAGE_STATUS2         0x2a // Something get this for Status rather than 0x5b
#define IAQ_PAGE_DEVICES         0x36
#define IAQ_PAGE_DEVICES2        0x35
#define IAQ_PAGE_DEVICES3        0x51
#define IAQ_PAGE_SET_TEMP        0x39
#define IAQ_PAGE_MENU            0x0f
#define IAQ_PAGE_SET_VSP         0x1e
#define IAQ_PAGE_SET_TIME        0x4b
#define IAQ_PAGE_SET_DATE        0x4e
#define IAQ_PAGE_SET_SWG         0x30
#define IAQ_PAGE_SET_BOOST       0x1d
#define IAQ_PAGE_SET_QBOOST      0x3f
#define IAQ_PAGE_ONETOUCH        0x4d
#define IAQ_PAGE_COLOR_LIGHT     0x48
#define IAQ_PAGE_SYSTEM_SETUP    0x14
#define IAQ_PAGE_SYSTEM_SETUP2   0x49
#define IAQ_PAGE_SYSTEM_SETUP3   0x4a
#define IAQ_PAGE_VSP_SETUP       0x2d
#define IAQ_PAGE_FREEZE_PROTECT  0x11
#define IAQ_PAGE_LABEL_AUX       0x32
#define IAQ_PAGE_HELP            0x0c
//#define IAQ_PAGE_START_BOOST     0x3f
//#define IAQ_PAGE_DEGREES         0xFF // Added this as never want to actually select the page, just go to it.



#define RSSA_DEV_STATUS 0x13
#define RSSA_DEV_READY  0x07  // Ready to receive change command
// For the moment, rest of RS_RA are in serialadapter.h


// Errors from get_packet
#define AQSERR_READ     -1 // General fileIO read error
#define AQSERR_TIMEOUT  -2 // Timeout
#define AQSERR_CHKSUM   -3 // Checksum failed
#define AQSERR_2LARGE   -4 // Buffer Overflow
#define AQSERR_2SMALL   -5 // Not enough read


// At the moment just used for next ack
typedef enum {
  DRS_NONE,
  DRS_SWG,
  DRS_EPUMP,
  DRS_JXI,
  DRS_LX,
  DRS_CHEM
} rsDeviceType;

typedef enum {
  ON,
  OFF,
  FLASH,
  ENABLE,
  LED_S_UNKNOWN
} aqledstate;

typedef struct aqualinkled
{
  //int number;
  aqledstate state;
} aqled;

// Battery Status Identifiers
enum {
	OK = 0,
	LOW
};

typedef enum {
  JANDY,
  PENTAIR,
  P_UNKNOWN
} protocolType;


int init_serial_port(const char* tty);
int init_blocking_serial_port(const char* tty);
//int init_readahead_serial_port(const char* tty);

void close_serial_port(int file_descriptor);
void close_blocking_serial_port();
bool serial_blockingmode();

//#ifdef AQ_PDA
//void set_pda_mode(bool mode);
//bool pda_mode();
//#endif
int generate_checksum(unsigned char* packet, int length);
protocolType getProtocolType(unsigned char* packet);
bool check_jandy_checksum(unsigned char* packet, int length);
bool check_pentair_checksum(unsigned char* packet, int length);
void send_ack(int file_descriptor, unsigned char command);
void send_extended_ack(int fd, unsigned char ack_type, unsigned char command);
//void send_cmd(int file_descriptor, unsigned char cmd, unsigned char args);
int get_packet(int file_descriptor, unsigned char* packet);
//int get_packet_lograw(int fd, unsigned char* packet);

//int get_packet_new(int fd, unsigned char* packet);
//int get_packet_new_lograw(int fd, unsigned char* packet);
//void close_serial_port(int file_descriptor, struct termios* oldtio);
//void process_status(void const * const ptr);
void process_status(unsigned char* ptr);
const char* get_packet_type(unsigned char* packet , int length);
/*
void set_onetouch_enabled(bool mode);
bool onetouch_enabled();
void set_iaqtouch_enabled(bool mode);
bool iaqtouch_enabled();
bool VSP_enabled();

void set_extended_device_id_programming(bool mode);
bool extended_device_id_programming();
*/

void send_jandy_command(int fd, unsigned char *packet_buffer, int size);
void send_pentair_command(int fd, unsigned char *packet_buffer, int size);
void send_command(int fd, unsigned char *packet_buffer, int size);

/*
#ifdef ONETOUCH
void set_onetouch_mode(bool mode);
bool onetouch_mode();
#endif
*/
//void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
//void send_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
//void send_messaged(int fd, unsigned char destination, char *message);
#endif // AQ_SERIAL_H_