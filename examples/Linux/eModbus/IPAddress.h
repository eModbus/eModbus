// =================================================================================================
// eModbus: Copyright 2020,2021 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _IPADDRESS_H
#define _IPADDRESS_H
#include "options.h"

#if IS_LINUX
#include <string>
#include <arpa/inet.h>

using std::string;

class IPAddress {
public:
  IPAddress();
  virtual ~IPAddress() {}
  IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
  IPAddress(uint32_t w);
  IPAddress(const char *ip);

  operator uint32_t();
  uint8_t operator[](int index) const;
  uint8_t& operator[](int index);
  bool operator==(IPAddress other);
  bool operator==(uint32_t w);
  bool operator==(const char *ip);
  bool operator!=(IPAddress other);
  bool operator!=(uint32_t w);
  bool operator!=(const char *ip);
  IPAddress& operator=(uint32_t w);
  IPAddress& operator=(const char *ip);
  operator std::string() const;

protected:
  union {
    uint32_t word;
    uint8_t byte[4];
  } addr;
  uint8_t vault;
  uint32_t fromChar(const char *ip);
};

const IPAddress NIL_ADDR(0, 0, 0, 0);

#endif // IS_LINUX
#endif // _IPADDRESS_H
