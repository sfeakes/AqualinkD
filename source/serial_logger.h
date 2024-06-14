
#ifndef SERIAL_LOGGER_H_
#define SERIAL_LOGGER_H_

/*
int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
  bool panleProbe = true;
  bool rsSerialSpeedTest = false;
  //bool serialBlocking = true;
  bool errorMonitor = false;
*/
//int serial_logger(int rs_fd, char *port_name);
//int serial_logger(int rs_fd, char *port_name, int logPackets, int logLevel, bool panleProbe, bool rsSerialSpeedTest, bool errorMonitor);

int serial_logger (int rs_fd, char *port_name, int logLevel);
void getPanelInfo(int rs_fd, unsigned char *packet_buffer, int packet_length);

#endif // SERIAL_LOGGER_H_