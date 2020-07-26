/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */


#include <stdio.h>
#include <stdarg.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include "aq_serial.h"
#include "utils.h"
#include "config.h"
#include "packetLogger.h"

//#define USE_AQ_SERIAL_OLD

//#define BLOCKING_MODE

static bool _blocking_mode = false;
int _blocking_fds = -1;

#ifndef USE_AQ_SERIAL_OLD  // Substansial changes to core component, make sure we can role back easily

static struct termios _oldtio;

void send_packet(int fd, unsigned char *packet, int length);
//unsigned char getProtocolType(unsigned char* packet);

/*
#ifdef ONETOUCH
bool _onetouch_mode = false;
void set_onetouch_mode(bool mode)
{
  if (mode)
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using Onetouch mode\n");

  _onetouch_mode = mode;
}
bool onetouch_mode()
{
  return _onetouch_mode;
}
#endif
*/

/*
#ifdef AQ_PDA
bool _pda_mode = false;

void set_pda_mode(bool mode)
{
  if (mode)
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using PDA mode\n");

  _pda_mode = mode;
}
bool pda_mode()
{
  return _pda_mode;
}
#endif
*/
/*
bool _onetouch_enabled = false;
bool _iaqtouch_enabled = false;
bool _extended_device_id_programming = false;


void set_onetouch_enabled(bool mode)
{
  if (mode) {
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using use ONETOUCH mode for VSP programming\n");
    _iaqtouch_enabled = false;
  }
  _onetouch_enabled = mode;
}

bool onetouch_enabled()
{
#ifdef AQ_ONETOUCH
  return _onetouch_enabled;
#else
  return false;
#endif
}

void set_iaqtouch_enabled(bool mode)
{
  if (mode) {
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using use IAQ TOUCH mode for VSP programming\n");
    _onetouch_enabled = false;
  }
  _iaqtouch_enabled = mode;
}

bool iaqtouch_enabled()
{
#ifdef AQ_IAQTOUCH
  return _iaqtouch_enabled;
#else
  return false;
#endif
}

bool VSP_enabled()
{
  // At present this is dependant on onetouch.
  //return onetouch_enabled();
  if (onetouch_enabled() || iaqtouch_enabled())
    return true;
  
  return false;
}

void set_extended_device_id_programming(bool mode)
{
  if (mode)
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using use %s mode for programming (where supported)\n",onetouch_enabled()?"ONETOUCH":"IAQ TOUCH");
  _extended_device_id_programming = mode;
}

bool extended_device_id_programming()
{
  return _extended_device_id_programming;
}
*/

const char* get_packet_type(unsigned char* packet , int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PKT_CMD]) {
    case CMD_ACK:
      return "Ack";
    break;
    case CMD_STATUS:
      return "Status";
    break;
    case CMD_MSG:
      return "Message";
    break;
    case CMD_MSG_LONG:
      return "Lng Message";
    break;
    case CMD_PROBE:
      return "Probe";
    break;
    case CMD_GETID:
      return "GetID";
    break;
    case CMD_PERCENT:
      return "AR %%";
    break;
    case CMD_PPM:
      return "AR PPM";
    break;
    case CMD_PDA_0x05:
      return "PDA Unknown";
    break;
    case CMD_PDA_0x1B:
      return "PDA Init (*guess*)";
    break;
    case CMD_PDA_HIGHLIGHT:
      return "PDA Hlight";
    break;
    case CMD_PDA_CLEAR:
      return "PDA Clear";
    break;
    case CMD_PDA_SHIFTLINES:
      return "PDA Shiftlines";
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      return "PDA HlightChars";
    break;
    case CMD_IAQ_PAGE_MSG:
      if (packet[4] == 0x31)
        return "iAq setDevice";
      else
        return "iAq pMessage";
    break;
    case CMD_IAQ_PAGE_BUTTON:
        return "iAq pButton";
    break;
    case CMD_IAQ_POLL:
      return "iAq Poll";
    break;
    case CMD_IAQ_PAGE_END:
      return "iAq PageEnd";
    break;
    case CMD_IAQ_PAGE_START:
      return "iAq PageStart";
    break;
    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PKT_CMD]);
      return buf;
    break;
  }
}

// Generate and return checksum of packet.
int generate_checksum(unsigned char* packet, int length)
{
  int i, sum, n;

  n = length - 3;
  sum = 0;
  for (i = 0; i < n; i++)
  sum += (int) packet[i];
  return(sum & 0x0ff);
}

bool check_jandy_checksum(unsigned char* packet, int length)
{
  if (generate_checksum(packet, length) == packet[length-3])
    return true;

  return false;
}

bool check_pentair_checksum(unsigned char* packet, int length)
{
  //printf("check_pentair_checksum \n");
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 8;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  //printf("Check High 0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n], packet[length-2],((sum >> 8) & 0xFF) );
  //printf("Check Low  0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n + 1], packet[length-1], (sum & 0xFF) ); 

  // Check against caculated length
  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;

  // Check against actual # length
  if (sum == (packet[n] * 256 + packet[n+1])) {
    LOG(RSSD_LOG,LOG_ERR, "Pentair checksum is accurate but length is not\n");
    return true;
  }
  
  return false;
}


void generate_pentair_checksum(unsigned char* packet, int length)
{
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 6;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  packet[n+1] = (unsigned char) (sum & 0xFF);        // Low Byte
  packet[n] = (unsigned char) ((sum >> 8) & 0xFF); // High Byte

}

