// Copyright 2012 Michael Harwerth - miq1 AT gmx DOT de

#include "parseTarget.h"

int parseTarget(const char *source, IPAddress& ip, uint16_t& port, uint8_t& serverID) {
  // Target host parameters
  IPAddress targetIP = NIL_ADDR;
  uint16_t targetPort = 502;
  uint8_t targetServer = 1;

  // RegEx for the first argument if it is an IP
  regex re("^((\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3}))(:(\\d{1,5})(:(\\d{1,3}))?)?$");
  cmatch ip_result;
  bool isIP = true;
  unsigned int portOffset = 0;    // sub_match with port number, if any

  // Do we really have an IP pattern?
  if (regex_match(source, ip_result, re)) {
    // Yes. Check the 4 digit groups to be 1..255
    for (uint8_t i = 2; i < 6; ++i) {
      // Get sub_match as integer
      int group = std::stoi(ip_result[i].str());
      // If it is out of bounds, deny IP
      if (group <= 0 || group > 255) isIP = false;
    }
  } else {
    // No, apparently no IP.
    isIP = false;
  }

  // Did we find an IP address?
  if (isIP) {
    // Yes. take it as target
    targetIP = ip_result[1].str().c_str();
    // Is a port etc. appended to the IP?
    if (ip_result[7].length()) {
      // Yes. save position for later analysis
      portOffset = 7;
    }
  } else {
    // No IP recognized. We may have a hostname instead
    regex hre("^(([a-zA-Z0-9][a-zA-Z0-9\\-]*)(\\.[a-zA-Z0-9][a-zA-Z0-9\\-]*)*)(:(\\d{1,5})(:(\\d{1,3}))?)?$");
    // Does the hostname pattern match?
    if (regex_match(source, ip_result, hre)) {
      // Yes. Try to convert the hostname into its IP address
      targetIP = Client::hostname_to_ip(ip_result[1].str().c_str());
      // Succeeded?
      if (targetIP != NIL_ADDR) {
        // Yes. Use it further on.
        isIP = true;
        // Is a port number etc. appended to the hostname?
        if (ip_result[5].length()) {
          // Yes. Save position for analysis
          portOffset = 5;
        }
      } else {
        // No, no IP was found. We cannot identify the target host, so bye.
        return -1;
      }
    } else {
      // No, the hostname pattern does not match either.
      return -1;
    }
  }

  // Do we have a valid IP address and is there more to read?
  if (isIP && portOffset > 0) {
    // Yes. Get the port number from the sub_match
    int p = std::stoi(ip_result[portOffset].str());
    // Is the port number a valid one?
    if (p && p < 65536) {
      // Yes. Take it as target port
      targetPort = p;
      // Is there a server ID also?
      if (ip_result[portOffset + 2].length()) {
        // Yes. get the number from the sub_match as an integer
        int s = std::stoi(ip_result[portOffset + 2].str());
        // Is it a valid Modbus server ID?
        if (s && s < 248) {
          // Yes. take it.
          targetServer = s;
        } else {
          // No. Bail out.
          return -3;
        }
      }
    } else {
      // No, port number is something odd. Exit.
      return -2;
    }
  }

  // We seem to be done here, return the results
  ip = targetIP;
  port = targetPort;
  serverID = targetServer;
  return 0;
}

