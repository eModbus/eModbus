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

#include <Ticker.h>

#define TARGETHOSTINTERVAL 10
#define DEFAULTTIMEOUT 2000
#define POLL_FREQ 500

class ModbusClientTCPasync : public ModbusClient {
public:
  // Constructor takes optional queue limit
  explicit ModbusClientTCPasync(uint16_t queueLimit = 50);

  // Alternative with initial target host
  ModbusClientTCPasync(IPAddress host, uint16_t port, uint16_t queueLimit = 50);

  // Destructor: clean up queue, task etc.
  ~ModbusClientTCPasync();

  // optionally manually connect to modbus server. Otherwise connection will be made upon first request
  void connect();

  // manually disconnect from modbus server. Connection will also auto close after idle time
  void disconnect(bool force = false);

  // Set default timeout value (and interval)
  void setTimeout(uint32_t timeout = DEFAULTTIMEOUT, uint32_t interval = TARGETHOSTINTERVAL);

  // Switch target host (if necessary)
  bool setTarget(IPAddress host, uint16_t port, uint32_t timeout = 0, uint32_t interval = 0);

// Remove all pending request from queue
  void clearQueue();

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

    inline ModbusTCPhead& operator= (const ModbusTCPhead& t) {
      transactionID = t.transactionID;
      protocolID    = t.protocolID;
      len           = t.len;
      return *this;
    }

  protected:
    uint8_t headRoom[6] = {0,0,0,0,0,0};        // Buffer to hold MSB-first TCP header
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
  Error addRequestM(ModbusMessage msg, uint32_t token) override;
  ModbusMessage syncRequestM(ModbusMessage msg, uint32_t token) override;

  // addToQueue: send freshly created request to queue
  bool addToQueue(int32_t token, ModbusMessage request, bool syncReq = false);

  // receive: get response via Client connection
  // TCPResponse* receive(uint8_t* data, size_t length);

  void isInstance() override { return; }     // make class instantiable

  // TCP handling code
  void connectUnlocked();
  void onConnected();
  void onDisconnected();
  void onACError(AsyncClient* c, int8_t error);
  void onPacket(uint8_t* data, size_t length);
  void onPoll();
  static void onPollStatic(void* mta);
  bool getNextRequest();
  void handleCurrentRequest();
  void respond(Error error, ModbusMessage* response);

  std::queue<RequestEntry*> queue;           // Queue to hold requests to be sent
  RequestEntry* currentRequest;             // Holds currently active request
  #if USE_MUTEX
  std::mutex qLock;                         // Mutex to protect queue
  #endif
  #if USE_LOCK
  bool qLock = false;                       // Lock to protect queue
  bool rLock = false;                       // Lock to protect current request
  #endif

  AsyncClient MTA_client;           // Async TCP client
  TargetHost MTA_lastTarget;        // last used server
  TargetHost MTA_target;            // Description of target server
  uint32_t MTA_defaultTimeout;      // Standard timeout value taken if no dedicated was set
  uint32_t MTA_defaultInterval;     // Standard interval value taken if no dedicated was set
  uint16_t MTA_qLimit;              // Maximum number of requests to accept in queue
  uint32_t MTA_lastActivity;        // Last time there was activity (disabled when queues are not empty)
  enum {
    DISCONNECTED,
    CONNECTING,
    RETRY_CONNECT,
    CONNECTED,
    CHANGE_TARGET,
    BUSY,
    DISCONNECTING
  } MTA_state;                      // TCP connection state
  Ticker timer;
};

#endif