protocolType getProtocolType(unsigned char* packet) {
  if (packet[0] == DLE)
    return JANDY;
  else if (packet[0] == PP1)
    return PENTAIR;

  return P_UNKNOWN; 
}
/*
unsigned char getProtocolType(unsigned char* packet) {
  if (packet[0] == DLE)
    return PCOL_JANDY;
  else if (packet[0] == PP1)
    return PCOL_PENTAIR;

  return PCOL_UNKNOWN; 
}
*/

#ifndef PLAYBACK_MODE
/*
Open and Initialize the serial communications port to the Aqualink RS8 device.
Arg is tty or port designation string
returns the file descriptor
*/
//#define TXDEN_DUMMY_RS485_MODE

#ifdef TXDEN_DUMMY_RS485_MODE

#include <linux/serial.h>
/* RS485 ioctls: */
#define TIOCGRS485      0x542E
#define TIOCSRS485      0x542F

int init_serial_port_Pi(const char* tty)
{
  struct serial_rs485 rs485conf = {0};

  //int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
  int fd = open(tty, O_RDWR);
  if (fd < 0)  {
    LOG(RSSD_LOG,LOG_ERR, "Unable to open port: %s\n", tty);
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Openeded serial port %s\n",tty);

  if (ioctl (fd, TIOCGRS485, &rs485conf) < 0) {
    LOG(RSSD_LOG,LOG_ERR, "Error reading ioctl port (%d): %s\n",  errno, strerror( errno ));
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG, "Port currently RS485 mode is %s\n", (rs485conf.flags & SER_RS485_ENABLED) ? "set" : "NOT set");

  /* Enable RS485 mode: */
	rs485conf.flags |= SER_RS485_ENABLED;

	/* Set logical level for RTS pin equal to 1 when sending: */
	rs485conf.flags |= SER_RS485_RTS_ON_SEND;
	/* or, set logical level for RTS pin equal to 0 when sending: */
	//rs485conf.flags &= ~(SER_RS485_RTS_ON_SEND);

	/* Set logical level for RTS pin equal to 1 after sending: */
	rs485conf.flags |= SER_RS485_RTS_AFTER_SEND;
	/* or, set logical level for RTS pin equal to 0 after sending: */
	//rs485conf.flags &= ~(SER_RS485_RTS_AFTER_SEND);

  /* Set this flag if you want to receive data even whilst sending data */
	//rs485conf.flags |= SER_RS485_RX_DURING_TX;

	if (ioctl (fd, TIOCSRS485, &rs485conf) < 0) {
		LOG(RSSD_LOG,LOG_ERR, "Unable to set port to RS485 %s (%d): %s\n", tty, errno, strerror( errno ));
    return -1;
	}

  return fd;
}
#endif

int _init_serial_port(const char* tty, bool blocking);

int init_serial_port(const char* tty)
{
  return _init_serial_port(tty, false);
}
int init_blocking_serial_port(const char* tty)
{
  _blocking_fds = _init_serial_port(tty, true);
  return _blocking_fds;
}

int _init_serial_port(const char* tty, bool blocking)
{

  long BAUD = B9600;
  long DATABITS = CS8;
  long STOPBITS = 0;
  long PARITYON = 0;
  long PARITY = 0;

  struct termios newtio;       //place for old and new port settings for serial port

  _blocking_mode = blocking;

  //int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
  int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
  if (fd < 0)  {
    LOG(RSSD_LOG,LOG_ERR, "Unable to open port: %s\n", tty);
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG, "Openeded serial port %s\n",tty);
  
  if (_blocking_mode) {
    // http://unixwiz.net/techtips/termios-vmin-vtime.html
    // Not designed behaviour, but it's what we need.
    fcntl(fd, F_SETFL, 0);
    newtio.c_cc[VMIN]= 1;
    newtio.c_cc[VTIME]= 0;
    LOG(RSSD_LOG,LOG_DEBUG, "Set serial port %s to blocking mode\n",tty);
  } else {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
    newtio.c_cc[VMIN]= 0;
    newtio.c_cc[VTIME]= 1;
    LOG(RSSD_LOG,LOG_DEBUG, "Set serial port %s to non blocking mode\n",tty);
  }

  tcgetattr(fd, &_oldtio); // save current port settings
    // set new port settings for canonical input processing
  newtio.c_cflag = BAUD | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_lflag = 0;       // ICANON;  
  newtio.c_oflag = 0;
    
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &newtio);
  
  LOG(RSSD_LOG,LOG_DEBUG, "Set serial port %s io attributes\n",tty);

  return fd;
}

void close_blocking_serial_port()
{
  if (_blocking_fds > 0) {
    LOG(RSSD_LOG,LOG_INFO, "Forcing close of blocking serial port, ignore following read errors\n");
    close_serial_port(_blocking_fds);
  } else {
    LOG(RSSD_LOG,LOG_ERR, "Didn't find valid blocking serial port file descripter\n");
  }
}
/* close tty port */
void close_serial_port(int fd)
{
  tcsetattr(fd, TCSANOW, &_oldtio);
  close(fd);
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Closed serial port\n");
}


// Send an ack packet to the Aqualink RS8 master device.
// fd: the file descriptor of the serial port connected to the device
// command: the command byte to send to the master device, NUL if no command
// 
// NUL = '\x00'
// DLE = '\x10'
// STX = '\x02'
// ETX = '\x03'
// 
// masterAddr = '\x00'          # address of Aqualink controller
// 
//msg = DLE+STX+dest+cmd+args
//msg = msg+self.checksum(msg)+DLE+ETX
//      DLE+STX+DEST+CMD+ARGS+CHECKSUM+DLE+ETX


void print_hex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
  printf("\n");
}

