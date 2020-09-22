#ifndef _PHYSICAL_INTERFACE_H
#define _PHYSICAL_INTERFACE_H
#include "ModbusTypeDefs.h"

using ModbusClient::MBOnData;
using ModbusClient::MBOnError;

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

class PhysicalInterface {
public:
  void onDataHandler(MBOnData handler);   // Accept onData handler 
  void onErrorHandler(MBOnError handler); // Accept onError handler 
  uint32_t getMessageCount();              // Informative: return number of messages created
  
protected:
  PhysicalInterface();             // Default constructor
  virtual void isInstance() = 0;   // Make class abstract
  uint32_t messageCount;           // Number of requests generated. Used for transactionID in TCPhead
  TaskHandle_t worker;             // Interface instance worker task
  MBOnData onData;                 // Response data handler
  MBOnError onError;               // Error response handler
  static uint16_t instanceCounter; // Number of PhysicalInterfaces created
};

#endif
