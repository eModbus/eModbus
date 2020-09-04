#ifndef _MODBUS_MESSAGE_TCP_H
#define _MODBUS_MESSAGE_TCP_H
#include "ModbusMessage.h"

// Struct describing the TCP header of Modbus packets
struct ModbusTCPhead {
  uint16_t transactionID;     // Caller-defined identification
  uint16_t protocolID;        // const 0x0000
  uint16_t len;               // Length of remainder of TCP packet
};

class TCPRequest : public ModbusRequest {
  friend class ModbusTCPsingle;
  friend class ModbusTCPmulti;
  friend class ModbusTCPasync;
protected:
  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
  uint32_t  targetHost;        // 4 bytes target IP address MSB first
  uint16_t  targetPort;        // 2 byte target port number

  // Default constructor
  TCPRequest(uint32_t targetHost, uint16_t targetPort, uint32_t token = 0);

  // Factory methods to create valid Modbus messages from the parameters
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  
  // 2. one uint16_t parameter (FC 0x18)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  
  // 4. three uint16_t parameters (FC 0x16)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  static TCPRequest *createTCPRequest(uint8_t& returnCode, uint32_t targetHost, uint16_t targetPort, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  void isInstance() { return; }       // Make class instantiable
};

class TCPResponse : public ModbusResponse {
  friend class ModbusTCP;
protected:
  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
  void isInstance() { return; }       // Make class instantiable
};

#endif