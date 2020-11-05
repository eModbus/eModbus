// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_BRIDGE_TEMP_H
#define _MODBUS_BRIDGE_TEMP_H

#include <map>
#include "ModbusClient.h"

// Known server types: local (plain alias), TCP (client, host/port) and RTU (client)
enum ServerType : uint8_t { TCP_SERVER, RTU_SERVER };

template<typename SERVERCLASS>
class ModbusBridge : public SERVERCLASS {
public:
  explicit ModbusBridge(uint32_t TOV = 10000);
  ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV = 10000, int rtsPin = -1);
  ~ModbusBridge();

  // Methods to link external servers to the bridge
  bool attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client);
  bool attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client, IPAddress host, uint16_t port);

protected:

  // ServerData holds all data necessary to address a single server
  struct ServerData {
    uint8_t serverID;
    ModbusClient *client;
    ServerType serverType;
    IPAddress host;
    uint16_t port;


    // RTU constructor
    ServerData(uint8_t sid, ModbusClient *c) :
      serverID(sid),
      client(c),
      serverType(RTU_SERVER),
      host(IPAddress(0, 0, 0, 0)),
      port(0) {}
    
    // TCP constructor
    ServerData(uint8_t sid, ModbusClient *c, IPAddress h, uint16_t p) :
      serverID(sid),
      client(c),
      serverType(TCP_SERVER),
      host(h),
      port(p) {}

  };

  struct ResponseBuf {
    ResponseType data;
    bool ready;

    ResponseBuf() : ready(false) {}
  };

  // Data and Error response handlers
  static void bridgeDataHandler(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token);
  static void bridgeErrorHandler(Error error, uint32_t token);

  // Default worker function
  ResponseType bridgeWorker(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data);

  // Map of servers attached
  std::map<uint8_t, ServerData *> servers;

  // Timeout for sent requests
  uint32_t requestTimeout;

};

// Constructor
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge(uint32_t TOV) :
  SERVERCLASS(),
  requestTimeout(TOV) {
  }

template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV, int rtsPin) :
  SERVERCLASS(serial, timeout, rtsPin),
  requestTimeout(TOV) {
  }

// Destructor
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::~ModbusBridge() { 
  // release storage in servers?
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client) {
  if (servers.find(aliasID) != servers.end()) return false;
  servers[aliasID] = new ServerData(serverID, client);
  client->onDataHandler(&(this->bridgeDataHandler));
  client->onErrorHandler(&(this->bridgeErrorHandler));
  this->registerWorker(aliasID, ANY_FUNCTION_CODE, reinterpret_cast<MBSworker>(&ModbusBridge<SERVERCLASS>::bridgeWorker));
  return true;
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client, IPAddress host, uint16_t port) {
  if (servers.find(aliasID) != servers.end()) return false;
  servers[aliasID] = new ServerData(serverID, static_cast<ModbusClient *>(client), host, port);
  client->onDataHandler(&(this->bridgeDataHandler));
  client->onErrorHandler(&(this->bridgeErrorHandler));
  this->registerWorker(aliasID, ANY_FUNCTION_CODE, &bridgeWorker);
  return true;
}

template<typename SERVERCLASS>
ResponseType ModbusBridge<SERVERCLASS>::bridgeWorker(uint8_t aliasID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  uint32_t startRequest = millis();
  ResponseType response;

  // Find the (alias) serverID
  if (servers.find(aliasID) != servers.end()) {
    // Found it. We may use servers[serverID] now without allocating a new map slot
    ResponseBuf *responseBuffer = new ResponseBuf();
    Error e = servers[aliasID]->client->addRequest(servers[aliasID]->serverID, functionCode, data, dataLen, (uint32_t)responseBuffer);
    if (e != SUCCESS) {
      delete responseBuffer;
      return ModbusServer::ErrorResponse(e);
    } else {
      // Loop until the response has arrived or timeout has struck
      while (!responseBuffer->ready && millis() - startRequest >= requestTimeout) {
        delay(10);
      }
      // Did we get a response?
      if (responseBuffer->ready) {
        // Yes. return it to the requester
        // Size>1?
        if (responseBuffer->data.size() > 1) {
          // Yes, we got a data buffer
          response = ModbusServer::DataResponse(responseBuffer->data.size(), responseBuffer->data.data());
        } else {
          // No, size==1 - error code
          response = ModbusServer::ErrorResponse(static_cast<Error>(responseBuffer->data[0]));
        }
        delete responseBuffer;
        return response;
      } else {
        // No response received - timeout
        delete responseBuffer;
        return ModbusServer::ErrorResponse(TIMEOUT);
      }
    }
  }
  // If we get here, something has gone wrong internally. We send back an error response anyway.
  return ModbusServer::ErrorResponse(INVALID_SERVER);
}

template<typename SERVERCLASS>
void ModbusBridge<SERVERCLASS>::bridgeDataHandler(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token) {
  ResponseBuf *responseBuffer = (ResponseBuf *)token;
  for (uint16_t i = 0; i < length; ++i) {
    responseBuffer->data.push_back(data[i]);
  }
  responseBuffer->ready = true;
}

template<typename SERVERCLASS>
void ModbusBridge<SERVERCLASS>::bridgeErrorHandler(Error error, uint32_t token) {
  ResponseBuf *responseBuffer = (ResponseBuf *)token;
  responseBuffer->data.push_back(error);
  responseBuffer->ready = true;
}

#endif
