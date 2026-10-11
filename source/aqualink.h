
#ifndef AQUALINK_H_
#define AQUALINK_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include "aq_serial.h"
#include "aq_programmer.h"
#include "sensors.h"
//#include "aq_panel.h"  // Moved to later in file to overcome circular dependency. (crappy I know)

#define DPRINTF(format, ...) printf("%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__)
//#define DPRINTF(format, ...)

#define isMASK_SET(bitmask, mask) ((bitmask & mask) == mask)
#define setMASK(bitmask, mask)    (bitmask |= mask)
#define removeMASK(bitmask, mask) (bitmask &= ~mask)

#define SIGRESTART SIGUSR1
#define SIGRUPGRADE SIGUSR2

#define CLIGHT_PANEL_FIX // Overcome bug in some jandy panels where color light status of on is not in LED status

#define PLIGHT_IAQTOUCH_FIX // Overcome bug in some jandy panels where programming a light can cause other light status to be missed, by resetting poll counter on light programming.

#define TIME_CHECK_INTERVAL  3600
//#define TIME_CHECK_INTERVAL  100 // DEBUG ONLY
#define ACCEPTABLE_TIME_DIFF 120


#define MAX_ZERO_READ_BEFORE_RECONNECT 10

// The below will change state of devices before that are actually set on the control panel, this helps
// with duplicate messages that come in quick succession that can catch the state before it happens.
//#define PRESTATE_ONOFF
#define PRESTATE_SWG_SETPOINT
//#define PRESTATE_HEATER_SETPOINT // This one is not implimented yet

void intHandler(int dummy);

bool isAqualinkDStopping();

#ifdef AQ_PDA
bool checkAqualinkTime(); // Only need to externalise this for PDA
#endif

// There are cases where SWG will read 80% in allbutton and 0% in onetouch/aqualinktouch, this will compile that in or out
//#define READ_SWG_FROM_EXTENDED_ID

//#define TOTAL_BUTTONS     12
/*
#ifndef AQ_RS16
#define TOTAL_BUTTONS          12
#else
#define TOTAL_BUTTONS          20
#define RS16_VBUTTONS_START    13  // RS16 panel has 4 buttons with no LED's, so list them for manual matching to RS messages
#define RS16_VBUTTONS_END      16  // RS16 panel has 4 buttons with no LED's, so list them for manual matching to RS messages
#endif
*/
#define TEMP_UNKNOWN    -999
#define TEMP_REFRESH    -998

#define AQ_UNKNOWN TEMP_UNKNOWN
//#define UNKNOWN TEMP_UNKNOWN
#define DATE_STRING_LEN   30

#define MAX_PUMPS 4
#define MAX_LIGHTS 4
#define MAX_SENSORS 4

bool isVirtualButtonEnabled();

#define PUMP_RPM_MAX 3450
#define PUMP_RPM_MIN 600
#define PUMP_GPM_MAX 130
#define PUMP_GPM_MIN 15

/*
typedef enum temperatureUOM {
 FAHRENHEIT,
 CELSIUS,
 UNKNOWN
} temperatureUOM;
*/

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

typedef struct aqualinkkey
{
  aqled *led;
  char *label;
  char *name;
  unsigned char code;
  unsigned char rssd_code;
  uint8_t special_mask;
  void *special_mask_ptr;
  uint32_t runtime_sec;
} aqkey;


//#include "aq_programmer.h"

// special_mask for above aqualinkkey structure.
#define VS_PUMP        (1 << 0)
#define PROGRAM_LIGHT  (1 << 1)
#define TIMER_ACTIVE   (1 << 2) 
//#define DIMMER_LIGHT   (1 << 3) // NOT USED (Use PROGRAM_LIGHT or type LC_DIMMER) 
#define VIRTUAL_BUTTON (1 << 4)
// Below are types of VIRT_BUTTON, SO VIRT_BUTTON must also be set
#define VIRTUAL_BUTTON_ALT_LABEL (1 << 5)
#define VIRTUAL_BUTTON_CHILLER   (1 << 6)
//typedef struct ProgramThread ProgramThread;  // Definition is later

struct programmingthread {
  pthread_t *thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  program_type ptype;
  //void *thread_args;
};
/*
struct programmerArgs {
  aqkey *button;
  int value;
  //char cval[PTHREAD_ARG];
};

struct programmingThreadCtrl {
  pthread_t thread_id;
  //void *thread_args;
  struct programmerArgs pArgs;
  char thread_args[PTHREAD_ARG];
  struct aqualinkdata *aq_data;
};
*/
/*
typedef enum panel_status {
  CONNECTED,
  CHECKING_CONFIG,
  CONECTING,
  LOOKING_IDS,
  STARTING,
  SERIAL_ERROR, // Errors that stop reading serial port should be below this line
  NO_IDS_ERROR,
} panel_status;
*/