/*
void test_cmd()
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //send_cmd(fd, CMD_ACK, command);
  
  print_hex((char *)ackPacket, length);
  
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
  
  ackPacket[6] = 0x02;
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
}
*/
/*
void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  ackPacket[3] = destination;
  ackPacket[4] = b1;
  ackPacket[5] = b2;
  ackPacket[6] = b3;
  ackPacket[7] = generate_checksum(ackPacket, length-1);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      LOG(RSSD_LOG,LOG_ERR, "write to serial port failed\n");
  }
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //LOG(RSSD_LOG,LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
#endif  

  log_packet("Sent ", ackPacket, length);
}
*/


/*
 unsigned char tp[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
 send_command(0, tp, 19);
 Should produce
{0xFF, 0x00, 0xFF, 0xA5, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x02, 0x9E};
 <-------  headder ----> <-- type to from type-> <len> <------------------------------ data ----------------------------------------> <checksum>
*/

void send_pentair_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;

  packet[0] = NUL;
  packet[1] = PP1;
  packet[2] = PP2;
  packet[3] = PP3;
  packet[4] = PP4;

  //packet[i++] = 0x00; // from
  //packet[i++] = // to
  for (i=5; i-5 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-4],i);
    if (i==6) {
      // Replace source
      //packet[i] = 0x00;
      // Don't replace source
      packet[i] = packet_buffer[i-5];
    } else if (i==9) {
      // Replace length
      //packet[i] = 0xFF;
      packet[i] = (unsigned char)size-5;
    } else {
      packet[i] = packet_buffer[i-5];
    }

    //packet[i] = packet_buffer[i-4];    
  }

  packet[++i] = NUL;  // Checksum
  packet[++i] = NUL;  // Checksum
  generate_pentair_checksum(&packet[1], i);
  packet[++i] = NUL;


  //logPacket(packet, i);
  send_packet(fd,packet,i);
}

void send_jandy_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;
  
  packet[0] = NUL;
  packet[1] = DLE;
  packet[2] = STX;

  for (i=3; i-3 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-2],i);
    packet[i] = packet_buffer[i-3];
  }

  packet[++i] = DLE;
  packet[++i] = ETX;
  packet[++i] = NUL;

  packet[i-3] = generate_checksum(packet, i);

  send_packet(fd,packet,++i);
}

//unsigned char packet_buffer[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
//unsigned char packet_buffer[] = {PCOL_JANDY, 0x07, 0x0F, 0x00, 0x00};
void send_command(int fd, unsigned char *packet_buffer, int size)
{
  //unsigned char packet[AQ_MAXPKTLEN];
  //int i=0;
  
  if (packet_buffer[0] == PCOL_JANDY) {
    //LOG(RSSD_LOG,LOG_ERR, "Only Jandy protocol supported at present!\n");
    send_jandy_command(fd, &packet_buffer[1], size-1);
    return;
  }
  if (packet_buffer[0] == PCOL_PENTAIR) {
    //LOG(RSSD_LOG,LOG_ERR, "Only Jandy protocol supported at present!\n");
    send_pentair_command(fd, &packet_buffer[1], size-1);
    return;
  }
}


void send_packet(int fd, unsigned char *packet, int length)
{
  if (_blocking_mode) {
    write(fd, packet, length);
  } else {
    if (_aqconfig_.readahead_b4_write) {
      unsigned char byte;
      int bytesRead;
      // int j=0;
      do {
        // j++;
        bytesRead = read(fd, &byte, 1);
        // printf("*** Peek Read %d 0x%02hhx ***\n",r,byte);
        if (bytesRead == 1 && byte != 0x00) {
          LOG(RSSD_LOG, LOG_ERR, "ERROR on RS485, AqualinkD was too slow in replying to message! (please check for performance issues)\n");
          do { // Just play catchup
            bytesRead = read(fd, &byte, 1);
            // if (bytesRead==1) { LOG(RSSD_LOG,LOG_ERR, "Error Cleanout read 0x%02hhx\n",byte); }
          } while (bytesRead == 1);

          return;
        }
      } while (bytesRead == 1 && byte == 0x00);
    }

    int nwrite, i;
    for (i = 0; i < length; i += nwrite) {
      nwrite = write(fd, packet + i, length - i);
      if (nwrite < 0)
        LOG(RSSD_LOG, LOG_ERR, "write to serial port failed\n");
    }
  } // _blockine_mode

  /*
  #ifdef AQ_DEBUG
    // Need to take this out for release
    if ( getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
      debuglogPacket(&packet[1], length-2);
    }
  #endif
  */
  if ( getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL) {
    // Packet is padded with 0x00, so discard for logging
    LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",length-2);
    logPacket(&packet[1], length-2);
  }
}

void _send_ack(int fd, unsigned char ack_type, unsigned char command)
{
  const int length = 11;
  // Default null ack with checksum generated, don't mess with it, just over right                    
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL }; 

  // Update the packet and checksum if command argument is not NUL.
  if(command != NUL || ack_type != NUL) {
    //ackPacket[5] = 0x00 normal, 0x03 some pause, 0x01 some pause ending  (0x01 = Screen Busy (also return from logn message))
    ackPacket[5] = ack_type;
    ackPacket[6] = command;
    ackPacket[7] = generate_checksum(ackPacket, length-1);
    if (command == DLE) {  // We shuld probably also check the ack type as well, just for future proofing.
      // 0x10(DLE) that's not part of the headder or footer needs to be escaped AFTER with NUL, so shif everyting uo one
      ackPacket[8] = ackPacket[7]; // move the caculated checksum
      ackPacket[7] = NUL; // escape the DLE
      ackPacket[9] = DLE; // add new end sequence 
      ackPacket[10] = ETX; // add new end sequence 
    }
  }

  //printf("***Send ACK (%s) ***\n",(ack_type==ACK_NORMAL?"Normal":(ack_type==ACK_SCREEN_BUSY?"ScreenBusy":"ScreenBusyDisplay")) );

  send_packet(fd, ackPacket, length);
}

