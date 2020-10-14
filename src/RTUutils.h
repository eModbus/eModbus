// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _RTU_UTILS_H
#define _RTU_UTILS_H
#include <stdint.h>
#include <vector>
#include "HardwareSerial.h"
#include "ModbusTypeDefs.h"

using namespace Modbus;  // NOLINT
using RTUMessage = std::vector<uint8_t>;

class RTUutils {
public:
  friend class ModbusMessageRTU;
  friend class ModbusClientRTU;
  friend class ModbusServerRTU;

  static uint16_t calcCRC(const uint8_t *data, uint16_t len);
  static bool validCRC(const uint8_t *data, uint16_t len);
  static bool validCRC(const uint8_t *data, uint16_t len, uint16_t CRC);
  static void addCRC(RTUMessage& raw);

  static RTUMessage receive(HardwareSerial& serial, uint32_t timeout, uint32_t& lastMicros, uint32_t interval);
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, const uint8_t *data, uint16_t len);
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, RTUMessage raw);

protected:
  RTUutils() = delete;
};

#endif
