#include "PhysicalInterface.h"

// Default constructor: set the default timeout to 2000ms, zero out all other 
PhysicalInterface::PhysicalInterface(uint32_t TOV) :
  messageCount(0),
  worker(NULL),
  timeOutValue(TOV),
  onData(nullptr),
  onError(nullptr) { }

// setTimeOut: set/change the default interface timeout
bool PhysicalInterface::setTimeOut(uint32_t TOV) {
  timeOutValue = TOV;
  // Return true if we have a timeOutValue != 0
  if (timeOutValue) return true;
  return false;
}

// onDataHandler: register callback for data responses
void PhysicalInterface::onDataHandler(MBOnData *handler) {
  onData = handler;
}

// onErrorHandler: register callback for error responses
void PhysicalInterface::onErrorHandler(MBOnError *handler) {
  onError = handler;
}

// getMessageCount: return message counter value
uint32_t PhysicalInterface::getMessageCount() {
  return messageCount;
}
  