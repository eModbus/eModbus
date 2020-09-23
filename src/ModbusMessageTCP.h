// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_MESSAGE_TCP_H
#define _MODBUS_MESSAGE_TCP_H
#include "ModbusMessage.h"
#include <IPAddress.h>
#include <string.h>
#include <Arduino.h>

// Struct describing the TCP header of Modbus packets
struct ModbusTCPhead {
  uint16_t transactionID;     // Caller-defined identification
  uint16_t protocolID;        // const 0x0000
  uint16_t len;               // Length of remainder of TCP packet
};

// Struct describing a target server
struct TargetHost {
  IPAddress     host;         // IP address
  uint16_t      port;         // Port number
  uint32_t      timeout;      // Time in ms waiting for a response
  uint32_t      interval;     // Time in ms to wait between requests
  
  inline TargetHost& operator=(TargetHost& t) {
    host = t.host;
    port = t.port;
    timeout = t.timeout;
    interval = t.interval;
    return *this;
  }
   
  inline TargetHost() :
    host(IPAddress(0, 0, 0, 0)),
    port(0),
    timeout(0),
    interval(0)
  { }

  inline TargetHost(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) :
    host(host),
    port(port),
    timeout(timeout),
    interval(interval)
  { }
};

class TCPRequest : public ModbusRequest {
  friend class ModbusTCP;
  friend class ModbusTCPasync;
  friend class TCPResponse;
protected:
  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
  TargetHost target;           // Target server description

  // Default constructor
  TCPRequest(TargetHost target, uint16_t dataLen, uint32_t token = 0);

  // Factory methods to create valid Modbus messages from the parameters
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  
  // 2. one uint16_t parameter (FC 0x18)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  
  // 4. three uint16_t parameters (FC 0x16)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  // 7. generic method for preformatted data ==> count is counting bytes!
  static TCPRequest *createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  void isInstance() { return; }       // Make class instantiable
  static Error isValidHost(TargetHost target);    // Check host address


#ifdef TESTING
// ************************** Test *******************
  void dump(const char *label);   // Overloading ModbusMessage::dump()
#endif
};

class TCPResponse : public ModbusResponse {
  friend class ModbusTCP;
protected:
  // Default constructor
  explicit TCPResponse(uint16_t dataLen);

  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
  void isInstance() { return; }       // Make class instantiable

#ifdef TESTING
// ************************** Test *******************
  void dump(const char *label);   // Overloading ModbusMessage::dump()
#endif
};

#endif
