#ifndef _MODBUS_MESSAGE_RTU_H
#define _MODBUS_MESSAGE_RTU_H
#include "ModbusMessage.h"

class RTURequest : public ModbusRequest {
public:
  uint16_t CRC;          // CRC16 value
protected:
  void isInstance() { return; }        // Make class instantiable
};

class RTUResponse : public ModbusResponse {
public:
  uint16_t CRC;          // CRC16 value
protected:
  void isInstance() { return; }        // Make class instantiable
};

#endif