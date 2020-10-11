// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_TCP_H
#define _MODBUS_SERVER_TCP_H

#ifdef CLIENTTYPE

#include <Arduino.h>

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

  // activeClients: return number of clients currently employed
  uint16_t activeClients();

  // start: create task with TCP server to accept requests
  bool start(uint16_t port, uint8_t maxClients, uint32_t timeout, int coreID = -1);

  // stop: drop all connections and kill server task
  bool stop();

protected:
  inline void isInstance() { }

  uint8_t numClients;
  TaskHandle_t serverTask;
  uint16_t serverPort;
  uint32_t serverTimeout;

  struct ClientData {
    ClientData() : task(nullptr), client(0), timeout(0), parent(nullptr) {}
    TaskHandle_t task;
    CLIENTTYPE client;
    uint32_t timeout;
    CLASSNAME *parent;
  };
  ClientData *clients;

  // serve: loop function for server task
  static void serve(CLASSNAME *myself);

  // worker: loop function for client tasks
  static void worker(ClientData *myData);

  // receive: read data from TCP
  TCPMessage receive(CLIENTTYPE client, uint32_t timeWait);

  // accept: start a task to receive requests and respond to a given client
  bool accept(CLIENTTYPE client, uint32_t timeout, int coreID = -1);

  // clientAvailable: return true,. if a client slot is currently unused
  inline bool clientAvailable() { return numClients - activeClients() > 0; }
};

#endif   // CLIENTTYPE
#endif