typedef enum action_type {
  NO_ACTION = -1,
  POOL_HTR_SETPOINT,
  SPA_HTR_SETPOINT,
  FREEZE_SETPOINT,
  CHILLER_SETPOINT,
  SWG_SETPOINT,
  SWG_BOOST,
  PUMP_RPM,
  PUMP_VSPROGRAM,
  POOL_HTR_INCREMENT,   // Setpoint add value (can be negative)
  SPA_HTR_INCREMENT,    // Setpoint add value
  ON_OFF,
  TIMER,
  TIMER_SEC,
  LIGHT_MODE,
  LIGHT_BRIGHTNESS,
  LIGHT_RGB,
  DATE_TIME
} action_type;

struct action {
  action_type type;
  time_t requested;
  int value;
  int id; // Only used for Pumps at the moment.
  aqkey *button;
  //char value[10];
};

// Moved to aq_programmer to stop circular dependency
/*
typedef enum pump_type {
  PT_UNKNOWN = -1,
  EPUMP,
  VSPUMP,
  VFPUMP
} pump_type;
*/

/*
typedef enum simulator_type {
  SIM_NONE,
  SIM_ALLB,
  SIM_ONET,
  SIM_PDA,
  SIM_IAQT
} simulator_type;
*/

// Set Point types
typedef enum SP_TYPE{
  SP_POOL,
  SP_SPA,
  SP_CHILLER
} SP_TYPE;

//#define PUMP_PRIMING -1
//#define PUMP_OFFLINE -2
//#define PUMP_ERROR   -3
#define PUMP_OFF_RPM 0
#define PUMP_OFF_GPM PUMP_OFF_RPM
#define PUMP_OFF_WAT PUMP_OFF_RPM


// FUTURE VSP STATUS, keep panel status and RS485 status separate

typedef enum panel_vsp_status
{
  PS_OK = 0,  // Start at 0 to match actual status from RS, but go down from there.
  PS_OFF = -1,
  PS_PRIMING = -2,
  PS_OFFLINE = -3,
  PS_ERROR = -4
} panel_vsp_status;

#define PUMP_NAME_LENGTH 30

// Overall Status of Aqualinkd

#define CONNECTED           ( 1 << 0 ) // All is good (every other mask should be cleared)
#define NOT_CONNECTED       ( 1 << 2 ) // Serial Error maybe rename
#define AUTOCONFIGURE_ID    ( 1 << 3 )
#define AUTOCONFIGURE_PANEL ( 1 << 4 )
#define CHECKING_CONFIG     ( 1 << 5 )
//#define LOOKING_IDS         ( 1 << 6 )
#define CONNECTING          ( 1 << 7 )
#define ERROR_NO_DEVICE_ID  ( 1 << 8 )                    // maybe covered in NOT_CONNECTED
#define ERROR_SERIAL        ( 1 << 9 )


/*
#define INSTALLDEVRELEASE  ( 1 << 0 )
#define UPDATERELEASE      ( 1 << 1 )
#define CHECKONLY          ( 1 << 3 )
*/

typedef struct {
    bool isConfigured;      
    int baselineRpmLow;
    float baselineKLow;
    int baselineRpmMed;
    float baselineKMed;    
    int baselineRpmHigh;
    float baselineKHigh;
} pump_efficiency_profile;

typedef struct pumpd
{
  int rpm;
  int gpm;
  int watts;
  int maxSpeed; // Max rpm or gpm depending on pump
  int minSpeed;
  unsigned char pumpID;
  int pumpIndex;
  char pumpName[PUMP_NAME_LENGTH];
  //char *pumpName;
  pump_type pumpType;
  //int buttonID;
  protocolType prclType;
  aqkey *button;
  //bool updated;
  // Other VSP values read directly from RS485
  int mode;  // 0 local control, 1 remote control
  //int driveState; // Haven't figured out what this is yet
  int status;
  panel_vsp_status pStatus;  // FUTURE VSP STATUS,
  int pressureCurve;
  pump_efficiency_profile efficiency;
} pump_detail;

// color light modes (Aqualink program, Jandy, Jandy LED, SAm/SAL, Color Logic, Intellibrite)
typedef enum clight_type {
  LC_PROGRAMABLE=0, 
  LC_JANDY, 
  LC_JANDYLED, 
  LC_SAL, 
  LC_CLOGIG, 
  LC_INTELLIB,
  LC_HAYWCL,
  LC_JANDYINFINATE,  // was SPARE_1 (Infinate watercolors LED)
  LC_SPARE_2,
  LC_SPARE_3,
  LC_DIMMER,  // use 0, 25, 50, 100
  LC_DIMMER2,  // use range 0 to 100
  NUMBER_LIGHT_COLOR_TYPES // This is used to size and count so add more prior to this
} clight_type;

