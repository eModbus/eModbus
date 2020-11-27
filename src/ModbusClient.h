// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_H
#define _MODBUS_CLIENT_H
#include "ModbusMessage.h"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

typedef void (*MBOnData) (ModbusMessage msg, uint32_t token);
typedef void (*MBOnError) (Modbus::Error errorCode, uint32_t token);

class ModbusClient {
public:
  bool onDataHandler(MBOnData handler);   // Accept onData handler 
  bool onErrorHandler(MBOnError handler); // Accept onError handler 
  uint32_t getMessageCount();              // Informative: return number of messages created
  // Virtual addRequest variant needed internally. All others done in the derived client classes by template!
  virtual Error addRequest(ModbusMessage msg, uint32_t token) = 0;
  
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
