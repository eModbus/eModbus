// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClient.h"
#undef LOCAL_LOG_LEVEL
#include "Logging.h"

uint16_t ModbusClient::instanceCounter = 0;

// Default constructor: set the default timeout to 2000ms, zero out all other 
ModbusClient::ModbusClient() :
  messageCount(0),
  #if HAS_FREERTOS
  worker(NULL),
  #elif IS_LINUX
  worker(0),
  #endif
  onData(nullptr),
  onError(nullptr),
  onResponse(nullptr) { instanceCounter++; }

// onDataHandler: register callback for data responses
bool ModbusClient::onDataHandler(MBOnData handler) {
  if (onData) {
    LOG_E("onData handler was already claimed\n");
    return false;
  } else if (onResponse) {
    LOG_E("onData handler is unavailable with an onResponse handler\n");
    return false;
  }
  onData = handler;
  return true;
}

// onErrorHandler: register callback for error responses
bool ModbusClient::onErrorHandler(MBOnError handler) {
  if (onError) {
    LOG_E("onError handler was already claimed\n");
    return false;
  } else if (onResponse) {
    LOG_E("onError handler is unavailable with an onResponse handler\n");
    return false;
  } 
  onError = handler;
  return true;
}

// onResponseHandler: register callback for error responses
bool ModbusClient::onResponseHandler(MBOnResponse handler) {
  if (onError || onData) {
    LOG_E("onResponse handler is unavailable with an onData or onError handler\n");
    return false;
  } 
  onResponse = handler;
  return true;
}

// getMessageCount: return message counter value
uint32_t ModbusClient::getMessageCount() {
  return messageCount;
}
