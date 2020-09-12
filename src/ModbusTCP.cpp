#include "ModbusTCP.h"

// Constructor takes reference to Client (EthernetClient or WiFiClient)
ModbusTCP::ModbusTCP(Client& client, uint16_t queueLimit) :
  PhysicalInterface(2000),
  MT_client(client),
  MT_lastHost(IPAddress(0, 0, 0, 0)),
  MT_lastPort(0),
  MT_targetHost(IPAddress(0, 0, 0, 0)),
  MT_targetPort(0),
  MT_qLimit(queueLimit) { }

// Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
ModbusTCP::ModbusTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit) :
  PhysicalInterface(2000),
  MT_client(client),
  MT_lastHost(IPAddress(0, 0, 0, 0)),
  MT_lastPort(0),
  MT_targetHost(host),
  MT_targetPort(port),
  MT_qLimit(queueLimit) { }

// Destructor: clean up queue, task etc.
ModbusTCP::~ModbusTCP() {
  // Clean up queue
  {
    // Safely lock access
    lock_guard<mutex> lockGuard(qLock);
    // Get all queue entries one by one
    while (!requests.empty()) {
      // Pull front entry
      TCPRequest *r = requests.front();
      // Delete request
      delete r;
      // Remove front entry
      requests.pop();
    }
  }
  // Kill task
  vTaskDelete(worker);
}

// begin: start worker task
void ModbusTCP::begin(int coreID) {
  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, "ModbusTCP", 4096, this, 5, &worker, coreID >= 0 ? coreID : NULL);
}

// Switch target host (if necessary)
// Return true, if host/port is different from last host/port used
bool ModbusTCP::setTarget(IPAddress host, uint16_t port) {
  MT_targetHost = host;
  MT_targetPort = port;
  if (MT_targetHost == MT_lastHost && MT_targetPort == MT_lastPort) return false;
  return true;
}

// Methods to set up requests
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}
  
// 2. one uint16_t parameter (FC 0x18)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, p1, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}
  
// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, p1, p2, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

// 4. three uint16_t parameters (FC 0x16)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, p1, p2, p3, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}
  
// 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, p1, p2, count, arrayOfWords, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}
  
// 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, p1, p2, count, arrayOfBytes, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
Error ModbusTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  TCPRequest *r = TCPRequest::createTCPRequest(rc, MT_targetHost, MT_targetPort, serverID, functionCode, count, arrayOfBytes, token);

  // Add it to the queue, if valid
  if (r) {
    r->tcpHead.transactionID = messageCount++;
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

// addToQueue: send freshly created request to queue
bool ModbusTCP::addToQueue(TCPRequest *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if(requests.size()<MT_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      lock_guard<mutex> lockGuard(qLock);
      requests.push(request);
    }
  }

  return rc;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusTCP::handleConnection(ModbusTCP *instance) {
  // Loop forever - or until task is killed
  while (1) {
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      TCPRequest *request = instance->requests.front();
      // check if lastHost/lastPort!=host/port off the queued request
      if (instance->MT_lastHost != request->targetHost || instance->MT_lastPort != request->targetPort) {
        // It is different. If client is connected, disconnect
        if (instance->MT_client.connected()) {
          // Is connected - cut it
          instance->MT_client.stop();
          delay(1);  // Give scheduler room to breathe
        }
      }
      // if client is disconnected (we will have to switch hosts)
      if (!instance->MT_client.connected()) {
        // It is disconnected. connect to host/port from queue
        instance->MT_client.connect(request->targetHost, request->targetPort);
        delay(1);  // Give scheduler room to breathe
      }
      // Are we connected (again)?
      if (instance->MT_client.connected()) {
        // Yes. Send the request via IP
        instance->send(instance->MT_client, request);
        // Get the response - if any
        TCPResponse *response = instance->receive(request);
        // Did we get a normal response?
        if (response->getError()==SUCCESS) {
          // Yes. Do we have an onData handler registered?
          if(instance->onData) {
            // Yes. call it
            instance->onData(response->getServerID(), response->getFunctionCode(), response->data(), response->len(), request->getToken());
          }
        }
        else {
          // No, something went wrong. All we have is an error
          // Do we have an onError handler?
          if(instance->onError) {
            // Yes. Forward the error code to it
            instance->onError(response->getError(), request->getToken());
          }
        }
        //   set lastHost/lastPort tp host/port
        instance->MT_lastHost = request->targetHost;
        instance->MT_lastPort = request->targetPort;
        delete response;  // object created in receive()
      }
      else {
        // Oops. Connection failed - report error.
        // Do we have an onError handler?
        if(instance->onError) {
          // Yes. Forward the error code to it
          instance->onError(IP_CONNECTION_FAILED, request->getToken());
        }
      }
      // Clean-up time. 
      {
        // Safely lock the queue
        lock_guard<mutex> lockGuard(instance->qLock);
        // Remove the front queue entry
        instance->requests.pop();
      }
      // Delete RTURequest and RTUResponse objects
      delete request;   // object created from addRequest()
    }
    else {
      delay(1);  // Give scheduler room to breathe
    }
  }
}

// send: send request via Client connection
void ModbusTCP::send(Client& client, TCPRequest *request) {
  // We have a established connection here, so we can write right away.
  // Wait...: tcpHead is not yet in MSB order:
  uint16_t headlong = 6;
  uint8_t head[headlong];
  headlong -= ModbusMessage::addValue(head, headlong, request->tcpHead.transactionID);
  headlong -= ModbusMessage::addValue(head, headlong, request->tcpHead.protocolID);
  headlong -= ModbusMessage::addValue(head, headlong, request->tcpHead.len);
  // headlong should be 0 here!
  // Write TCP header first
  client.write(head, 6);
  // Request comes next
  client.write(request->data(), request->len());
  // Done. Are we?
  client.flush();
}

// receive: get response via Client connection
TCPResponse* ModbusTCP::receive(TCPRequest *request) {

}
