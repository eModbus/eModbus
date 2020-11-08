// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_H
#define _MODBUS_SERVER_H
#include <map>
#include <vector>
#include <functional>
#include <mutex>      // NOLINT
#include "ModbusTypeDefs.h"
#include "ModbusError.h"
#include "ModbusMessage.h"

using ResponseType = std::vector<uint8_t>;
using std::mutex;
using std::lock_guard;

// Standard response variants for "no response" and "echo the request"
const ResponseType NIL_RESPONSE = { 0xFF, 0xF0 };
const ResponseType ECHO_RESPONSE = { 0xFF, 0xF1 };

// MBSworker: function signature for worker functions to handle single serverID/functionCode combinations
// typedef ResponseType (*MBSworker) (uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data);
using MBSworker = std::function<ResponseType(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data)>;

class ModbusServer {
public:
  // registerWorker: register a worker function for a certain serverID/FC combination
  // If there is one already, it will be overwritten!
  void registerWorker(uint8_t serverID, uint8_t functionCode, MBSworker worker);

  // getWorker: if a worker function is registered, return its address, nullptr otherwise
  MBSworker getWorker(uint8_t serverID, uint8_t functionCode);

  // isServerFor: if any worker function is registered for the given serverID, return true
  bool isServerFor(uint8_t serverID);

  // getMessageCount: read number of messages processed
  uint32_t getMessageCount();

  // ErrorResponse: create an error response message from an error code
  static ResponseType ErrorResponse(Error errorCode);

  // DataResponse: create a regular response from given data
  static ResponseType DataResponse(uint16_t dataLen, uint8_t *data);

  // Local request to the server
  ResponseType localRequest(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t* data);

  // listServer: print out all server/FC combinations served
  void listServer();

protected:
  // Constructor
  ModbusServer();

  // Destructor
  ~ModbusServer();

  // Virtual function to prevent this class being instantiated
  virtual void isInstance() = 0;

  std::map<uint8_t, std::map<uint8_t, MBSworker>> workerMap;      // map on serverID->functionCode->worker function
  uint32_t messageCount;         // Number of Requests processed
  mutex m;                       // mutex to cover changes to messageCount
};


#endif