/*
typedef enum {
  MD_CHILLER,
  MD_HEATPUMP
} heatmump_mode;
*/
typedef struct altlabeld
{
  char *altlabel;
  bool in_alt_mode;  // Example if altlabel="chiller", if last seen was chiller message this is true. 
  //heatmump_mode chiller_mode;
  // Add any other special params for virtual button
} altlabel_detail;

typedef enum {
  NET_MQTT=0, 
  NET_API, 
  NET_WS,
  NET_TIMER,       // Timer or Scheduler (eg poweron/freezeprotect check)
  UNACTION_TIMER
} request_source;



typedef struct clightd
{
  clight_type lightType;
  clight_type lightColorModes; // if lightType=0 then this can be used to define different colormodes for AqualinkD UI's
  aqkey *button;
  unsigned char lightID; // RS485 ID (only Jandy infinate watercolor)
  int currentValue;
  int lastValue;         // Used for AqualinkD self programming

  /* Shared memory block for light-type-specific parameters */
  union {
    struct {
      int brightness;        // Last known brightness (LC_JANDYINFINATE only, from 0x31 response)
      int red;               // Last set RGB red channel (LC_JANDYINFINATE, optimistic)
      int green;             // Last set RGB green channel
      int blue;              // Last set RGB blue channel
    };
    struct {
      int initialOn;         // Initial stabilization duration (seconds)
      int resetOff;          // Duration OFF required for mode reset (seconds)
      float pulseOnOff;      // Pulse toggle interval (seconds/milliseconds)
    };
  };

  //int brightness;        // Last known brightness (LC_JANDYINFINATE only, from 0x31 response)
  //int red;               // Last set RGB red channel (LC_JANDYINFINATE, optimistic)
  //int green;             // Last set RGB green channel
  //int blue;              // Last set RGB blue channel
  // Need to add below
  // initialON
  // restOFF
  // pulseONOFF 
  aqledstate RSSDstate;  // state from rs serial adapter
} clight_detail;


#include "aq_panel.h"

// Macro to print the positions (0 to 31) of all set bits in a uint32_t mask.
// The use of 'do-while(0)' allows the macro to be used safely in if/else statements.
#include <limits.h> // Required for CHAR_BIT
#define PRINT_SET_BITS(mask) do { \
    const uint32_t m = (mask); /* Use a const variable for safety and to avoid side effects */ \
    int printed_count = 0; \
    printf("Set bits at positions: "); \
    for (int i = 0; i < (sizeof(uint32_t) * CHAR_BIT); i++) { \
        if ((m >> i) & 1U) { /* Check the i-th bit by shifting right */ \
            printf("%d ", i); \
            printed_count++; \
        } \
    } \
    if (printed_count == 0) { \
        printf("None"); \
    } \
    printf("\n"); \
} while(0)


/**
 * SET_IF_CHANGED: Updates a variable and sets a flag if the value has changed.
 *
 * @src: The variable to be updated (can be a struct member).
 * @val: The new value.
 * @flag: A boolean flag to set to true if a change occurs.
 *
 * This macro uses GCC extensions for type safety and to prevent
 * double-evaluation of the `val` argument.
 */
//#define DEBUG_SET_IF_CHANGED
#ifndef DEBUG_SET_IF_CHANGED

#define SET_IF_CHANGED(src, val, flag) \
    ({                                                           \
        typeof(src) __new_val = (val);                           \
        if ((src) != __new_val) {                                \
            (src) = __new_val;                                   \
            (flag) = true;                                       \
        }                                                        \
    })

#define SET_IF_CHANGED_STRCPY(src, val, flag)                  \
    ({                                                         \
        const char *__new_val = (val);                         \
        if (strncmp((src), __new_val, sizeof(src)) != 0) {     \
            strncpy((src), __new_val, sizeof(src));            \
            (src)[sizeof(src) - 1] = '\0';                     \
            (flag) = true;                                     \
        }                                                      \
    })

#define SET_DIRTY(flag)    ((flag) = true)
#define CLEAR_DIRTY(flag)  ((flag) = false)

#else