void send_ack(int fd, unsigned char command)
{
   _send_ack(fd, ACK_NORMAL, command);
}

// ack_typ should only be ACK_PDA, ACK_NORMAL, ACK_SCREEN_BUSY, ACK_SCREEN_BUSY_DISPLAY
void send_extended_ack(int fd, unsigned char ack_type, unsigned char command)
{
  _send_ack(fd, ack_type, command);
}

int _get_packet(int fd, unsigned char* packet, bool rawlog);

int get_packet(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, false);
}
int get_packet_lograw(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, true);
}
int _get_packet(int fd, unsigned char* packet, bool rawlog)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  bool endOfPacket = false;
  //bool packetStarted = FALSE;
  bool lastByteDLE = false;
  int retry = 0;
  bool jandyPacketStarted = false;
  bool pentairPacketStarted = false;
  //bool lastByteDLE = false;
  int PentairPreCnt = 0;
  int PentairDataCnt = -1;

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

  while (!endOfPacket) {
    bytesRead = read(fd, &byte, 1);
//printf("Read %d 0x%02hhx err=%d\n",bytesRead,byte,errno);
    //if (bytesRead < 0 && errno == EAGAIN && packetStarted == FALSE && lastByteDLE == FALSE) {
    if (bytesRead < 0 && errno == EAGAIN && 
        jandyPacketStarted == false && 
        pentairPacketStarted == false && 
        lastByteDLE == false) {
      // We just have nothing to read
      return 0;
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 20) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
        //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        logPacketError(packet, index);
        return 0;
      }
      retry++;
      delay(10);
 #ifdef TESTING
    } else if (bytesRead == 0 && jandyPacketStarted == false && pentairPacketStarted == false) {
      // Probably set port to /dev/null for testing.
      //printf("Read loop return\n");
      return 0;
  #endif
    } else if (bytesRead == 1) {
      if (rawlog)
        logPacketByte(&byte);

      if (lastByteDLE == true && byte == NUL)
      {
        // Check for DLE | NULL (that's escape DLE so delete the NULL)
        //printf("IGNORE THIS PACKET\n");
        lastByteDLE = false;
      }
      else if (lastByteDLE == true)
      {
        if (index == 0)
          index++;

        packet[index] = byte;
        index++;
        if (byte == STX && jandyPacketStarted == false)
        {
          jandyPacketStarted = true;
          pentairPacketStarted = false;
        }
        else if (byte == ETX && jandyPacketStarted == true)
        {
          endOfPacket = true;
        }
      }
      else if (jandyPacketStarted || pentairPacketStarted)
      {
        packet[index] = byte;
        index++;
        if (pentairPacketStarted == true && index == 9)
        {
          //printf("Read 0x%02hhx %d pentair\n", byte, byte);
          PentairDataCnt = byte;
        }
        if (PentairDataCnt >= 0 && index - 11 >= PentairDataCnt && pentairPacketStarted == true)
        {
          endOfPacket = true;
          PentairPreCnt = -1;
        }
      }
      else if (byte == DLE && jandyPacketStarted == false)
      {
        packet[index] = byte;
      }

      // // reset index incase we have EOP before start
      if (jandyPacketStarted == false && pentairPacketStarted == false)
      {
        index = 0;
      }

      if (byte == DLE && pentairPacketStarted == false)
      {
        lastByteDLE = true;
        PentairPreCnt = -1;
      }
      else
      {
        lastByteDLE = false;
        if (byte == PP1 && PentairPreCnt == 0)
          PentairPreCnt = 1;
        else if (byte == PP2 && PentairPreCnt == 1)
          PentairPreCnt = 2;
        else if (byte == PP3 && PentairPreCnt == 2)
          PentairPreCnt = 3;
        else if (byte == PP4 && PentairPreCnt == 3)
        {
          pentairPacketStarted = true;
          jandyPacketStarted = false;
          PentairDataCnt = -1;
          packet[0] = PP1;
          packet[1] = PP2;
          packet[2] = PP3;
          packet[3] = byte;
          index = 4;
        }
        else if (byte != PP1) // Don't reset counter if multiple PP1's
          PentairPreCnt = 0;
      }
    } else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      LOG(RSSD_LOG,LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet too large\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }
  
  // Clean out rest of buffer, make sure their is nothing else
/*  Doesn't work for shit due to probe message speed, need to come back and re-think
  if (_aqconfig_.readahead_b4_write) {
    if (endOfPacket == true) {
      do {
        bytesRead = read(fd, &byte, 1);
        //if (bytesRead==1) { LOG(RSSD_LOG,LOG_ERR, "Cleanout buffer read 0x%02hhx\n",byte); }
        if (bytesRead==1 && byte != 0x00) {
          LOG(RSSD_LOG,LOG_ERR, "SERIOUS ERROR on RS485, AqualinkD caught packet collision on bus, ignoring!\n");
          LOG(RSSD_LOG,LOG_ERR, "Error Cleanout read 0x%02hhx\n",byte); 
          // Run the buffer out
          do { 
            bytesRead = read(fd, &byte, 1);
            if (bytesRead==1) { LOG(RSSD_LOG,LOG_ERR, "Error Cleanout read 0x%02hhx\n",byte); }
          } while (bytesRead==1);
          return 0;
        }
      } while (bytesRead==1);
    }
  }
 */ 
  //LOG(RSSD_LOG,LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));
  if (jandyPacketStarted) {
    if (check_jandy_checksum(packet, index) != true){
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  }
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    logPacketError(packet, index);
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  logPacket(packet, index);
  // Return the packet length.
  return index;
}

