#include "PhysicalInterface.h"

uint16_t PhysicalInterface::instanceCounter = 0;

// Default constructor: set the default timeout to 2000ms, zero out all other 
PhysicalInterface::PhysicalInterface() :
  messageCount(0),
  worker(NULL),
  onData(nullptr),
  onError(nullptr) { instanceCounter++; }

// onDataHandler: register callback for data responses
void PhysicalInterface::onDataHandler(MBOnData handler) {
  onData = handler;
}

// onErrorHandler: register callback for error responses
void PhysicalInterface::onErrorHandler(MBOnError handler) {
  onError = handler;
}

// getMessageCount: return message counter value
uint32_t PhysicalInterface::getMessageCount() {
  return messageCount;
}
  