#define SET_IF_CHANGED(src, val, flag) \
    ({ \
        typeof(src) __old_val = (src); \
        typeof(src) __new_val = (val); \
        if (__old_val != __new_val) { \
            (src) = __new_val; \
            (flag) = true; \
            printf("[%s:%d] Changed %s: %d -> %d\n", __FILE__, __LINE__, #src, (int)__old_val, (int)__new_val); \
        } \
    })

#define SET_IF_CHANGED_STRCPY(src, val, flag)                          \
    ({                                                                 \
        const char *__new_val = (val);                                 \
        if (strncmp((src), __new_val, sizeof(src)) != 0) {             \
            printf("[%s:%d] Changed %s: \"%s\" -> \"%s\"\n", __FILE__, __LINE__, #src, (src), __new_val);        \
            strncpy((src), __new_val, sizeof(src));                    \
            (src)[sizeof(src) - 1] = '\0';                             \
            (flag) = true;                                             \
        }                                                              \
    })

#define SET_DIRTY(flag)  \
    do {                  \
        if (!(flag)) {    \
            (flag) = true;\
            printf("[%s:%d] Set dirty flag\n", __FILE__, __LINE__); \
        }                 \
    } while(0)

#define CLEAR_DIRTY(flag)  \
    do {                  \
        if ((flag)) {    \
            (flag) = false;\
            printf("[%s:%d] Clear dirty flag\n", __FILE__, __LINE__); \
        }                 \
    } while(0)

#endif // DEBUG_SET_IF_CHANGED


struct aqualinkdata
{
  //panel_status panelstatus;
  uint16_t status_mask;
  //char version[AQ_MSGLEN*2]; // Will be replaced by below in future
  //char revision[AQ_MSGLEN]; // Will be replaced by below in future
  uint8_t updatetype;
  char *upgrade_version;
  // The below 4 are set (sometimes) but not used yet
  char panel_rev[AQ_MSGLEN];    // From panel
  char panel_cpu[AQ_MSGLEN];    // From panel
  char panel_string[AQ_MSGLEN]; // This is from actual PANEL not aqualinkd's config
  uint32_t panel_support_options;

  char date[AQ_MSGLEN];
  char time[AQ_MSGLEN];
  char last_message[AQ_MSGLONGLEN+1]; // Last ascii message from panel - allbutton (or PDA) protocol
  char last_display_message[AQ_MSGLONGLEN+1]; // Last message to display in web UI
  bool is_display_message_programming;
  aqled aqualinkleds[TOTAL_LEDS];
  aqkey aqbuttons[TOTAL_BUTTONS];
  unsigned short total_buttons;
  unsigned short virtual_button_start;
  int air_temp;
  int pool_temp;
  int spa_temp;
  int temp_units;
  //bool single_device; // Pool or Spa only, not Pool & Spa (Thermostat setpoints are different)
  int battery;
  int frz_protect_set_point;
  int pool_htr_set_point;
  int spa_htr_set_point;
  int swg_percent;
  int swg_ppm;
  int chiller_set_point;
  aqkey *chiller_button;
  //heatmump_mode chiller_mode;
  unsigned char ar_swg_device_status; // Actual state 
  unsigned char heater_err_status;
  aqledstate swg_led_state; // Display state for UI's
  aqledstate service_mode_state;
  aqledstate frz_protect_state;
  //aqledstate chiller_state;
  int num_pumps;
  pump_detail pumps[MAX_PUMPS];
  int num_lights;
  clight_detail lights[MAX_LIGHTS];
  bool boost;
  char boost_msg[10];
  int boost_duration; // need to remove boost message and use this
  int boost_linked_device;
  float ph;
  int orp;

  // Below this line is not state related.
  //aqkey *orderedbuttons[TOTAL_BUTTONS]; // Future to reduce RS4,6,8,12,16 & spa buttons
  //unsigned short total_ordered_buttons;
  unsigned char last_packet_type;
  int swg_delayed_percent;
  //bool simulate_panel; // NSF remove in future
  unsigned char simulator_packet[AQ_MAXPKTLEN+1];
  bool simulator_packet_updated;
  int simulator_packet_length;
  
  //bool simulator_active; // should be redundant with other two
  unsigned char simulator_id;
  //simulator_type simulator_active;
  emulation_type simulator_active;

  bool aqManagerActive;
  int open_websockets;
  struct programmingthread active_thread;
  struct action unactioned;
  unsigned char raw_status[AQ_PSTLEN];
  // Multiple threads update this value.
  //volatile bool updated;
  volatile bool is_dirty;
  char self[AQ_MSGLEN*2];

  int num_sensors;
  external_sensor sensors[MAX_SENSORS];

  #ifdef AQ_MANAGER
  volatile bool run_slogger;
   int slogger_packets;
   bool slogger_debug;
   char slogger_ids[20];
  #endif


  int rs16_vbutton_start;
  int rs16_vbutton_end;

  #ifdef AQ_PDA
  int pool_heater_index;
  int spa_heater_index;
  int solar_heater_index;
  #endif
  // Timing for millisecond timestamp logging
  struct timespec last_active_time;
  struct timespec start_active_time;

  // Overcome color light bug, by reconnecting allbutton panel.
  //bool reconnectAllButton;
};




#endif 