#else // PLAYBACKMODE

#endif



























































#else  //USE_AQ_SERIAL_OLD




//#define BLOCKING_MODE

static struct termios _oldtio;

//static struct aqconfig *_config_parameters;
//static struct aqconfig *_aqualink_config;
bool _pda_mode = false;

void set_pda_mode(bool mode)
{
  if (mode)
    LOG(RSSD_LOG,LOG_NOTICE, "AqualinkD is using PDA mode\n");

  _pda_mode = mode;
}

bool pda_mode()
{
  return _pda_mode;
}

void log_packet(int level, char *init_str, unsigned char* packet, int length)
{
  
  if ( getLogLevel() < level) {
    //LOG(RSSD_LOG,LOG_INFO, "Send '0x%02hhx'|'0x%02hhx' to controller\n", packet[5] ,packet[6]);
    return;
  }
  
  int cnt;
  int i;
  char buff[MAXLEN * 2];

  cnt = sprintf(buff, "%s", init_str);
  cnt += sprintf(buff+cnt, " | %8.8s",getProtocolType(packet)==JANDY?"Jandy":"Pentair");
  cnt += sprintf(buff+cnt, " | HEX: ");
  //printHex(packet_buffer, packet_length);
  for (i=0;i<length;i++)
    cnt += sprintf(buff+cnt, "0x%02hhx|",packet[i]);

  cnt += sprintf(buff+cnt, "\n");
  LOG(RSSD_LOG,level, buff);
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, buff);
}

const char* get_packet_type(unsigned char* packet , int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PKT_CMD]) {
    case CMD_ACK:
      return "Ack";
    break;
    case CMD_STATUS:
      return "Status";
    break;
    case CMD_MSG:
      return "Message";
    break;
    case CMD_MSG_LONG:
      return "Lng Message";
    break;
    case CMD_PROBE:
      return "Probe";
    break;
    case CMD_GETID:
      return "GetID";
    break;
    case CMD_PERCENT:
      return "AR %%";
    break;
    case CMD_PPM:
      return "AR PPM";
    break;
    case CMD_PDA_0x05:
      return "PDA Unknown";
    break;
    case CMD_PDA_0x1B:
      return "PDA Init (*guess*)";
    break;
    case CMD_PDA_HIGHLIGHT:
      return "PDA Hlight";
    break;
    case CMD_PDA_CLEAR:
      return "PDA Clear";
    break;
    case CMD_PDA_SHIFTLINES:
      return "PDA Shiftlines";
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      return "PDA C_HlightChar";
    break;
    case CMD_IAQ_MSG:
      return "iAq Message";
    break;
    case CMD_IAQ_MENU_MSG:
      return "iAq Menu";
    break;
    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PKT_CMD]);
      return buf;
    break;
  }
}




// Generate and return checksum of packet.
int generate_checksum(unsigned char* packet, int length)
{
  int i, sum, n;

  n = length - 3;
  sum = 0;
  for (i = 0; i < n; i++)
  sum += (int) packet[i];
  return(sum & 0x0ff);
}





// Send an ack packet to the Aqualink RS8 master device.
// fd: the file descriptor of the serial port connected to the device
// command: the command byte to send to the master device, NUL if no command
// 
// NUL = '\x00'
// DLE = '\x10'
// STX = '\x02'
// ETX = '\x03'
// 
// masterAddr = '\x00'          # address of Aqualink controller
// 
//msg = DLE+STX+dest+cmd+args
//msg = msg+self.checksum(msg)+DLE+ETX
//      DLE+STX+DEST+CMD+ARGS+CHECKSUM+DLE+ETX


void print_hex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
  printf("\n");
}

void test_cmd()
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //send_cmd(fd, CMD_ACK, command);
  
  print_hex((char *)ackPacket, length);
  
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
  
  ackPacket[6] = 0x02;
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
}

protocolType getProtocolType(unsigned char* packet) {
  if (packet[0] == DLE)
    return JANDY;
  else if (packet[0] == PP1)
    return PENTAIR;

  return P_UNKNOWN; 
}

#ifndef PLAYBACK_MODE

/*
Open and Initialize the serial communications port to the Aqualink RS8 device.
Arg is tty or port designation string
returns the file descriptor
*/
int init_serial_port(char* tty)
{
  long BAUD = B9600;
  long DATABITS = CS8;
  long STOPBITS = 0;
  long PARITYON = 0;
  long PARITY = 0;

  struct termios newtio;       //place for old and new port settings for serial port

  //int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
  int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
  if (fd < 0)  {
    LOG(RSSD_LOG,LOG_ERR, "Unable to open port: %s\n", tty);
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Openeded serial port %s\n",tty);
  
#ifdef BLOCKING_MODE
  fcntl(fd, F_SETFL, 0);
  newtio.c_cc[VMIN]= 1;
  newtio.c_cc[VTIME]= 0;
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Set serial port %s to blocking mode\n",tty);
#else
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
  newtio.c_cc[VMIN]= 0;
  newtio.c_cc[VTIME]= 1;
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Set serial port %s to non blocking mode\n",tty);
#endif
/*
// Need to change this to flock, but that depends on good exit.
  if ( ioctl(fd, TIOCEXCL) != 0) {
    displayLastSystemError("Locking serial port.");
  }

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  if (fcntl(fd, F_SETLK, &fl) == -1)
  {
    if (errno == EACCES || errno == EAGAIN)
    {
      displayLastSystemError("Locking serial port.");
    }
    else
    {
      displayLastSystemError("Unknown error Locking serial port.");
    }
  }
*/
  tcgetattr(fd, &_oldtio); // save current port settings
    // set new port settings for canonical input processing
  newtio.c_cflag = BAUD | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_lflag = 0;       // ICANON;  
  newtio.c_oflag = 0;
    
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &newtio);
  
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Set serial port %s io attributes\n",tty);

  return fd;
}

