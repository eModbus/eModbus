// =================================================================================================
// eModbus: Copyright 2020,2021 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "options.h"

#if IS_LINUX
#include "IPAddress.h"

// Standard constructor - set to 0.0.0.0
IPAddress::IPAddress() {
  addr.word = 0;
}

// Constructor using 4 single byte values
IPAddress::IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  addr.byte[0] = b0;
  addr.byte[1] = b1;
  addr.byte[2] = b2;
  addr.byte[3] = b3;
}

// Constructor taking a 4-byte unsigned value
IPAddress::IPAddress(uint32_t w) {
  addr.word = ::htonl(w);
}

// Constructor using a written IP string "1.2.3.4"
IPAddress::IPAddress(const char *ip) {
  addr.word = fromChar(ip);
}

// operator uint32_t: return raw 4-byte value
IPAddress::operator uint32_t() {
  return ::ntohl(addr.word);
}

// operator[] const: return a byte 0..3 or value 0 for invalid indexes
uint8_t IPAddress::operator[](int index) const {
  if (index >= 0 && index <= 3) return addr.byte[index];
  return 0;
}

// reference operator[]: return byte 0..3 or "The Vault" for invalid indexes
uint8_t& IPAddress::operator[](int index) {
  if (index >= 0 && index <= 3) return addr.byte[index];
  return vault;
}
  
// operator== for IPAdresses
bool IPAddress::operator==(IPAddress other) {
  return addr.word == other.addr.word;
}

// operator== for raw 4-byte values
bool IPAddress::operator==(uint32_t w) {
  return addr.word == ::htonl(w);
}

// operator== for written IP strings
bool IPAddress::operator==(const char *ip) {
  return addr.word == fromChar(ip);
}

// Same triple for inequality
bool IPAddress::operator!=(IPAddress other) { 
  return addr.word != other.addr.word; 
}
bool IPAddress::operator!=(uint32_t w) { 
  return addr.word != ::htonl(w); 
}
bool IPAddress::operator!=(const char *ip) { 
  return addr.word != fromChar(ip);
}

// Assignment of raw 4-byte value
IPAddress& IPAddress::operator=(uint32_t w) {
  addr.word = ::htonl(w);
  return *this;
}

// Assignment from written IP string
IPAddress& IPAddress::operator=(const char *ip) {
  addr.word = fromChar(ip);
  return *this;
}

// Conversion to std::string
IPAddress::operator std::string() const {
  char buf[16];
  snprintf(buf, 16, "%u.%u.%u.%u", addr.byte[0], addr.byte[1], addr.byte[2], addr.byte[3]);
  return std::string(buf);
}

// fromChar: utility function to convert written IP strings 
// Note: this will return a uint32_t in network order!
uint32_t IPAddress::fromChar(const char *ip) {
  // Target data structure
  union {
    uint32_t val;
    uint8_t bval[4];
  } address;
  address.val = 0;

  uint8_t byte = 0;      // Byte index in address
  uint8_t b = 0;         // working variable
  bool leave = false;    // Termination flag
  const char *cp = ip;

  // Loop while we are not finished
  while (!leave) {
    switch (*cp) {
    case 0x00:  // EOI
    case '.':   // digit group divider
      address.bval[byte++] = b;
      b = 0;
      // EOI? Then leave!
      if (*cp == 0x00) leave = true;
      // No. Are we behind the fourth group already?
      else if (byte == 4) {
        // Yes. Something wrong - bail out.
        address.val = 0;
        leave = true;
      }
      break;
    case '0':  // Digits proper
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      b *= 10;
      b += *cp - '0';
      break;
    default:  // All else is not acceptable
      address.val = 0;
      leave = true;
    }
    cp++;
  }
  return address.val;
}

#endif // IS_LINUX

