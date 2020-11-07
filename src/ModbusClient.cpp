// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClient.h"
#include "Logging.h"

uint16_t ModbusClient::instanceCounter = 0;

// Default constructor: set the default timeout to 2000ms, zero out all other 
ModbusClient::ModbusClient() :
  messageCount(0),
  worker(NULL),
  onData(nullptr),
  onError(nullptr) { instanceCounter++; }

// onDataHandler: register callback for data responses
void ModbusClient::onDataHandler(MBOnData handler) {
  if (onData) {
    log_e("onData handler was already claimed\n");
  } else {
    onData = handler;
  }
}

// onErrorHandler: register callback for error responses
void ModbusClient::onErrorHandler(MBOnError handler) {
  if (onError) {
    log_e("onError handler was already claimed\n");
  } else {
    onError = handler;
  }
}

// getMessageCount: return message counter value
uint32_t ModbusClient::getMessageCount() {
  return messageCount;
}