/* close tty port */
void close_serial_port(int fd)
{
  tcsetattr(fd, TCSANOW, &_oldtio);
  close(fd);
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Closed serial port\n");
}

void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  ackPacket[3] = destination;
  ackPacket[4] = b1;
  ackPacket[5] = b2;
  ackPacket[6] = b3;
  ackPacket[7] = generate_checksum(ackPacket, length-1);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      LOG(RSSD_LOG,LOG_ERR, "write to serial port failed\n");
  }
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //LOG(RSSD_LOG,LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
#endif  

  log_packet(LOG_DEBUG_SERIAL, "Sent ", ackPacket, length);

}
void send_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  ackPacket[3] = destination;
  ackPacket[4] = b1;
  ackPacket[5] = b2;
  ackPacket[6] = b3;
  ackPacket[7] = generate_checksum(ackPacket, length-1);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      LOG(RSSD_LOG,LOG_ERR, "write to serial port failed\n");
  }
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //LOG(RSSD_LOG,LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
#endif  

  if ( getLogLevel() >= LOG_DEBUG_SERIAL) {
    char buf[30];
    sprintf(buf, "Sent     %8.8s ", get_packet_type(ackPacket+1 , length));
    log_packet(LOG_DEBUG_SERIAL, buf, ackPacket, length);
  }
}

void send_messaged(int fd, unsigned char destination, char *message)
{
  const unsigned int length = 24;
  unsigned int i;
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  unsigned char msgPacket[] = { DLE,STX,DEV_MASTER,CMD_MSG,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,NUL,DLE,ETX };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  msgPacket[2] = destination;
  for (i=0; i < strlen(message) && i < AQ_MSGLEN; i++)
    msgPacket[4+i] = message[i];

  msgPacket[length-3] = generate_checksum(msgPacket, length-1);

#ifdef BLOCKING_MODE
  write(fd, msgPacket, length);
#else
  int nwrite;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, msgPacket + i, length - i);
    if (nwrite < 0) 
      LOG(RSSD_LOG,LOG_ERR, "write to serial port failed\n");
  }
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //LOG(RSSD_LOG,LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
#endif  

  log_packet(LOG_DEBUG_SERIAL, "Sent ", msgPacket, length);
}

void _send_ack(int fd, unsigned char ack_type, unsigned char command);

void send_extended_ack(int fd, unsigned char ack_type, unsigned char command)
{
  // ack_typ should only be ACK_NORMAL, ACK_SCREEN_BUSY, ACK_SCREEN_BUSY_DISPLAY
  _send_ack(fd, ack_type, command);
}

void send_ack(int fd, unsigned char command)
{
  if (! _pda_mode)
    _send_ack(fd, ACK_NORMAL, command);
  else
    _send_ack(fd, ACK_PDA, command);
}

void _send_ack(int fd, unsigned char ack_type, unsigned char command)
{
  const int length = 11;
  // Default null ack with checksum generated, don't mess with it, just over right                    
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL }; 
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  if(command != NUL || ack_type != NUL) {
    //ackPacket[5] = 0x00 normal, 0x03 some pause, 0x01 some pause ending  (0x01 = Screen Busy (also return from logn message))
    ackPacket[5] = ack_type;
    ackPacket[6] = command;
    ackPacket[7] = generate_checksum(ackPacket, length-1);

    // NULL out the command byte if it is the same. Difference implies that
    // a new command has come in, and is awaiting processing.
    /*
    if(aqualink_cmd == command) {
      aqualink_cmd = NUL;
    }
    */
    
    // In debug mode, log the packet to the private log file.
    //log_packet(ackPacket, length);
  }

  // Send the packet to the master device.
  //write(fd, ackPacket, length);
  //LOG(RSSD_LOG,LOG_DEBUG, "Send '0x%02hhx' to controller\n", command);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      LOG(RSSD_LOG,LOG_ERR, "write to serial port failed\n");
  }
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
#endif  
  
  //LOG(RSSD_LOG,LOG_DEBUG, "Sent '0x%02hhx' to controller\n", command);
  log_packet(LOG_DEBUG_SERIAL, "Sent ", ackPacket, length);

  //LOG(RSSD_LOG,LOG_DEBUG, "Sent '0x%02hhx' to controller \n", command);
}


int get_packet(int fd, unsigned char* packet)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  bool endOfPacket = FALSE;
  bool packetStarted = FALSE;
  bool lastByteDLE = FALSE;
  int retry = 0;

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

  while (!endOfPacket) {
    bytesRead = read(fd, &byte, 1);
    if (bytesRead < 0 && errno == EAGAIN && packetStarted == FALSE && lastByteDLE == FALSE) {
      // We just have nothing to read
      return 0;
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 20) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
        log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        return 0;
      }
      retry++;
      delay(10);
    } else if (bytesRead == 1) {
//printf("Byte 0x%02hhx\n",byte);
      if (lastByteDLE==TRUE && byte == NUL) {
        // Check for DLE | NULL (that's escape DLE so delete the NULL)
        //printf("IGNORE THIS PACKET\n");
        lastByteDLE = FALSE;
      } else if (lastByteDLE==TRUE) {
        if (index == 0)
          index++;

        packet[index] = byte;
        index++;
        if (byte == STX && packetStarted == FALSE) {
          packetStarted = TRUE;
        } else if (byte == ETX && packetStarted == TRUE) {
          endOfPacket = TRUE;
        }
      } else if (packetStarted) {
        packet[index] = byte;
        index++;
      } else if (byte == DLE && packetStarted == FALSE) {
        packet[index] = byte;
      }

      // // reset index incase we have EOP before start
      if (packetStarted == FALSE)
        index=0;
      
      if (byte == DLE) {
        lastByteDLE = TRUE;
      } else {
        lastByteDLE = FALSE;
      }
    } else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      LOG(RSSD_LOG,LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet too large\n");
      log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }

  //LOG(RSSD_LOG,LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));

  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN && packetStarted) { //NSF. Sometimes we get END sequence only, so just ignore.
  //} else if (index < AQ_MINPKTLEN) { //NSF. Sometimes we get END sequence only, so just ignore.
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  // Return the packet length.
  return index;
}





