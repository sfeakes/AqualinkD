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

#include "aq_serial.h"
#include "utils.h"

//#define BLOCKING_MODE

static struct termios _oldtio;




void log_packet(char *init_str, unsigned char* packet, int length)
{
  if ( getLogLevel() < LOG_DEBUG_SERIAL)
    return;

  int cnt;
  int i;
  char buff[MAXLEN];

  cnt = sprintf(buff, "%s", init_str);
  cnt += sprintf(buff+cnt, " | HEX: ");
  //printHex(packet_buffer, packet_length);
  for (i=0;i<length;i++)
    cnt += sprintf(buff+cnt, "0x%02hhx|",packet[i]);

  cnt += sprintf(buff+cnt, "\n");
  logMessage(LOG_DEBUG_SERIAL, buff);
/*
  int i;
  char temp_string[64];
  char message_buffer[MAXLEN];

  sprintf(temp_string, "Send 0x%02hhx|", packet[0]);
  strcpy(message_buffer, temp_string);

  for (i = 1; i < length; i++) {
    sprintf(temp_string, "0x%02hhx|", packet[i]);
    strcat(message_buffer, temp_string);
  }

  strcat(message_buffer, "\n");
  logMessage(LOG_DEBUG, message_buffer);
  */
}

const char* get_packet_type(unsigned char* packet, int length)
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
    case CMD_MSG_LONG:
      return "Message";
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
    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PKT_CMD]);
      return buf;
    break;
  }
}

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

  log_packet("Sent ", ackPacket, length);

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
    sprintf(buf, "Sent     %8.8s ", get_packet_type(ackPacket+1, length));
    log_packet(buf, ackPacket, length);
  }
}

void send_messaged(int fd, unsigned char destination, char *message)
{
  const int length = 24;
  int i;
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

  log_packet("Sent ", msgPacket, length);
}

void send_ack(int fd, unsigned char command)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  if(command != NUL) {
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
  
  log_packet("Sent ", ackPacket, length);
}

// Reads the bytes of the next incoming packet, and
// returns when a good packet is available in packet
// fd: the file descriptor to read the bytes from
// packet: the unsigned char buffer to store the bytes in
// returns the length of the packet
int get_packet(int fd, unsigned char* packet)
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
      if (retry > 10)
        return 0;
        
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
    log_packet("Bad packet ", packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN) {
    logMessage(LOG_WARNING, "Serial read too small\n");
    log_packet("Bad packet ", packet, index);
    return 0;
  }

  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  // Return the packet length.
  return index;
}

