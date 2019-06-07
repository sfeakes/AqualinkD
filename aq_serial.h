
#ifndef AQ_SERIAL_H_
#define AQ_SERIAL_H_

#include <termios.h>

#define CONNECTION_ERROR "ERROR No connection to RS control panel"

// packet offsets
#define PKT_DEST        2
#define PKT_CMD         3
#define PKT_DATA        4

#define PKT_STATUS_BYTES 5


#define DEV_MASTER      0x00
#define SWG_DEV_ID      0x50
#define IAQ_DEV_ID      0x33

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

#define PEN_CMD_STATUS 0x07

#define PEN_PKT_FROM 6
#define PEN_PKT_DEST 5
#define PEN_PKT_CMD 7

#define PEN_HI_B_RPM 14
#define PEN_LO_B_RPM 15
#define PEN_HI_B_WAT 12
#define PEN_LO_B_WAT 13
// END Pentair

#define AQ_MINPKTLEN    5
#define AQ_MAXPKTLEN   64
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

/* ACK RETURN COMMANDS */
#define ACK_NORMAL               0x00
#define ACK_SCREEN_BUSY          0x01 // Seems to be busy but can cache a message,
#define ACK_SCREEN_BUSY_BLOCK    0x03 // Seems to be don't send me shit.
#define ACK_PDA                  0x40

/* AquaRite commands */
#define CMD_GETID       0x14  // May be remote control control
#define CMD_PERCENT     0x11  // Set Percent
#define CMD_PPM         0x16  // Received PPM

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
#define TOTAL_LEDS          20

// Index starting at 1
#define POOL_HTR_LED_INDEX   15
#define SPA_HTR_LED_INDEX    17
#define SOLAR_HTR_LED_INDEX  19

#define LNG_MSG_SERVICE_ACTIVE        "SERVICE MODE IS ACTIVE"
#define LNG_MSG_POOL_TEMP_SET         "POOL TEMP IS SET TO"
#define LNG_MSG_SPA_TEMP_SET          "SPA TEMP IS SET TO"
#define LNG_MSG_FREEZE_PROTECTION_SET "FREEZE PROTECTION IS SET TO"
#define LNG_MSG_CLEANER_DELAY         "CLEANER WILL TURN ON AFTER SAFETY DELAY"
#define LNG_MSG_BATTERY_LOW           "BATTERY LOW"
//#define LNG_MSG_FREEZE_PROTECTION_ACTIVATED "FREEZE PROTECTION ACTIVATED"
#define LNG_MSG_FREEZE_PROTECTION_ACTIVATED "FREEZE PROTECTION IS ACTIVATED"


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

#define MSG_PMP_RPM   "RPM:" 
#define MSG_PMP_WAT   "Watts:"  
#define MSG_PMP_GPH   "GPH:" 


/* AQUAPURE SWG */
// These are madeup.
#define SWG_STATUS_OFF     0xFF
#define SWG_STATUS_UNKNOWN -128
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

#define CMD_PDA_0x05           0x05
#define CMD_PDA_0x1B           0x1b
#define CMD_PDA_HIGHLIGHT      0x08
#define CMD_PDA_CLEAR          0x09
#define CMD_PDA_SHIFTLINES     0x0F
#define CMD_PDA_HIGHLIGHTCHARS 0x10

/* iAqualink */
#define CMD_IAQ_MSG           0x25
#define CMD_IAQ_MENU_MSG      0x24



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


int init_serial_port(char* tty);
void close_serial_port(int file_descriptor);
void set_pda_mode(bool mode);
bool pda_mode();
int generate_checksum(unsigned char* packet, int length);
protocolType getProtocolType(unsigned char* packet);
bool check_jandy_checksum(unsigned char* packet, int length);
bool check_pentair_checksum(unsigned char* packet, int length);
void send_ack(int file_descriptor, unsigned char command);
void send_extended_ack(int fd, unsigned char ack_type, unsigned char command);
//void send_cmd(int file_descriptor, unsigned char cmd, unsigned char args);
int get_packet(int file_descriptor, unsigned char* packet);
int get_packet_new(int fd, unsigned char* packet);
//void close_serial_port(int file_descriptor, struct termios* oldtio);
//void process_status(void const * const ptr);
void process_status(unsigned char* ptr);
const char* get_packet_type(unsigned char* packet , int length);
void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
void send_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
void send_messaged(int fd, unsigned char destination, char *message);
#endif // AQ_SERIAL_H_
