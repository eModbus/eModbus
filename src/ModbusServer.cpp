// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusServer.h"

// registerWorker: register a worker function for a certain serverID/FC combination
// If there is one already, it will be overwritten!
void ModbusServer::registerWorker(uint8_t serverID, uint8_t functionCode, MBSworker worker) {
  workerMap[serverID][functionCode] = worker;
}

// getWorker: if a worker function is registered, return its address, nullptr otherwise
MBSworker ModbusServer::getWorker(uint8_t serverID, uint8_t functionCode) {
  // Search the FC map associated with the serverID
  auto svmap = workerMap.find(serverID);
  // Is there one?
  if (svmap != workerMap.end()) {
    // Yes. Now look for the function code in the inner map
    auto fcmap = svmap->second.find(functionCode);;
    // Found it?
    if (fcmap != svmap->second.end()) {
      // Yes. Return the function pointer for it.
      return fcmap->second;
    }
  }
  // No matching function pointer found
  return nullptr;
}

// isServerFor: if any worker function is registered for the given serverID, return true
bool ModbusServer::isServerFor(uint8_t serverID) {
  // Search the FC map for the serverID
  auto svmap = workerMap.find(serverID);
  // Is it there? Then return true
  if (svmap != workerMap.end()) return true;
  // No, serverID was not found. Return false
  return false;
}

// getMessageCount: read number of messages processed
inline uint32_t ModbusServer::getMessageCount() { return messageCount; }

// ErrorResponse: create an error response message from an error code
ResponseType ModbusServer::ErrorResponse(Error errorCode) {
  ResponseType r = { 0xFF, 0xF2, errorCode };
  return r;
}

// DataResponse: create a regular response from given data
ResponseType ModbusServer::DataResponse(uint16_t dataLen, uint8_t *data) {
  ResponseType r = { 0xFF, 0xF3 };

  if (data) {
    while (dataLen--) {
      r.push_back(*data++);
    }
  }
  return r;
}

// Constructor
ModbusServer::ModbusServer() :
  messageCount(0) { }

// Destructor
ModbusServer::~ModbusServer() {
  
}
