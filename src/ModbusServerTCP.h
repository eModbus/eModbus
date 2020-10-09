// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_TCP_H
#define _MODBUS_SERVER_TCP_H

#ifdef CLIENTTYPE

#include <Arduino.h>
#include <vector>

#include "ModbusServer.h"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

using std::vector;
using TCPMessage = std::vector<uint8_t>;

class CLASSNAME : public ModbusServer {
public:
  // Constructor
  CLASSNAME(uint8_t maxClients);

  // Destructor: closes the connections
  ~CLASSNAME();

  // accept: start a task to receive requests and respond to a given client
  bool accept(CLIENTTYPE client, uint32_t timeout, int coreID = -1);

  // activeClients: return number of clients currently employed
  uint16_t activeClients();

  // clientAvailable: return true,. if a client slot is currently unused
  inline bool clientAvailable() { return numClients - activeClients() > 0; }

protected:
  inline void isInstance() { }
  uint8_t numClients;
  struct ClientData {
    TaskHandle_t task;
    CLIENTTYPE client;
    uint32_t timeout;
    CLASSNAME *parent;
  };
  ClientData *clients;
  static void worker(ClientData *myData);
  TCPMessage receive(CLIENTTYPE client, uint32_t timeWait);
};

#endif   // CLIENTTYPE
#endif
