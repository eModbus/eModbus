#ifndef _PHYSICAL_INTERFACE_H
#define _PHYSICAL_INTERFACE_H
#include "ModbusTypeDefs.h"

using namespace ModbusClient;

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

class PhysicalInterface {
public:
  bool setTimeOut(uint32_t TOV);           // Set default timeout value for interface
  void onDataHandler(MBOnData handler);   // Accept onData handler 
  void onErrorHandler(MBOnError handler); // Accept onError handler 
  void onGenerateHandler(MBOnGenerate handler); // Accept onGenerate handler 
  uint32_t getMessageCount();              // Informative: return number of messages created
  
protected:
  PhysicalInterface(uint32_t timeOutValue); // Default constructor
  virtual void isInstance() = 0;   // Make class abstract
  uint32_t messageCount;           // Number of requests generated. Used for transactionID in TCPhead
  TaskHandle_t worker;             // Interface instance worker task
  uint32_t timeOutValue;           // Interface default timeout
  MBOnData onData;                // Response data handler
  MBOnGenerate onGenerate;                // Response data handler
  MBOnError onError;              // Error response handler
};

#endif