// =================================================================================================
// eModbus: Copyright 2020,2021 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "options.h"

#if IS_LINUX
#include "Client.h"
#include "Logging.h"
#include <libexplain/connect.h>

// Default constructor: just initialize host variables
Client::Client() : sockfd(-1), host(NIL_ADDR), port(0) { } 

// Constructor with IP/port: initialize, then try to connect
Client::Client(IPAddress ip, uint16_t p) : sockfd(-1), host(NIL_ADDR), port(0) {
  connect(ip, p);
}

// Constructor with hostname/port: initialize, then try to connect
Client::Client(const char *hostname, uint16_t p) : sockfd(-1), host(NIL_ADDR), port(0) {
  connect(hostname, p);
}

// Destructor: terminate connection, if any.
Client::~Client() { stop(); }

// connect with IP/port: establish a connection
int Client::connect(IPAddress ip, uint16_t p) {
// Are we still connected? Then terminate the existing connection.
  if (connected()) disconnect();

// Get a fresh socket
  sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    LOG_E("Error %d opening socket\n", errno);
  }

// Set up sockaddr_in struct
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  char buf[16];
  snprintf(buf, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  server.sin_addr.s_addr = ::htonl(uint32_t(ip));
  server.sin_port = ::htons(p);

// Try to connect
  int rc = ::connect(sockfd, (struct sockaddr *)&server, sizeof(server));

// Failed?
  if (rc < 0) {
  // Yes. Print out error and return
    LOG_E("Error %d connecting to %s:%d -\n", rc, buf, p);
    LOG_E("%s\n\n", explain_connect(sockfd, (struct sockaddr *)&server, sizeof(server)));
    return rc;
  }

// Connection was successful. Remember host data and return
  LOG_D("Connected.\n");
  host = ip;
  port = p;
  return 0;
}

// connect with hostname/port: try to find and connect to host 
int Client::connect(const char *host, uint16_t port) {
// Get IP for given hostname
  char ip[16];
  IPAddress myHost = hostname_to_ip(host);
  if (myHost != NIL_ADDR) {
  // Failure. Report and bail out
    LOG_E("No such host '%s'\n", host);
    return -1;
  }
  // Success - connect to found IP/port
  return connect(myHost, port);
}

// disconnect: cut any existing connection
bool Client::disconnect() {
// Do we have a valid socket?
  if (sockfd >= 0) {
  // Yes. Try to empty buffer...
    const int BUFLEN(256);
    uint8_t buf[BUFLEN];
    int sz = 0;
    auto lastCall = millis();
  // ...but for 2s only
    while ((millis() - lastCall < 2000) && (sz = ::recv(sockfd, buf, BUFLEN, MSG_DONTWAIT)) > 0) {
      HEXDUMP_D("Read", buf, sz);
    }
  // Close socket
    ::close(sockfd);
    sockfd = -1;
  }
  host = NIL_ADDR;
  port = 0;
  return true;
}

// write (single byte): send out 1 byte
size_t Client::write(uint8_t t) {
// Send byte, disabled SIGPIPE
  int rc = ::send(sockfd, &t, 1, MSG_NOSIGNAL);
  LOG_D("send '%c' -> %d\n", (t > ' ' ? (char)t : '.'), rc);
// Something wrong?
  if (rc <= 0) {
  // Yes, print it out
    LOG_E("Error sending: %s (%d)\n", strerror(errno), errno);
    return 0;
  }
  return 1;
}

// write (buffer): send block of data
size_t Client::write(const uint8_t *buf, size_t size) {
// Send buffer, disabled SIGPIPE
  int rc = ::send(sockfd, buf, size, MSG_NOSIGNAL);
  LOG_D("send buffer[%d] -> %d\n", size, rc);
// Something wrong?
  if (rc <= 0) {
  // Yes, print it out
    LOG_E("Error sending: %s (%d)\n", strerror(errno), errno);
    return 0;
  }
  return rc;
}

