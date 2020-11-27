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

// RTUutils is bundling the send, receive and CRC functions for Modbus RTU communications.
// RTU server and client will make use of it. 
// All functions are static!
class RTUutils {
public:
  friend class ModbusClientRTU;
  friend class ModbusServerRTU;

// calcCRC: calculate the CRC16 value for a given block of data
  static uint16_t calcCRC(const uint8_t *data, uint16_t len);

// calcCRC: calculate the CRC16 value for a given block of data
  static uint16_t calcCRC(ModbusMessage msg);

// validCRC #1: check the CRC in a block of data for validity
  static bool validCRC(const uint8_t *data, uint16_t len);

// validCRC #2: check the CRC of a block of data against a given one
  static bool validCRC(const uint8_t *data, uint16_t len, uint16_t CRC);

// validCRC #1: check the CRC in a message for validity
  static bool validCRC(ModbusMessage msg);

// validCRC #2: check the CRC of a message against a given one
  static bool validCRC(ModbusMessage msg, uint16_t CRC);

// addCRC: extend a RTUMessage by a valid CRC
  static void addCRC(ModbusMessage& raw);

protected:
  RTUutils() = delete;

// receive: get a Modbus message from serial, maintaining timeouts etc.
  static ModbusMessage receive(HardwareSerial& serial, uint32_t timeout, uint32_t& lastMicros, uint32_t interval);

// send: send a Modbus message in either format (ModbusMessage or data/len)
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, const uint8_t *data, uint16_t len);
  static void send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, ModbusMessage raw);
};

#endif
