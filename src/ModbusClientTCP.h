// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_TCP_H
#define _MODBUS_CLIENT_TCP_H
#include <Arduino.h>
#include "ModbusMessageTCP.h"
#include "ModbusClient.h"
#include "Client.h"
#include <queue>
#include <vector>
#include <mutex>                    // NOLINT

using std::queue;
using std::mutex;
using std::lock_guard;
using TCPMessage = std::vector<uint8_t>;

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
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint8_t *data, uint16_t dataLen, uint32_t token);

  template <typename... Args>
  Error addRequest(Args&&... args) {
    Error rc = SUCCESS;        // Return value

    // Create request, if valid
    TCPRequest *r = TCPRequest::createTCPRequest(rc, this->MT_target, std::forward<Args>(args) ...);

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

    // Was the message generated?
    if (rc != SUCCESS) {
      // No. Return the Error code only - vector size is 1
      rv.reserve(1);
      rv.push_back(rc);
    // If it was successful - did we get a message?
    } else if (r) {
      // Yes, obviously. 
      // Resize the vector to take tcpHead (6 bytes) + message proper
      rv.reserve(r->len() + 6);
      rv.resize(r->len() + 6);
      r->tcpHead.transactionID = transactionID;

      // Do a fast (non-C++-...) copy
      uint8_t *cp = rv.data();
      // Copy in TCP header
      memcpy(cp, (const uint8_t *)r->tcpHead, 6);
      // Copy in message contents
      memcpy(cp + 6, r->data(), r->len());
    }
    
    // Delete request again, if one was created
    if (r) delete r;

    // Move back vector contents
    return rv;
  }

  // Method to generate an error response - properly enveloped for TCP
  TCPMessage generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode);

protected:
  // addToQueue: send freshly created request to queue
  bool addToQueue(TCPRequest *request);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientTCP *instance);

  // send: send request via Client connection
  void send(TCPRequest *request);

  // receive: get response via Client connection
  TCPResponse* receive(TCPRequest *request);

  // Create standard error response 
  TCPResponse* errorResponse(Error e, TCPRequest *request);

  void isInstance() { return; }   // make class instantiable
  queue<TCPRequest *> requests;   // Queue to hold requests to be processed
  mutex qLock;                    // Mutex to protect queue
  Client& MT_client;              // Client reference for Internet connections (EthernetClient or WifiClient)
  TargetHost MT_lastTarget;       // last used server
  TargetHost MT_target;           // Description of target server
  uint32_t MT_defaultTimeout;     // Standard timeout value taken if no dedicated was set
  uint32_t MT_defaultInterval;    // Standard interval value taken if no dedicated was set
  uint16_t MT_qLimit;             // Maximum number of requests to accept in queue
};

#endif