bool check_jandy_checksum(unsigned char* packet, int length)
{
  if (generate_checksum(packet, length) == packet[length-3])
    return true;

  return false;
}

bool check_pentair_checksum(unsigned char* packet, int length)
{
  //printf("check_pentair_checksum \n");
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 8;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  //printf("Check High 0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n], packet[length-2],((sum >> 8) & 0xFF) );
  //printf("Check Low  0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n + 1], packet[length-1], (sum & 0xFF) ); 

  // Check against caculated length
  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;

  // Check against actual # length
  if (sum == (packet[n] * 256 + packet[n+1])) {
    //LOG(RSSD_LOG,LOG_ERR, "Pentair checksum is accurate but length is not\n");
    return true;
  }
  
  return false;
}
/*
bool check_pentair_checksum_old(unsigned char* packet, int length)
{
  int i, sum, n;
  n = packet[8] + 9;
  sum = 0;
  for (i = 3; i < n; i++) {
    sum += (int) packet[i];
  }

  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;

  return false;
}
*/
int _get_packet(int fd, unsigned char* packet, bool rawlog);

int get_packet_new_lograw(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, true);
}
int get_packet_new(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, false);
}
int _get_packet(int fd, unsigned char* packet, bool rawlog)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  bool endOfPacket = false;
  //bool packetStarted = FALSE;
  bool lastByteDLE = false;
  int retry = 0;
  bool jandyPacketStarted = false;
  bool pentairPacketStarted = false;
  //bool lastByteDLE = false;
  int PentairPreCnt = 0;
  int PentairDataCnt = -1;

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

  while (!endOfPacket) {
    bytesRead = read(fd, &byte, 1);
    //if (bytesRead < 0 && errno == EAGAIN && packetStarted == FALSE && lastByteDLE == FALSE) {
    if (bytesRead < 0 && errno == EAGAIN && 
        jandyPacketStarted == false && 
        pentairPacketStarted == false && 
        lastByteDLE == false) {
      // We just have nothing to read
      return 0;
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 20) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
        log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        return 0;
      }
      retry++;
      delay(10);
    } else if (bytesRead == 1) {

      if (rawlog)
        logPacketByte(&byte);

      if (lastByteDLE == true && byte == NUL)
      {
        // Check for DLE | NULL (that's escape DLE so delete the NULL)
        //printf("IGNORE THIS PACKET\n");
        lastByteDLE = false;
      }
      else if (lastByteDLE == true)
      {
        if (index == 0)
          index++;

        packet[index] = byte;
        index++;
        if (byte == STX && jandyPacketStarted == false)
        {
          jandyPacketStarted = true;
          pentairPacketStarted = false;
        }
        else if (byte == ETX && jandyPacketStarted == true)
        {
          endOfPacket = true;
        }
      }
      else if (jandyPacketStarted || pentairPacketStarted)
      {
        packet[index] = byte;
        index++;
        if (pentairPacketStarted == true && index == 9)
        {
          //printf("Read 0x%02hhx %d pentair\n", byte, byte);
          PentairDataCnt = byte;
        }
        if (PentairDataCnt >= 0 && index - 11 >= PentairDataCnt && pentairPacketStarted == true)
        {
          endOfPacket = true;
          PentairPreCnt = -1;
        }
      }
      else if (byte == DLE && jandyPacketStarted == false)
      {
        packet[index] = byte;
      }

      // // reset index incase we have EOP before start
      if (jandyPacketStarted == false && pentairPacketStarted == false)
      {
        index = 0;
      }

      if (byte == DLE && pentairPacketStarted == false)
      {
        lastByteDLE = true;
        PentairPreCnt = -1;
      }
      else
      {
        lastByteDLE = false;
        if (byte == PP1 && PentairPreCnt == 0)
          PentairPreCnt = 1;
        else if (byte == PP2 && PentairPreCnt == 1)
          PentairPreCnt = 2;
        else if (byte == PP3 && PentairPreCnt == 2)
          PentairPreCnt = 3;
        else if (byte == PP4 && PentairPreCnt == 3)
        {
          pentairPacketStarted = true;
          jandyPacketStarted = false;
          PentairDataCnt = -1;
          packet[0] = PP1;
          packet[1] = PP2;
          packet[2] = PP3;
          packet[3] = byte;
          index = 4;
        }
        else if (byte != PP1) // Don't reset counter if multiple PP1's
          PentairPreCnt = 0;
      }
    } else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      LOG(RSSD_LOG,LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet too large\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }

  //LOG(RSSD_LOG,LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));
  if (jandyPacketStarted) {
    if (check_jandy_checksum(packet, index) != true){
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      logPacketError(packet, index);
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  }
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    logPacketError(packet, index);
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  logPacket(packet, index);
  // Return the packet length.
  return index;
}


#else // PLAYBACK_MODE
#include <stdlib.h>
FILE *_fp;


