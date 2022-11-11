// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_TCP_ASYNC_H
#define _MODBUS_CLIENT_TCP_ASYNC_H
#include <Arduino.h>
#if defined ESP32
#include <AsyncTCP.h>
#elif defined ESP8266
#include <ESPAsyncTCP.h>
#endif
#include "options.h"
#include "ModbusMessage.h"
#include "ModbusClient.h"
#include <queue>
#if USE_MUTEX
#include <mutex>      // NOLINT
#endif
using std::queue;

#define TARGETHOSTINTERVAL 10
#define DEFAULTTIMEOUT 2000

class ModbusClientTCPasync : public ModbusClient {
public:
  // Constructor
  explicit ModbusClientTCPasync(uint16_t queueLimit = 100);

  // Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
  ModbusClientTCPasync(IPAddress host, uint16_t port = 502, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientTCPasync();

  // optionally manually connect to modbus server. Otherwise connection will be made upon first request
  void connect();

  // manually disconnect from modbus server.
  void disconnect(bool force = false);

  // Switch target host (if necessary)
  // Return true, if host/port is different from last host/port used
  bool setTarget(IPAddress host, uint16_t port, uint32_t timeout = DEFAULTTIMEOUT, uint32_t interval = TARGETHOSTINTERVAL);

  // Set timeout value
  void setTimeout(uint32_t timeout = DEFAULTTIMEOUT, uint32_t interval = TARGETHOSTINTERVAL);

protected:

  // class describing a target server
  struct TargetHost {
    IPAddress     host;         // IP address
    uint16_t      port;         // Port number
    uint32_t      timeout;      // Time in ms waiting for a response
    uint32_t      interval;     // Time in ms to wait between requests
    
    inline TargetHost& operator=(TargetHost& t) {
      host = t.host;
      port = t.port;
      timeout = t.timeout;
      interval = t.interval;
      return *this;
    }
    
    inline TargetHost(TargetHost& t) :
      host(t.host),
      port(t.port),
      timeout(t.timeout),
      interval(t.interval) {}
    
    inline TargetHost() :
      host(IPAddress(0, 0, 0, 0)),
      port(0),
      timeout(0),
      interval(0)
    { }

    inline TargetHost(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) :
      host(host),
      port(port),
      timeout(timeout),
      interval(interval)
    { }

    inline bool operator==(TargetHost& t) {
      if (host != t.host) return false;
      if (port != t.port) return false;
      return true;
    }

    inline bool operator!=(TargetHost& t) {
      if (host != t.host) return true;
      if (port != t.port) return true;
      return false;
    }
  };

  // class describing the TCP header of Modbus packets
  class ModbusTCPhead {
  public:
    ModbusTCPhead() :
    transactionID(0),
    protocolID(0),
    len(0) {}

    ModbusTCPhead(uint16_t tid, uint16_t pid, uint16_t _len) :
    transactionID(tid),
    protocolID(pid),
    len(_len) {}

    uint16_t transactionID;     // Caller-defined identification
    uint16_t protocolID;        // const 0x0000
    uint16_t len;               // Length of remainder of TCP packet

    inline explicit operator const uint8_t *() {
      uint8_t *cp = headRoom;
      *cp++ = (transactionID >> 8) & 0xFF;
      *cp++ = transactionID  & 0xFF;
      *cp++ = (protocolID >> 8) & 0xFF;
      *cp++ = protocolID  & 0xFF;
      *cp++ = (len >> 8) & 0xFF;
      *cp++ = len  & 0xFF;
      return headRoom;
    }

    inline ModbusTCPhead& operator= (ModbusTCPhead& t) {
      transactionID = t.transactionID;
      protocolID    = t.protocolID;
      len           = t.len;
      return *this;
    }

  protected:
    uint8_t headRoom[6];        // Buffer to hold MSB-first TCP header
  };

  struct RequestEntry {
    uint32_t token;
    ModbusMessage msg;
    TargetHost target;
    ModbusTCPhead head;
    uint32_t sentTime;
    bool isSyncRequest;
    RequestEntry(uint32_t t, ModbusMessage m, TargetHost tg, bool syncReq = false) :
      token(t),
      msg(m),
      target(tg),
      head(ModbusTCPhead()),
      sentTime(0),
      isSyncRequest(syncReq) {}
  };

  // Base addRequest and syncRequest both must be present
  Error addRequestM(ModbusMessage msg, uint32_t token);
  ModbusMessage syncRequestM(ModbusMessage msg, uint32_t token);

  // addToQueue: send freshly created request to queue
  bool addToQueue(int32_t token, ModbusMessage request, TargetHost target, bool syncReq = false);

  void isInstance() { return; }     // make class instantiable

  // TCP handling code
  void connectUnlocked();
  void disconnectUnlocked(bool force);
  void onConnected();
  void onDisconnected();
  void onACError(AsyncClient* c, int8_t error);
  void onPacket(uint8_t* data, size_t length);
  void onPoll();
  void handleSendingQueue();
  void respond(Error error, RequestEntry* request, ModbusMessage* response);

  queue<RequestEntry *> requests;  // Queue to hold requests to be processed
  #if USE_MUTEX
  std::mutex aoLock;                // Mutex to protect any async operation
  #endif

  AsyncClient MTA_client;           // Async TCP client
  TargetHost MT_lastTarget;         // last used server
  TargetHost MT_target;             // Description of target server
  uint32_t MT_defaultTimeout;       // Standard timeout value taken if no dedicated was set
  uint32_t MT_lastActivity;         // Last time the client sent a request
  uint32_t MT_defaultInterval;      // Standard interval value taken if no dedicated was set
  uint16_t MTA_qLimit;              // Maximum number of requests to accept in queue
  enum {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    BUSY,
    CHANGE_TARGET,
    DISCONNECTING
  } MTA_state;                      // TCP connection state
};

#endif
