// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_BRIDGE_TEMP_H
#define _MODBUS_BRIDGE_TEMP_H

#include <map>
#include "ModbusClient.h"
#include "ModbusServerTCPtemp.h"
#include "ModbusServerRTU.h"

// Known server types: local (plain alias), TCP (client, host/port) and RTU (client)
enum ServerType : uint8_t { TCP_SERVER, RTU_SERVER };

template<typename SERVERCLASS>
class ModbusBridge : public SERVERCLASS {
public:
  ModbusBridge();
  ModbusBridge(HardwareSerial& serial, uint32_t timeout, int rtsPin = -1);
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

  // link client to bridge
  void linkServer(uint8_t aliasID, ModbusClient *c);

  // Data and Error response handlers
  void bridgeDataHandler(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token);
  void bridgeErrorHandler(Error error, uint32_t token);

  // Default worker function
  ResponseType bridgeWorker(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data);

  // Map of servers attached
  std::map<uint8_t, ServerData> servers;

};

// Constructor
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge() :
  SERVERCLASS() {
  }

template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::ModbusBridge(HardwareSerial& serial, uint32_t timeout, int rtsPin) :
  SERVERCLASS(serial, timeout, rtsPin) {
  }

// Destructor
template<typename SERVERCLASS>
ModbusBridge<SERVERCLASS>::~ModbusBridge() { 
  // release storage in servers?
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client) {
  if (servers.find(aliasID) != servers.end()) return false;
  servers[aliasID] = ServerData(serverID, client);
  linkServer(aliasID, client);
  return true;
}

template<typename SERVERCLASS>
bool ModbusBridge<SERVERCLASS>::attachServer(uint8_t aliasID, uint8_t serverID, ModbusClient *client, IPAddress host, uint16_t port) {
  if (servers.find(aliasID) != servers.end()) return false;
  servers[aliasID] = ServerData(serverID, client, host, port);
  linkServer(aliasID, client);
  return true;
}

template<typename SERVERCLASS>
void ModbusBridge<SERVERCLASS>::linkServer(uint8_t aliasID, ModbusClient *c) {
  c->onDataHandler(&bridgeDataHandler);
  c->onErrorHandler(&bridgeErrorHandler);
  registerWorker(aliasID, ANY_FUNCTION_CODE, &bridgeWorker);
}

#endif
