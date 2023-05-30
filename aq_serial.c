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


//#define BLOCKING_MODE

static bool _blocking_mode = false;
int _blocking_fds = -1;

static struct termios _oldtio;

void send_packet(int fd, unsigned char *packet, int length);
//unsigned char getProtocolType(unsigned char* packet);



const char* get_packet_type(unsigned char* packet , int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PKT_CMD]) {
    case CMD_ACK:
      if (packet[5] == NUL)
        return "Ack";
      else
        return "Ack w/ Command";
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
    case CMD_IAQ_TABLE_MSG:
      return "iAq TableMsg";
    break;
    case CMD_IAQ_PAGE_CONTINUE:
      return "iAq PageContinue";
    break;
    case CMD_IAQ_STARTUP:
      return "iAq init";
    break;
    case RSSA_DEV_STATUS:
      // This is a fail reply 0x10|0x02|0x48|0x13|0x02|0x00|0x10|0x00|0x7f|0x10|0x03|
       // Rather than check all, just check 0x02 and checksum sin't I'm not sure 0x10 means faiure without 0x00 around it.
      if (packet[4] == 0x02 && packet[8] == 0x7f)
        return "RSSA Cmd Error";
      else
        return "RSSA DevStatus";
    break;
    case RSSA_DEV_READY:
      return "RSSA SendCommand";
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
  //printf("Checking 0x%02hhx against 0x%02hhx\n",generate_checksum(packet, length), packet[length-3]);
  if (generate_checksum(packet, length) == packet[length-3])
    return true;

  // There seems to be a bug with jandy one touch protocol where on a long msg to one touch you get
  // a bad checksum but everything is actually accurate, so forcing a good return on this.
  // Example message (always one touch / status message / line 3 and always 0x0a checksum)
  // 0x10|0x02|0x43|0x04|0x03|0x20|0x20|0x20|0x20|0x35|0x3a|0x30|0x35|0x20|0x50|0x4d|0x20|0x20|0x20|0x20|0x20|0x0a|0x10|0x03|
  if (packet[3] == 0x04 && packet[4] == 0x03 && packet[length-3] == 0x0a) {
    LOG(RSSD_LOG,LOG_INFO, "Ignoring bad checksum, seems to be bug in Jandy protocol\n");
    if (getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
      static char buf[1000];
      beautifyPacket(buf,packet,length,true);
      LOG(RSSD_LOG,LOG_DEBUG, "Packetin question %s\n",buf);
    }
    return true;
  }

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
#endif // TXDEN_DUMMY_RS485_MODE

int _init_serial_port(const char* tty, bool blocking, bool readahead);

int init_serial_port(const char* tty)
{
  return _init_serial_port(tty, false, false);
}
int init_readahead_serial_port(const char* tty)
{
  return _init_serial_port(tty, false, true);
}
int init_blocking_serial_port(const char* tty)
{
  _blocking_fds = _init_serial_port(tty, true, false);
  return _blocking_fds;
}

// https://www.cmrr.umn.edu/~strupp/serial.html#2_5_2
// http://unixwiz.net/techtips/termios-vmin-vtime.html
//#define OLD_SERIAL_INIT
#ifndef OLD_SERIAL_INIT

int _init_serial_port(const char* tty, bool blocking, bool readahead)
{
  //B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400
  const int BAUD = B9600;
  const int PARITY = 0;
  struct termios newtio;   

  _blocking_mode = blocking;

  int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
  //int fd = open(tty, O_RDWR | O_NOCTTY | O_SYNC); // This is way to slow at reading
  if (fd < 0)  {
    LOG(RSSD_LOG,LOG_ERR, "Unable to open port: %s, error %d\n", tty, errno);
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG, "Openeded serial port %s\n",tty);

  if (tcgetattr(fd, &newtio) != 0) {
    LOG(RSSD_LOG,LOG_ERR, "Unable to get port attributes: %s, error %d\n", tty,errno);
    return -1;
  }

  memcpy(&_oldtio, &newtio, sizeof(struct termios));

  cfsetospeed(&newtio, BAUD);
  cfsetispeed(&newtio, BAUD);

  newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8; // 8-bit chars
  // disable IGNBRK for mismatched speed tests; otherwise receive break
  // as \000 chars
  //newtio.c_iflag &= ~IGNBRK; // disable break processing
  newtio.c_iflag = 0;        // raw input
  newtio.c_lflag = 0;        // no signaling chars, no echo,
                             // no canonical processing
  newtio.c_oflag = 0;        // no remapping, no delays, raw output

  if (_blocking_mode) {
    fcntl(fd, F_SETFL, 0);     //efficient blocking for the read
    //newtio.c_cc[VMIN] = 1;     // read blocks for 1 character or timeout below
    //newtio.c_cc[VTIME] = 5;    // 0.5 seconds read timeout
    newtio.c_cc[VTIME] = 255;  // 25 seconds read timeout
    newtio.c_cc[VMIN] = 0;    
  } else {
    newtio.c_cc[VMIN]= 0;   // read doesn't block
    //newtio.c_cc[VTIME]= 1;
    newtio.c_cc[VTIME]= (readahead?0:1);
  }
  /*
  Raw output is selected by resetting the OPOST option in the c_oflag member:
  newtio.c_oflag &= ~OPOST;
  When the OPOST option is disabled, all other option bits in c_oflag are ignored.
  */
  //newtio.c_oflag &= ~OPOST; // Raw output

  newtio.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  newtio.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                        // enable reading
  newtio.c_cflag &= ~(PARENB | PARODD); // shut off parity
  newtio.c_cflag |= PARITY;
  newtio.c_cflag &= ~CSTOPB;
  newtio.c_cflag &= ~CRTSCTS;

  tcflush(fd, TCIFLUSH);
  if (tcsetattr(fd, TCSANOW, &newtio) != 0) {
    LOG(RSSD_LOG,LOG_ERR, "Unable to set port attributes: %s, error %d\n", tty,errno);
    return -1;
  }

  LOG(RSSD_LOG,LOG_INFO, "Set serial port %s io %s attributes\n",tty,_blocking_mode?"blocking":"non blocking");

  return fd;
}
#else //OLD_SERIAL_INIT
int _init_serial_port(const char* tty, bool blocking, bool readahead) // readahead ignored in this implimentation
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
    //newtio.c_cc[VTIME]= 0; // This will wait indefinatly. (not the best if panel / rs485 adapter is down)
    newtio.c_cc[VTIME]= 50; // (1 to 255)  1 = 0.1 sec, 255 = 25.5 sec
    LOG(RSSD_LOG,LOG_INFO, "Set serial port %s to blocking mode\n",tty);
  } else {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
    newtio.c_cc[VMIN]= 0;
    newtio.c_cc[VTIME]= 1; // This should be 0 if we have readahead before write
    LOG(RSSD_LOG,LOG_INFO, "Set serial port %s to non blocking mode\n",tty);
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
#endif


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

/*
void print_hex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
  printf("\n");
}
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

/*
 unsigned char tp[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
 send_command(0, tp, 19);
 Should produce
{0xFF, 0x00, 0xFF, 0xA5, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x02, 0x9E};
 <-------  headder ----> <-- type to from type-> <len> <------------------------------ data ----------------------------------------> <checksum>
*/
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

/* Clean out any NULL in serial line.
 * Return 0x00 if nothing or only nuls read, or return byte if something else was read
 * leave fullclean as option as may want to use this to cleanout end of message on get_packet function
 */
 unsigned char cleanOutSerial(int fd, bool fullClean)
 {
   unsigned char byte = 0x00;
   while ( (read(fd, &byte, 1) == 1) && (byte == 0x00 || fullClean) ) {
     //printf("*** Peek Read 0x%02hhx ***\n",byte);
   }
   return byte;
 }  


/*
NEWPACKETADDRESSSPACE 
is test to copy packet to unused address space before send, just incase tc_drain doesn't work correctly 
and we change buffer after function call.  (shouldn;t even happen / be needed buyt good for testing)
*/
#ifdef NEWPACKETADDRESSSPACE
void send_packet(int fd, unsigned char *packet_buffer, int length)
{
  static unsigned char packet[AQ_MAXPKTLEN];
  memcpy(packet, packet_buffer, length);
#else
void send_packet(int fd, unsigned char *packet, int length)
{
#endif

  if (_blocking_mode) {
    //int nwrite = write(fd, packet, length);
    //LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes of %d\n",nwrite,length);
    int nwrite = write(fd, packet, length);
    //if (nwrite < 0)
    if (nwrite != length)
        LOG(RSSD_LOG, LOG_ERR, "write to serial port failed\n");
  } else {
    if (_aqconfig_.readahead_b4_write) {
      if (cleanOutSerial(fd, false) != 0x00) {
        LOG(RSSD_LOG, LOG_ERR, "ERROR on RS485, AqualinkD was too slow in replying to message! (please check for performance issues)\n");
        cleanOutSerial(fd, true);
      }
    }

    int nwrite, i;
    for (i = 0; i < length; i += nwrite) {
      nwrite = write(fd, packet + i, length - i);
      if (nwrite < 0)
        LOG(RSSD_LOG, LOG_ERR, "write to serial port failed\n");
      //else
      //  LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes of %d\n",nwrite,length-1);
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

  // MAYBE Change this back to debug serial
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",length-2);
  //LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes, type 0x%02hhx cmd 0x%02hhx\n",length-2,packet[5],packet[6]);
  logPacketWrite(&packet[1], length-2);
/*
  if (getLogLevel(PDA_LOG) == LOG_DEBUG) {
    char buff[1024];
    beautifyPacket(buff, &packet[1], length, false);
    LOG(PDA_LOG,LOG_DEBUG, "%s", buff);
  }
*/
/*
  if ( getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL || ) {
    // Packet is padded with 0x00, so discard for logging
    LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",length-2);
    logPacket(&packet[1], length-2);
  }*/ /*else if (getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
    static char buf[1000];
    beautifyPacket(buf,&packet[1],length-2);
    LOG(RSSD_LOG,LOG_DEBUG, "Serial write %s\n",buf);
  }*/

  tcdrain(fd); // Make sure buffer has been sent.
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
/*
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
*/
int get_packet(int fd, unsigned char* packet)
{
  unsigned char byte = 0x00;
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

  memset(packet, 0, AQ_MAXPKTLEN);

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

//#ifndef OLD_SERIAL_INIT .. Need to re-do ERROR like EAGAIN with new init


  while (!endOfPacket) {
    bytesRead = read(fd, &byte, 1);
//printf("Read %d 0x%02hhx err=%d fd=%d\n",bytesRead,byte,errno,fd);
    //if (bytesRead < 0 && errno == EAGAIN && packetStarted == FALSE && lastByteDLE == FALSE) {
    //if (bytesRead < 0 && (errno == EAGAIN || errno == 0) && 
    if (bytesRead <= 0 && (errno == EAGAIN || errno == 0) ) {
      if (_blocking_mode) {
         // Something is wrong wrong
        return AQSERR_TIMEOUT;
      } else if (jandyPacketStarted == false && pentairPacketStarted == false && lastByteDLE == false) {
          // We just have nothing else to read
        return 0;
      } else if (++retry > 120 ) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
          //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        if (index > 0) { logPacketError(packet, index); }
        return AQSERR_TIMEOUT;
      }
      delay(1);
    } else if (bytesRead == 1) {
      retry = 0;
      if (_aqconfig_.log_raw_bytes)
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
        return AQSERR_READ;
      }
      delay(100);
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet too large\n");
      logPacketError(packet, index);
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return AQSERR_2LARGE;
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
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      logPacketError(packet, index);
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return AQSERR_CHKSUM;
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      logPacketError(packet, index);
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return AQSERR_CHKSUM;
    }
  }
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    logPacketError(packet, index);
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return AQSERR_2SMALL;
  }

  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  if (_aqconfig_.log_protocol_packets || getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL)
    logPacketRead(packet, index);
  // Return the packet length.
  return index;
}

#else // PLAYBACKMODE

// Need to re-write this if we ever use playback mode again.  Pull info from aq_serial.old.c


#endif





































