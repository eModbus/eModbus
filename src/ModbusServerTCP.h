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
  CLASSNAME();

  // Destructor: closes the connections
  ~CLASSNAME();

  // accept: start a task to receive requests and respond to a given client
  bool accept(CLIENTTYPE client, uint32_t timeout, int coreID = -1);

  // removeClient: drop client entry
  void removeClient(TaskHandle_t task);

  // activeClients: return number of clients currently employed
  inline uint16_t activeClients() { return clients.size(); }

protected:
  inline void isInstance() { }
  struct ClientData {
    TaskHandle_t task;
    CLIENTTYPE client;
    uint32_t timeout;
    CLASSNAME *parent;
  };
  vector<ClientData> clients;
  static uint8_t clientCounter;
  static void worker(ClientData *myData);
  TCPMessage receive(CLIENTTYPE client, uint32_t timeWait);
};

#endif   // CLIENTTYPE
#endif
