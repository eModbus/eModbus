#ifndef _MODBUS_MESSAGE_TCP_H
#define _MODBUS_MESSAGE_TCP_H
#include "ModbusMessage.h"

// Struct describing the TCP header of Modbus packets
struct ModbusTCPhead {
  uint16_t transactionID;     // Client-defined identification
  uint16_t protocolID;        // const 0x0000
  uint16_t len;               // Length of remainder of TCP packet
};

class TCPRequest : public ModbusRequest {
public:
  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
  uint32_t  targetHost;        // 4 bytes target IP address MSB first
  uint16_t  targetPort;        // 2 byte target port number
protected:
  void isInstance() { return; }       // Make class instantiable
};

class TCPResponse : public ModbusResponse {
public:
  ModbusTCPhead tcpHead;       // Header structure for Modbus TCP packets
protected:
  void isInstance() { return; }       // Make class instantiable
};

#endif