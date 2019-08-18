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
#define PENTAIR_LENGTH_FIX

static struct termios _oldtio;

//static struct aqconfig *_config_parameters;
//static struct aqconfig *_aqualink_config;
bool _pda_mode = false;

void set_pda_mode(bool mode)
{
  if (mode)
    logMessage(LOG_NOTICE, "AqualinkD is using PDA mode\n");

  _pda_mode = mode;
}

bool pda_mode()
{
  return _pda_mode;
}

void log_packet(int level, char *init_str, unsigned char* packet, int length)
{
  
  if ( getLogLevel() < level) {
    //logMessage(LOG_INFO, "Send '0x%02hhx'|'0x%02hhx' to controller\n", packet[5] ,packet[6]);
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
  logMessage(level, buff);
  //logMessage(LOG_DEBUG_SERIAL, buff);
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
    logMessage(LOG_ERR, "Unable to open port: %s\n", tty);
    return -1;
  }

  logMessage(LOG_DEBUG_SERIAL, "Openeded serial port %s\n",tty);
  
#ifdef BLOCKING_MODE
  fcntl(fd, F_SETFL, 0);
  newtio.c_cc[VMIN]= 1;
  newtio.c_cc[VTIME]= 0;
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s to blocking mode\n",tty);
#else
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
  newtio.c_cc[VMIN]= 0;
  newtio.c_cc[VTIME]= 1;
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s to non blocking mode\n",tty);
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
  
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s io attributes\n",tty);

  return fd;
}

/* close tty port */
void close_serial_port(int fd)
{
  tcsetattr(fd, TCSANOW, &_oldtio);
  close(fd);
  logMessage(LOG_DEBUG_SERIAL, "Closed serial port\n");
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
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  //logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
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
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  //logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
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
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  //logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
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
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to controller\n", command);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
#endif  
  
  //logMessage(LOG_DEBUG, "Sent '0x%02hhx' to controller\n", command);
  log_packet(LOG_DEBUG_SERIAL, "Sent ", ackPacket, length);

  //logMessage(LOG_DEBUG, "Sent '0x%02hhx' to controller \n", command);
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
        logMessage(LOG_WARNING, "Serial read timeout\n");
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
      logMessage(LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
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
      logMessage(LOG_WARNING, "Serial packet too large\n");
      log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }

  //logMessage(LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));

  if (generate_checksum(packet, index) != packet[index-3]){
    logMessage(LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN && packetStarted) { //NSF. Sometimes we get END sequence only, so just ignore.
  //} else if (index < AQ_MINPKTLEN) { //NSF. Sometimes we get END sequence only, so just ignore.
    logMessage(LOG_WARNING, "Serial read too small\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
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
  int i, sum, n;
  n = packet[8] + 9;
  sum = 0;
  for (i = 3; i < n; i++) {
    sum += (int) packet[i];
  }

#ifndef PENTAIR_LENGTH_FIX
  if (sum == (packet[length-1] * 256 + packet[length]))
    return true;
#else
  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;
#endif

  return false;
}

int get_packet_new(int fd, unsigned char* packet)
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
        logMessage(LOG_WARNING, "Serial read timeout\n");
        log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        return 0;
      }
      retry++;
      delay(10);
    } else if (bytesRead == 1) {

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
#ifndef PENTAIR_LENGTH_FIX
          index--;
#endif
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
      logMessage(LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
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
      logMessage(LOG_WARNING, "Serial packet too large\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }

  //logMessage(LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));
  if (jandyPacketStarted) {
    if (check_jandy_checksum(packet, index) != true){
      logPacketError(packet, index);
      logMessage(LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      logPacketError(packet, index);
      logMessage(LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  }
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    logMessage(LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    logPacketError(packet, index);
    logMessage(LOG_WARNING, "Serial read too small\n");
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
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

    logMessage(LOG_DEBUG_SERIAL, "PLAYBACK read %d bytes\n",packet_length);
    //printf("To 0x%02hhx, type %15.15s, length %2.2d ", packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length),packet_length);
    //fputs ( line, stdout ); 

    if (getProtocolType(packet_buffer)==JANDY) {
      if (generate_checksum(packet_buffer, packet_length) != packet_buffer[packet_length-3]) {
        logPacketError(packet_buffer, packet_length);
        logMessage(LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      } else
        logPacket(packet_buffer, packet_length);
    } else {
      //check_pentair_checksum(packet_buffer, packet_length);
      //check_pentair_checksum(packet_buffer, packet_length-1);
      //check_pentair_checksum(packet_buffer, packet_length+1);
      if (check_pentair_checksum(packet_buffer, packet_length-1) != true) {
        logPacketError(packet_buffer, packet_length);
        logMessage(LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
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
        logMessage(LOG_WARNING, "Serial read timeout\n");
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
      logMessage(LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }
  }

  if (generate_checksum(packet, index) != packet[index-3]){
    logMessage(LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN) {
    logMessage(LOG_WARNING, "Serial read too small\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  //if (_config_parameters.debug_RSProtocol_packets || getLogLevel() >= LOG_DEBUG_SERIAL) 
  //      logPacket(packet_buffer, packet_length);
  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);

  // Return the packet length.
  return index;
}

