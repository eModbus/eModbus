// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_H
#define _MODBUS_CLIENT_H
#include "ModbusTypeDefs.h"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

typedef void (*MBOnData) (uint8_t serverID, uint8_t functionCode, const uint8_t *data, uint16_t data_length, uint32_t token);
typedef void (*MBOnError) (Modbus::Error, uint32_t token);

class ModbusClient {
public:
  void onDataHandler(MBOnData handler);   // Accept onData handler 
  void onErrorHandler(MBOnError handler); // Accept onError handler 
  uint32_t getMessageCount();              // Informative: return number of messages created
  
protected:
  ModbusClient();             // Default constructor
  virtual void isInstance() = 0;   // Make class abstract
  uint32_t messageCount;           // Number of requests generated. Used for transactionID in TCPhead
  TaskHandle_t worker;             // Interface instance worker task
  MBOnData onData;                 // Response data handler
  MBOnError onError;               // Error response handler
  static uint16_t instanceCounter; // Number of ModbusClients created
};

#endif
