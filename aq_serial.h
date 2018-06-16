
#ifndef AQ_SERIAL_H_
#define AQ_SERIAL_H_

#include <termios.h>

// packet offsets
#define PKT_DEST        2
#define PKT_CMD         3
#define PKT_DATA        4

#define PKT_STATUS_BYTES 5


#define DEV_MASTER      0

// PACKET DEFINES
#define NUL  0x00
#define DLE  0x10
#define STX  0x02
#define ETX  0x03

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

/* AquaRite commands */
#define CMD_GETID       0x14  // May be remote control control
#define CMD_PERCENT     0x11  // Set Percent
#define CMD_PPM         0x16  // Received PPM

/* PDA KEY CODES */  // Just plating at the moment
#define KEY_PDA_UP     0x06
#define KEY_PDA_DOWN   0x05
#define KEY_PDA_BACK   0x02
#define KEY_PDA_SELECT 0x04
#define KEY_PDA_PGUP   0x05
#define KEY_PDA_PGDN   0x05

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

#define MSG_AIR_TEMP   "AIR TEMP"
#define MSG_POOL_TEMP  "POOL TEMP"
#define MSG_SPA_TEMP   "SPA TEMP"
#define MSG_AIR_TEMP_LEN   8
#define MSG_POOL_TEMP_LEN  9
#define MSG_SPA_TEMP_LEN   8

#define MSG_SWG_PCT   "AQUAPURE"  // AquaPure 55%
#define MSG_SWG_PPM   "SALT"  // Salt 3000 PPM
#define MSG_SWG_PCT_LEN  8
#define MSG_SWG_PPM_LEN  4


/* AQUAPURE SWG */

#define SWG_STATUS_OFF    0xFF

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


int init_serial_port(char* tty);
void close_serial_port(int file_descriptor);
int generate_checksum(unsigned char* packet, int length);
void send_ack(int file_descriptor, unsigned char command);
//void send_cmd(int file_descriptor, unsigned char cmd, unsigned char args);
int get_packet(int file_descriptor, unsigned char* packet);
//void close_serial_port(int file_descriptor, struct termios* oldtio);
//void process_status(void const * const ptr);
void process_status(unsigned char* ptr);
const char* get_packet_type(unsigned char* packet, int length);
void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
void send_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3);
void send_messaged(int fd, unsigned char destination, char *message);
#endif // AQ_SERIAL_H_
