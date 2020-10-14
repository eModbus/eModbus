// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_RTU_H
#define _MODBUS_SERVER_RTU_H

#include <Arduino.h>
#include "HardwareSerial.h"
#include "ModbusServer.h"
#include "RTUutils.h"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

using std::vector;
using RTUMessage = std::vector<uint8_t>;

class ModbusServerRTU : public ModbusServer {
public:
  // Constructor
  ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, int rtsPin = -1);

  // Destructor: closes the connections
  ~ModbusServerRTU();

  // start: create task with RTU server to accept requests
  bool start(int coreID = -1);

  // stop: drop all connections and kill server task
  bool stop();

protected:
  inline void isInstance() { }

  static uint8_t instanceCounter;
  TaskHandle_t serverTask;
  uint32_t serverTimeout;
  HardwareSerial& MSRserial;
  uint32_t MSRinterval;
  uint32_t MSRlastMicros;
  uint32_t MSRrtsPin;

  // serve: loop function for server task
  static void serve(ModbusServerRTU *myself);

};

#endif
