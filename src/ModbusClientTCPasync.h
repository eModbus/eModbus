// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_TCP_ASYNC_H
#define _MODBUS_CLIENT_TCP_ASYNC_H
#include <Arduino.h>
#include <AsyncTCP.h>
#include "ModbusMessage.h"
#include "ModbusClient.h"
#include <list>
#include <map>
#include <vector>
#include <mutex>      // NOLINT

using std::mutex;
using std::lock_guard;
using std::vector;

#define DEFAULTTIMEOUT 10000
#define DEFAULTIDLETIME 60000

class ModbusClientTCPasync : public ModbusClient {
public:
  // Constructor takes address and port
  explicit ModbusClientTCPasync(IPAddress address, uint16_t port = 502, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientTCPasync();

  // optionally manually connect to modbus server. Otherwise connection will be made upon first request
  void connect();

  // manually disconnect from modbus server. Connection will also auto close after idle time
  void disconnect(bool force = false);

  // Set timeout value
  void setTimeout(uint32_t timeout);

  // Set idle timeout value (time before connection auto closes after being idle)
  void setIdleTimeout(uint32_t timeout);

  // Set maximum amount of messages awaiting a response. Subsequent messages will be queued.
  void setMaxInflightRequests(uint32_t maxInflightRequests);

  // Base addRequest must be present
  Error addRequest(ModbusMessage msg, uint32_t token);

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
      if (!addToQueue(token, m)) {
        // No. Return error after deleting the allocated request.
        rc = REQUEST_QUEUE_FULL;
      }
    }
    return rc;
  }

protected:

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
    ModbusTCPhead head;
    uint32_t timeout;
    RequestEntry(uint32_t t, ModbusMessage m) :
      token(t),
      msg(m),
      head(ModbusTCPhead()),
      timeout(0) {}
  };


  // addToQueue: send freshly created request to queue
  bool addToQueue(int32_t token, ModbusMessage request);

  // send: send request via Client connection
  bool send(RequestEntry *request);

  // receive: get response via Client connection
  // TCPResponse* receive(uint8_t* data, size_t length);

  void isInstance() { return; }     // make class instantiable

  // TCP handling code, all static taking a class instancs as param
  void onConnected();
  void onDisconnected();
  void onACError(AsyncClient* c, int8_t error);
  // void onTimeout(uint32_t time);
  // void onAck(size_t len, uint32_t time);
  void onPacket(uint8_t* data, size_t length);
  void onPoll();
  void handleSendingQueue();

  std::list<RequestEntry*> txQueue;           // Queue to hold requests to be sent
  std::map<uint16_t, RequestEntry*> rxQueue;  // Queue to hold requests to be processed
  mutex sLock;                         // Mutex to protect state
  mutex qLock;                         // Mutex to protect queues

  AsyncClient MTA_client;           // Async TCP client
  uint32_t MTA_timeout;             // Standard timeout value taken
  uint32_t MTA_idleTimeout;         // Standard timeout value taken
  uint16_t MTA_qLimit;              // Maximum number of requests to accept in queue
  uint32_t MTA_maxInflightRequests; // Maximum number of inflight requests
  uint32_t MTA_lastActivity;        // Last time there was activity (disabled when queues are not empty)
  enum {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
  } MTA_state;                      // TCP connection state
  IPAddress MTA_host;
  uint16_t MTA_port;
};

#endif
