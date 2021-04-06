// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _CLIENT_H
#define _CLIENT_H
#include "options.h"

#if IS_LINUX
#include <unistd.h> 
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include "IPAddress.h"

class Client {
public:
  Client();
  Client(IPAddress ip, uint16_t port);
  Client(const char *hostname, uint16_t port);
  ~Client();
  int connect(IPAddress ip, uint16_t port);
  int connect(const char *host, uint16_t port);
  bool disconnect();
  size_t write(uint8_t t);
  size_t write(const uint8_t *buf, size_t size);
  int available();
  int read();
  int read(uint8_t *buf, size_t size);
  int peek();
  void flush();
  void stop();
  void setNoDelay(bool yesNo);
  uint8_t connected();
  operator bool();
  static IPAddress hostname_to_ip(const char *hostname);

protected:
  int sockfd;
  IPAddress host;
  uint16_t port;
  struct sockaddr_in server;
};

#endif // IS_LINUX
#endif // _CLIENT_H
