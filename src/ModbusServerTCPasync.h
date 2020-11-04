// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_TCP_ASYNC_H
#define _MODBUS_SERVER_TCP_ASYNC_H

#include <list>
#include <queue>
#include <mutex>
#include <vector>

#include <Arduino.h>  // for millis()

#include <AsyncTCP.h>

#include "ModbusServer.h"
#include "Logging.h"

using std::lock_guard;
using TCPMessage = std::vector<uint8_t>;

class ModbusServerTCPasync : public ModbusServer {

 private:
  class mb_client {
   friend class ModbusServerTCPasync;
   
   public:
    mb_client(ModbusServerTCPasync* s, AsyncClient* c);
    ~mb_client();

   private:
    void onData(uint8_t* data, size_t len);
    void onPoll();
    void onDisconnect();
    void generateResponse(Modbus::Error e, ResponseType* data);  // only to be called if request is complete
    void addResponseToOutbox();
    void handleOutbox();
    ModbusServerTCPasync* server;
    AsyncClient* client;
    uint32_t lastActiveTime;
    TCPMessage currentRequest;
    size_t requestLength;
    ResponseType* currentResponse;
    Modbus::Error error;
    std::queue<ResponseType*> outbox;
    std::mutex m;
    enum {
      RCV1,
      VAL1,
      RCV2,
      VAL2
    } state;
  };


 public:
  // Constructor
  ModbusServerTCPasync();

  // Destructor: closes the connections
  ~ModbusServerTCPasync();

  // activeClients: return number of clients currently employed
  uint16_t activeClients();

  // start: create task with TCP server to accept requests
  bool start(uint16_t port, uint8_t maxClients, uint32_t timeout, int coreID = -1);

  // stop: drop all connections and kill server task
  bool stop();

 protected:
  inline void isInstance() { }
  void onClientConnect(AsyncClient* client);
  void onClientDisconnect(mb_client* client);

  AsyncServer* server;
  std::list<mb_client*> clients;
  uint8_t maxNoClients;
  uint32_t idle_timeout;
  std::mutex m;
};

#endif