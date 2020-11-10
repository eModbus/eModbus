// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_BRIDGE_TEMP_H
#define _MODBUS_BRIDGE_TEMP_H

#include <map>
#include <functional>
#include "ModbusClient.h"
#include "ModbusClientTCP.h"  // Needed for client.setTarget()

#include "Logging.h"

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

// Known server types: TCP (client, host/port) and RTU (client)
enum ServerType : uint8_t { TCP_SERVER, RTU_SERVER };

// Bridge class template, takes one of ModbusServerRTU, ModbusServerWiFi, ModbusServerEthernet or ModbusServerTCPasync as parameter
template<typename SERVERCLASS>
class ModbusBridge : public SERVERCLASS {
public:
  // Constructor for TCP server variants. TOV is the timeout while waiting for a client to respond
  explicit ModbusBridge(uint32_t TOV = 10000);

  // Constructor for the RTU variant. Parameters as are for ModbusServerRTU, plus TOV (see before)
  ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV = 10000, int rtsPin = -1);

  // Destructor
  ~ModbusBridge();

  // Method to link external servers to the bridge
  bool attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client, IPAddress host = IPAddress(0, 0, 0, 0), uint16_t port = 0);

  // Link another function code to the server
  bool addFunctionCode(uint8_t aliasID, uint8_t functionCode);

  // Block a function code (respond with ILLEGAL_FUNCTION error)
  bool denyFunctionCode(uint8_t aliasID, uint8_t functionCode);

protected:
  // ServerData holds all data necessary to address a single server
  struct ServerData {
    uint8_t serverID;             // External server id
    ModbusClient *client;         // client to be used to request the server
    ServerType serverType;        // TCP_SERVER or RTU_SERVER
    IPAddress host;               // TCP: host IP address, else 0.0.0.0
    uint16_t port;                // TCP: host port number, else 0

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

  // Struct ResponseBuf for transport of response data from bridgeDataHandler and bridgeErrorHandler to bridgeWorker
  struct ResponseBuf {
    ResponseType data;       // The response proper
    bool ready;              // set by the handlers to signal response is ready
    bool isDone;             // set by worker if data is not needed any more and struct can be deleted by the handlers

    // Constructor to init bool values
    ResponseBuf() : ready(false), isDone(false) {}
  };

  // Data and Error response handlers
  static void bridgeDataHandler(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token);
  static void bridgeErrorHandler(Error error, uint32_t token);

  // Default worker functions
  ResponseType bridgeWorker(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data);
  ResponseType bridgeDenyWorker(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data);

  // Map of servers attached
  std::map<uint8_t, ServerData *> servers;

  // Timeout for sent requests
  uint32_t requestTimeout;

};

// Constructor for TCP variants
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge(uint32_t TOV) :
  SERVERCLASS(),
  requestTimeout(TOV) { } 

// Constructor for RTU variant
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV, int rtsPin) :
  SERVERCLASS(serial, timeout, rtsPin),
  requestTimeout(TOV) { }

// Destructor
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::~ModbusBridge() { 
  // Release ServerData storage in servers array
  for (auto itr = servers.begin(); itr != servers.end(); itr++) {
    delete (itr->second);
  }
  servers.clear();
}

