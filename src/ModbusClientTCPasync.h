// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_TCPASYNC_H
#define _MODBUS_TCPASYNC_H
#include <Arduino.h>
#include <AsyncTCP.h>
#include "ModbusMessageTCP.h"
#include "ModbusClient.h"
#include <list>
#include <vector>
#include <mutex>      // NOLINT

using std::list;
using std::mutex;
using std::lock_guard;
using std::vector;

using TCPMessage = std::vector<uint8_t>;

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

  template <typename... Args>
  Error addRequest(Args&&... args) {
    Error rc = SUCCESS;        // Return value

    // Create request, if valid
    TCPRequest *r = TCPRequest::createTCPRequest(rc, this->MTA_target, std::forward<Args>(args) ...);

    // Add it to the queue, if valid
    if (r) {
      // Queue add successful?
      if (!addToQueue(r)) {
        // No. Return error after deleting the allocated request.
        rc = REQUEST_QUEUE_FULL;
        delete r;
      }
    }

    return rc;
  }

  template <typename... Args>
  TCPMessage generateRequest(uint16_t transactionID, Args&&... args) {
    Error rc = SUCCESS;       // Return code from generating the request
    TCPMessage rv;       // Returned std::vector with the message or error code
    TargetHost dummyHost = { IPAddress(1, 1, 1, 1), 99, 0, 0 };

    // Create request, if valid
    TCPRequest *r = TCPRequest::createTCPRequest(rc, dummyHost, std::forward<Args>(args) ..., 0xDEADDEAD);

    // Put it in the return std::vector
    rv = vectorize(transactionID, r, rc);
    
    // Delete request again, if one was created
    if (r) delete r;

    // Move back vector contents
    return rv;
  }

protected:
// makeHead: helper function to set up a MSB TCP header
  bool makeHead(uint8_t *data, uint16_t dataLen, uint16_t TID, uint16_t PID, uint16_t LEN);

  // addToQueue: send freshly created request to queue
  bool addToQueue(TCPRequest *request);

  // send: send request via Client connection
  bool send(TCPRequest *request);

  // receive: get response via Client connection
  // TCPResponse* receive(uint8_t* data, size_t length);

  // Create standard error response 
  TCPResponse* errorResponse(Error e, TCPRequest *request);

  // Move complete message data including tcpHead into a std::vector
  vector<uint8_t> vectorize(uint16_t transactionID, TCPRequest *request, Error err);

  void isInstance() { return; }     // make class instantiable

  // TCP handling code, all static taking a class instancs as param
  void onConnected();
  void onDisconnected();
  // void onError(int8_t error);
  // void onTimeout(uint32_t time);
  // void onAck(size_t len, uint32_t time);
  void onPacket(uint8_t* data, size_t length);
  void onPoll();
  void handleSendingQueue();

  list<TCPRequest*> txQueue;        // Queue to hold requests to be sent
  list<TCPRequest*> rxQueue;        // Queue to hold requests to be processed
  mutex sLock;                      // Mutex to protect state
  mutex qLock;                      // Mutex to protect queues
  AsyncClient MTA_client;           // Async TCP client
  uint32_t MTA_timeout;             // Standard timeout value taken
  uint32_t MTA_idleTimeout;         // Standard timeout value taken
  uint16_t MTA_qLimit;              // Maximum number of requests to accept in queue
  uint32_t MTA_lastActivity;        // Last time there was activity (disabled when queues are not empty)
  enum {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
  } MTA_state;                      // TCP connection state
  TargetHost MTA_target;            // dummy target
};

#endif
