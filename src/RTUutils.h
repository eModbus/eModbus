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

// RTUutils is bundling the send, receive and CRC functions for Modbus RTU communications.
// RTU server and client will make use of it. 
// All functions are static!
class RTUutils {
public:
  friend class ModbusMessageRTU;
  friend class ModbusClientRTU;
  friend class ModbusServerRTU;

// calcCRC: calculate the CRC16 value for a given block of data
  static uint16_t calcCRC(const uint8_t *data, uint16_t len);

// validCRC #1: check the CRC in a message for validity
  static bool validCRC(const uint8_t *data, uint16_t len);

// validCRC #2: check the CRC of a message against a given one
  static bool validCRC(const uint8_t *data, uint16_t len, uint16_t CRC);

// addCRC: extend a RTUMessage by a valid CRC
  static void addCRC(RTUMessage& raw);

// receive: get a Modbus message from serial, maintaining timeouts etc.
  static RTUMessage receive(HardwareSerial& serial, uint32_t timeout, uint32_t& lastMicros, uint32_t interval, const char *lbl);

// send: send a Modbus message in either format (RTUMessage or data/len)
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, const uint8_t *data, uint16_t len, const char *lbl);
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, RTUMessage raw, const char *lbl);

protected:
  RTUutils() = delete;
};

#endif