// available: return number of waiting bytes to be read - if any (but 255 max)
int Client::available() {
  uint8_t buf[256];
interrupted:
// peek-read buffer
  int r = ::recv(sockfd, &buf, 256, MSG_DONTWAIT|MSG_PEEK);
// Anything >0 is number of bytes available
  if (r > 0) return r;
// We may have been prevented to read
  if (r < 0 && errno == EINTR) goto interrupted;
// All else is either an empty buffer or no connection at all
  return 0;
}

// read: get a single byte from buffer
int Client::read() {
  int x;
interrupted:
// read a byte
  int r = ::read(sockfd, &x, 1);
// a 1 signals success
  if (r == 1) return x;
// We may have been prevented to read
  if (r < 0 && errno == EINTR) goto interrupted;
// A zero signals no data available
  if (r == 0) return -1;
// All else is some error state
// Lazyness... No error handling here!
  return r;
}

// read: get a buffer full of data
int Client::read(uint8_t *buf, size_t size) {
interrupted:
// peek-read buffer
  int r = ::read(sockfd, buf, size);
// Anything >0 is number of bytes available
  if (r > 0) return r;
// We may have been prevented to read
  if (r < 0 && errno == EINTR) goto interrupted;
// All else is either an empty buffer or no connection at all
  return 0;
}

// peek: read one byte without popping it from the buffer
int Client::peek() {
  int x;
interrupted:
// peek-read a byte
  int r = ::recv(sockfd, &x, 1, MSG_DONTWAIT|MSG_PEEK);
// a 1 signals success
  if (r == 1) return x;
// We may have been prevented to read
  if (r < 0 && errno == EINTR) goto interrupted;
// All else is some error state
  return r;
}

// flush: no op for now
void Client::flush() {
}

// stop: empty buffers and close connection
void Client::stop() {
  if (connected()) disconnect();
}

// connected: return stat eof current host connetion
uint8_t Client::connected() {
  char x;
interrupted:
// Try to peek a byte
  int r = ::recv(sockfd, &x, 1, MSG_DONTWAIT|MSG_PEEK);
// Unsuccessful?
  if (r < 0) {
  // Check errno for the reasons
    switch (errno) {
    case EINTR:     goto interrupted;
    case EAGAIN:    break; // empty rx queue
    case ETIMEDOUT: break; // recv timeout
    case ENOTCONN:  r = 0; break; // not connected yet
    default:        r = 0; break; // Assume closed...
    }
  }
  LOG_D("returns %d (r=%d, errno=%d, %s)\n", ((r != 0) ? 1 : 0), r, errno, strerror(errno));
  return ((r != 0) ? 1 : 0);
}

// bool operator: return connected() state
Client::operator bool() { return connected() == 1; }

// setNoDelay: disable Nagle algorithm for fast handling of small data packets
void Client::setNoDelay(bool yesNo) {
  int yes = (yesNo ? 1 : 0);
  setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int));
}

// hostname_to_ip: try to find an IP address for a given host name
IPAddress Client::hostname_to_ip(const char *hostname)
{
  IPAddress returnIP = NIL_ADDR;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_in *h;
  int rv;

// Set up request addrinfo struct
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  LOG_D("Looking for '%s'\n", hostname);

// ask for host data
  if ((rv = getaddrinfo(hostname, NULL, &hints, &servinfo)) != 0) {
    LOG_E("getaddrinfo: %s\n", gai_strerror(rv));
    return returnIP;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    h = (struct sockaddr_in *)p->ai_addr;
    returnIP = ::htonl(h->sin_addr.s_addr);
    if (returnIP != NIL_ADDR) break;
  }
  // Release allocated memory
  freeaddrinfo(servinfo);

  if (returnIP != NIL_ADDR) {
    LOG_D("Host '%s'=%s\n", hostname, string(returnIP).c_str());
  } else {
    LOG_D("No IP for '%s' found\n", hostname);
  }
  return returnIP;
}

#endif // IS_LINUX