bool check_pentair_checksum(unsigned char* packet, int length)
{
  int i, sum, n;
  n = packet[8] + 9;
  sum = 0;
  //printf("Pentair ");
  for (i = 3; i < n; i++) {
    sum += (int) packet[i];
    //printf("+ (0x%02hhx) %d = %d ",packet[i],(int)packet[i],sum);
  }

  //printf("\nPentair checksum %d = %d (0x%02hhx) (0x%02hhx)\n",sum, (packet[length-1] * 256 + packet[length]),packet[length-1],packet[length]);

  if (sum == (packet[length-1] * 256 + packet[length]))
    return true;

  return false;
}


int init_serial_port(char* file)
{ 
  _fp = fopen ( file, "r" );
  if ( _fp == NULL )
  {
    perror ( file ); /* why didn't the file open? */
    return -1;
  }
  return 0;
}

void close_serial_port(int fd)
{
  fclose(_fp);
}

int get_packet_new(int fd, unsigned char* packet_buffer)
{
  return get_packet(fd, packet_buffer);
}
int get_packet(int fd, unsigned char* packet_buffer)
{
  int packet_length = 0;
  char line[256];
  char hex[6];
  int i;

  if ( fgets ( line, sizeof line, _fp ) != NULL ) /* read a line */
  {
    packet_length=0;
    for (i=0; i < strlen(line); i=i+5)
    {
      strncpy(hex, &line[i], 4);
      hex[5] = '\0';
      packet_buffer[packet_length] = (int)strtol(hex, NULL, 16);
      //printf("%s = 0x%02hhx = %c\n", hex, packet_buffer[packet_length], packet_buffer[packet_length]);
      packet_length++;
    }
    packet_length--;

    LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "PLAYBACK read %d bytes\n",packet_length);
    //printf("To 0x%02hhx, type %15.15s, length %2.2d ", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length),packet_length);
    //fputs ( line, stdout ); 

    if (getProtocolType(packet_buffer)==JANDY) {
      if (generate_checksum(packet_buffer, packet_length) != packet_buffer[packet_length-3]) {
        logPacketError(packet_buffer, packet_length);
        LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      } else
        logPacket(packet_buffer, packet_length);
    } else {
      //check_pentair_checksum(packet_buffer, packet_length);
      //check_pentair_checksum(packet_buffer, packet_length-1);
      //check_pentair_checksum(packet_buffer, packet_length+1);
      if (check_pentair_checksum(packet_buffer, packet_length-1) != true) {
        logPacketError(packet_buffer, packet_length);
        LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      } else
        logPacket(packet_buffer, packet_length);
    }

    return packet_length;
  }
  return 0;
}

void send_extended_ack(int fd, unsigned char ack_type, unsigned char command)
{}

void send_ack(int fd, unsigned char command)
{}

void send_messaged(int fd, unsigned char destination, char *message)
{}

void send_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{}
#endif // PLAYBACK_MODE
 


// Reads the bytes of the next incoming packet, and
// returns when a good packet is available in packet
// fd: the file descriptor to read the bytes from
// packet: the unsigned char buffer to store the bytes in
// returns the length of the packet
int get_packet_old(int fd, unsigned char* packet)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  int endOfPacket = FALSE;
  int packetStarted = FALSE;
  int foundDLE = FALSE;
  bool started = FALSE;
  int retry=0;

  while (!endOfPacket) {
    //printf("Read loop %d\n",++i);
    bytesRead = read(fd, &byte, 1);

    if (bytesRead < 0 && errno == EAGAIN && started == FALSE) {
      // We just have nothing to read
      return 0;
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 20) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
        log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        return 0;
      }
      retry++;
      delay(10);
    } else if (bytesRead == 1) {
      started = TRUE;
      //if (bytesRead == 1) {
      if (byte == DLE) {
        // Found a DLE byte. Set the flag, and record the byte.
        foundDLE = TRUE;
        packet[index] = byte;
      }
      else if (byte == STX && foundDLE == TRUE) {
        // Found the DLE STX byte sequence. Start of packet detected.
        // Reset the DLE flag, and record the byte.
        foundDLE = FALSE;
        packetStarted = TRUE;
        packet[index] = byte;
      }
      else if (byte == NUL && foundDLE == TRUE) {
        // Found the DLE NUL byte sequence. Detected a delimited data byte.
        // Reset the DLE flag, and decrement the packet index to offset the
        // index increment at the end of the loop. The delimiter, [NUL], byte
        // is not recorded.
        foundDLE = FALSE;
        //trimmed = true;
        index--;
      }
      else if (byte == ETX && foundDLE == TRUE) {
        // Found the DLE ETX byte sequence. End of packet detected.
        // Reset the DLE flag, set the end of packet flag, and record
        // the byte.
        foundDLE = FALSE;
        packetStarted = FALSE;
        endOfPacket = TRUE;
        packet[index] = byte;
      }
      else if (packetStarted == TRUE) {
        // Found a data byte. Reset the DLE flag just in case it is set
        // to prevent anomalous detections, and record the byte.
        foundDLE = FALSE;
        packet[index] = byte;
      }
      else {
        // Found an extraneous byte. Probably a NUL between packets.
        // Ignore it, and decrement the packet index to offset the
        // index increment at the end of the loop.
        index--;
      }

      // Finished processing the byte. Increment the packet index for the
      // next byte.
      index++;

      // Break out of the loop if we exceed maximum packet
      // length.
      if (index >= AQ_MAXPKTLEN) {
        break;
      }
    }
    else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      LOG(RSSD_LOG,LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }
  }

  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN) {
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  //if (_config_parameters.debug_RSProtocol_packets || getLogLevel() >= LOG_DEBUG_SERIAL) 
  //      logPacket(packet_buffer, packet_length);
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);

  // Return the packet length.
  return index;
}

#endif // USE_OLD

