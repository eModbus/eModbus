// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_TCP_H
#define _MODBUS_CLIENT_TCP_H
#include <Arduino.h>
#include "ModbusClient.h"
#include "Client.h"
#include <queue>
#include <vector>
#include <mutex>                    // NOLINT

using std::queue;
using std::mutex;
using std::lock_guard;

#define TARGETHOSTINTERVAL 10
#define DEFAULTTIMEOUT 2000

class ModbusClientTCP : public ModbusClient {
public:
  // Constructor takes reference to Client (EthernetClient or WiFiClient)
  explicit ModbusClientTCP(Client& client, uint16_t queueLimit = 100);

  // Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
  ModbusClientTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientTCP();

  // begin: start worker task
  void begin(int coreID = -1);

  // Set default timeout value (and interval)
  void setTimeout(uint32_t timeout = DEFAULTTIMEOUT, uint32_t interval = TARGETHOSTINTERVAL);

  // Switch target host (if necessary)
  bool setTarget(IPAddress host, uint16_t port, uint32_t timeout = 0, uint32_t interval = 0);

  // Base addRequest must be present
  Error addRequest(ModbusMessage msg, uint32_t token);

  // Alternate form with a target host
  Error addRequest(ModbusMessage msg, uint32_t token, IPAddress targetHost, uint16_t targetPort);

// Template variant for last set target host
template <typename... Args>
Error addRequest(uint32_t token, Args&&... args) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  ModbusMessage m;
  rc = m.setMessage(std::forward<Args>(args) ...);

  // Add it to the queue, if valid
  if (rc == SUCCESS) {
    // Queue add successful?
    if (!addToQueue(token, m, MT_target)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  }
  return rc;
}

// Template variant for new target host
template <typename... Args>
Error addRequest(uint32_t token, IPAddress host, uint16_t port, Args&&... args) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  ModbusMessage m;
  rc = m.setMessage(std::forward<Args>(args) ...);

  // Add it to the queue, if valid
  if (rc == SUCCESS) {
    // Queue add successful?
    TargetHost th(host, port, MT_defaultTimeout, MT_defaultInterval);
    if (!addToQueue(token, m, th)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  }
  return rc;
}

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
      addValue(headRoom, 6, transactionID);
      addValue(headRoom + 2, 4, protocolID);
      addValue(headRoom + 4, 2, len);
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
    RequestEntry(uint32_t t, ModbusMessage m, TargetHost tg) :
      token(t),
      msg(m),
      target(tg),
      head(ModbusTCPhead()) {}
  };

  // addToQueue: send freshly created request to queue
  bool addToQueue(uint32_t token, ModbusMessage request, TargetHost target);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientTCP *instance);

  // send: send request via Client connection
  void send(RequestEntry *request);

  // receive: get response via Client connection
  ModbusMessage receive(RequestEntry *request);

  void isInstance() { return; }   // make class instantiable
  queue<RequestEntry *> requests;   // Queue to hold requests to be processed
  mutex qLock;                    // Mutex to protect queue
  Client& MT_client;              // Client reference for Internet connections (EthernetClient or WifiClient)
  TargetHost MT_lastTarget;       // last used server
  TargetHost MT_target;           // Description of target server
  uint32_t MT_defaultTimeout;     // Standard timeout value taken if no dedicated was set
  uint32_t MT_defaultInterval;    // Standard interval value taken if no dedicated was set
  uint16_t MT_qLimit;             // Maximum number of requests to accept in queue
};

#endif