// attachServer: memorize the access data for an external server with ID serverID under bridge ID aliasID
template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client, IPAddress host, uint16_t port) {
  // Is there already an entry for the aliasID?
  if (servers.find(aliasID) == servers.end()) {
    // No. Store server data in map.

    // First loop over all servers entries to find if we already know the client
    bool hadIt = false;
    for (auto it = servers.begin(); it != servers.end(); ++it) {
      // Found it?
      if (it->second->client == client) {
        // Yes. stop searching.
        hadIt = true;
        break;
      }
    }

    // Do we have a port number?
    if (port != 0) {
      // Yes. Must be a TCP client
      servers[aliasID] = new ServerData(serverID, static_cast<ModbusClient *>(client), host, port);
      LOG_D("(TCP): %02X->%02X %d.%d.%d.%d:%d\n", aliasID, serverID, host[0], host[1], host[2], host[3], port);
    } else {
      // No - RTU client required
      servers[aliasID] = new ServerData(serverID, static_cast<ModbusClient *>(client));
      LOG_D("(RTU): %02X->%02X\n", aliasID, serverID);
    }

    // Now lock client callbacks, if not already done
    // Did we find the client?
    if (!hadIt) {
      // No. Claim the onError and on Data handlers for the bridge
      bool lockData = client->onDataHandler(&(this->bridgeDataHandler));
      bool lockError = client->onErrorHandler(&(this->bridgeErrorHandler));

      // Check if the claim was successful
      if (!lockData || !lockError) {
        LOG_W("Warning: onError/onData handler claim was not successful.\n");
        return false;
      }
    }
  }

  // Finally register the server/FC combination for the bridgeWorker
  addFunctionCode(aliasID, functionCode);
  return true;
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::addFunctionCode(uint8_t aliasID, uint8_t functionCode) {
  // Is there already an entry for the aliasID?
  if (servers.find(aliasID) != servers.end()) {
    // Yes. Link server to own worker function
    this->registerWorker(aliasID, functionCode, std::bind(&ModbusBridge<SERVERCLASS>::bridgeWorker, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    LOG_D("FC %02X added for server %02X\n", functionCode, aliasID);
  } else {
    LOG_E("Server %d not attached to bridge!\n", aliasID);
    return false;
  }
  return true;
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::denyFunctionCode(uint8_t aliasID, uint8_t functionCode) {
  // Is there already an entry for the aliasID?
  if (servers.find(aliasID) != servers.end()) {
    // Yes. Link server to own worker function
    this->registerWorker(aliasID, functionCode, std::bind(&ModbusBridge<SERVERCLASS>::bridgeDenyWorker, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    LOG_D("FC %02X blocked for server %02X\n", functionCode, aliasID);
  } else {
    LOG_E("Server %d not attached to bridge!\n", aliasID);
    return false;
  }
  return true;
}

// bridgeWorker: default worker function to process bridge requests
template<typename SERVERCLASS>
ResponseType ModbusBridge<SERVERCLASS>::bridgeWorker(uint8_t aliasID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  uint32_t startRequest = millis();
  ResponseType response;

  // Find the (alias) serverID
  if (servers.find(aliasID) != servers.end()) {
    // Found it. We may use servers[aliasID] now without allocating a new map slot
    ResponseBuf *responseBuffer = new ResponseBuf();

    // TCP servers have a target host/port that needs to be set in the client
    if (servers[aliasID]->serverType == TCP_SERVER) {
      reinterpret_cast<ModbusClientTCP *>(servers[aliasID]->client)->setTarget(servers[aliasID]->host, servers[aliasID]->port);
    }

    // Schedule request
    Error e = servers[aliasID]->client->addRequest(servers[aliasID]->serverID, functionCode, data, dataLen, (uint32_t)responseBuffer);
    // If request is formally wrong, return error code
    if (e != SUCCESS) {
      delete responseBuffer;
      return ModbusServer::ErrorResponse(e);
    } else {
      // Loop until the response has arrived or timeout has struck
      while (!responseBuffer->ready && ((millis() - startRequest) < requestTimeout)) {
        delay(10);
      }
      // Did we get a response?
      if (responseBuffer->ready) {
        // Yes. return it to the requester
        // Size>1?
        if (responseBuffer->data.size() > 1) {
          // Yes, we got a data buffer. Return it without the serverID/FC in front
          response = ModbusServer::DataResponse(responseBuffer->data.size() - 2, responseBuffer->data.data() + 2);
        } else {
          // No, size==1 - error code
          response = ModbusServer::ErrorResponse(static_cast<Error>(responseBuffer->data[0]));
        }
        delete responseBuffer;
        LOG_D("Response!\n");
        return response;
      } else {
        // No response received - timeout
        // Signal to the handlers that the response can be discarded
        responseBuffer->isDone = true;
        LOG_D("Timeout!\n");
        return ModbusServer::ErrorResponse(TIMEOUT);
      }
    }
  }
  // If we get here, something has gone wrong internally. We send back an error response anyway.
  return ModbusServer::ErrorResponse(INVALID_SERVER);
}

// bridgeDenyWorker: worker function to block function codes
template<typename SERVERCLASS>
ResponseType ModbusBridge<SERVERCLASS>::bridgeDenyWorker(uint8_t aliasID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  return ModbusServer::ErrorResponse(ILLEGAL_FUNCTION);
}

// bridgeDataHandler: default onData handler for all responses
template<typename SERVERCLASS>
void ModbusBridge<SERVERCLASS>::bridgeDataHandler(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token) {

  // Get the response buffer's address
  ResponseBuf *responseBuffer = (ResponseBuf *)token;

  // Is it already obsolete?
  if (responseBuffer->isDone) {
    // Yes, drop it.
    LOG_D("Data response out of sync, dropped\n");
    delete responseBuffer;
  } else {
    // No, we need it - copy it into the response buffer
    LOG_D("length = %d\n", length);
    for (uint16_t i = 0; i < length; ++i) {
      responseBuffer->data.push_back(data[i]);
    }
    responseBuffer->ready = true;
  }
}

// bridgeErrorHandler: default onError handler for all responses
template<typename SERVERCLASS>
void ModbusBridge<SERVERCLASS>::bridgeErrorHandler(Error error, uint32_t token) {

  // Get the response buffer's address
  ResponseBuf *responseBuffer = (ResponseBuf *)token;

  // Is it already obsolete?
  if (responseBuffer->isDone) {
    // Yes, drop it.
    LOG_D("Error response out of sync, dropped\n");
    delete responseBuffer;
  } else {
    // No, we need it - copy it into the response buffer
    LOG_D("%02X\n", error);
    responseBuffer->data.push_back(error);
    responseBuffer->ready = true;
  }
}

#endif
