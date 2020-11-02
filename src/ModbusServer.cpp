// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
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
      // No, no explicit worker found, but may be there is one for ANY_FUNCTION_CODE?
    } else {
      fcmap = svmap->second.find(ANY_FUNCTION_CODE);;
      // Found it?
      if (fcmap != svmap->second.end()) {
        // Yes. Return the function pointer for it.
        return fcmap->second;
      }
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
uint32_t ModbusServer::getMessageCount() { 
  uint32_t retCnt = 0;
  {
    lock_guard<mutex> cntLock(m);
    retCnt = messageCount;
  }
  return retCnt;
}

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

// LocalRequest: get response from locally running server.
ResponseType ModbusServer::localRequest(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  // Try to get a worker for the request
  MBSworker worker = getWorker(serverID, functionCode);
  // Did we get one?
  if (worker != nullptr) {
    // Yes. call it and return the response
    ResponseType m = worker(serverID, functionCode, dataLen, data);
    // Process Response. Is it one of the predefined types?
    if (m[0] == 0xFF && (m[1] == 0xF0 || m[1] == 0xF1 || m[1] == 0xF2 || m[1] == 0xF3)) {
      // Yes. Check it
      switch (m[1]) {
      case 0xF0: // NIL
        m.clear();
        break;
      case 0xF1: // ECHO
        m.clear();
        m.push_back(serverID);
        m.push_back(functionCode);
        for (uint16_t i = 0; i < dataLen; ++i) {
          m.push_back(data[i]);
        }
        break;
      case 0xF2: // ERROR
        m[0] = serverID;
        m[1] = functionCode | 0x80;
        break;
      case 0xF3: // DATA
        m[0] = serverID;
        m[1] = functionCode;
        break;
      default:   // Will not get here, but lint likes it!
        break;
      }
    }
    return m;
  } else {
    // No. Is there at least one worker for the serverID?
    if (isServerFor(serverID)) {
      // Yes. Respond with "illegal function code"
      return ErrorResponse(ILLEGAL_FUNCTION);
    } else {
      // No. Respond with "Invalid server ID"
      return ErrorResponse(INVALID_SERVER);
    }
  }
  // We should never get here...
  return ErrorResponse(UNDEFINED_ERROR);
}

// Constructor
ModbusServer::ModbusServer() :
  messageCount(0) { }

// Destructor
ModbusServer::~ModbusServer() {
  
}
