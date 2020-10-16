// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
 #include "ModbusClientTCP.h"

// Constructor takes reference to Client (EthernetClient or WiFiClient)
ModbusClientTCP::ModbusClientTCP(Client& client, uint16_t queueLimit) :
  ModbusClient(),
  MT_client(client),
  MT_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_target(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_defaultTimeout(DEFAULTTIMEOUT),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MT_qLimit(queueLimit)
  { }

// Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
ModbusClientTCP::ModbusClientTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit) :
  ModbusClient(),
  MT_client(client),
  MT_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_target(host, port, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_defaultTimeout(DEFAULTTIMEOUT),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MT_qLimit(queueLimit)
  { }

// Destructor: clean up queue, task etc.
ModbusClientTCP::~ModbusClientTCP() {
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
void ModbusClientTCP::begin(int coreID) {
  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "Modbus%02XTCP", instanceCounter);
  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, taskName, 4096, this, 5, &worker, coreID >= 0 ? coreID : NULL);
}

// Set default timeout value (and interval)
void ModbusClientTCP::setTimeout(uint32_t timeout, uint32_t interval) {
  MT_defaultTimeout = timeout;
  MT_defaultInterval = interval;
}

// Switch target host (if necessary)
// Return true, if host/port is different from last host/port used
bool ModbusClientTCP::setTarget(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) {
  MT_target.host = host;
  MT_target.port = port;
  MT_target.timeout = timeout ? timeout : MT_defaultTimeout;
  MT_target.interval = interval ? interval : MT_defaultInterval;
  if (MT_target.host == MT_lastTarget.host && MT_target.port == MT_lastTarget.port) return false;
  return true;
}

// addToQueue: send freshly created request to queue
bool ModbusClientTCP::addToQueue(TCPRequest *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if (requests.size()<MT_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      lock_guard<mutex> lockGuard(qLock);
      requests.push(request);
    }
  }

  return rc;
}

// Move complete message data including tcpHead into a std::vector
TCPMessage ModbusClientTCP::vectorize(uint16_t transactionID, TCPRequest *request, Error err) {
  TCPMessage rv;       /// Returned std::vector

  // Was the message generated?
  if (err != SUCCESS) {
    // No. Return the Error code only - vector size is 1
    rv.reserve(1);
    rv.push_back(err);
  // If it was successful - did we get a message?
  } else if (request) {
    // Yes, obviously. 
    // Resize the vector to take tcpHead (6 bytes) + message proper
    rv.reserve(request->len() + 6);
    rv.resize(request->len() + 6);

    // Do a fast (non-C++-...) copy
    uint8_t *cp = rv.data();
    // Copy in TCP header
    makeHead(cp, 6, transactionID, request->tcpHead.protocolID, request->tcpHead.len);
    // Copy in message contents
    memcpy(cp + 6, request->data(), request->len());
  }
  // Bring back the vector
  return rv;
}

// Method to generate an error response - properly enveloped for TCP
TCPMessage ModbusClientTCP::generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode) {
  TCPMessage rv;       // Returned std::vector

  Error rc = TCPRequest::checkServerFC(serverID, functionCode);
  if (rc != SUCCESS) {
    rv.reserve(1);
    rv.push_back(rc);
  } else {
    rv.reserve(9);            // 6 bytes TCP header plus serverID, functionCode and error code
    rv.resize(9);

    // Copy in TCP header
    uint8_t *cp = rv.data();
    makeHead(cp, 6, transactionID, 0, 3);

    // Add payload
    rv[6] = serverID;
    rv[7] = (functionCode | 0x80);
    rv[8] = errorCode;
  }
  return rv;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusClientTCP::handleConnection(ModbusClientTCP *instance) {
  const uint8_t RETRIES(2);
  uint8_t retryCounter = RETRIES;
  bool doNotPop;
  uint32_t lastRequest = millis();

  // Loop forever - or until task is killed
  while (1) {
    // Do we have a request in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      TCPRequest *request = instance->requests.front();
      doNotPop = false;

/*
      Serial.printf("Target: %d.%d.%d.%d/%d Timeout=%d Interval=%d\n", 
        request->target.host[0], 
        request->target.host[1], 
        request->target.host[2], 
        request->target.host[3], 
        request->target.port, 
        request->target.timeout, 
        request->target.interval);
        */

      // Empty the RX buffer - just in case...
      while (instance->MT_client.available()) instance->MT_client.read();

      // check if lastHost/lastPort!=host/port off the queued request
      if (instance->MT_lastTarget.host != request->target.host || instance->MT_lastTarget.port != request->target.port) {
        // It is different. If client is connected, disconnect
        if (instance->MT_client.connected()) {
          // Is connected - cut it
          instance->MT_client.stop();
          delay(1);  // Give scheduler room to breathe
        }
      } else {
        // it is the same host/port. Give it some slack to get ready again
        while (millis() - lastRequest < request->target.interval) {
          delay(1);
        }
      }
      // if client is disconnected (we will have to switch hosts)
      if (!instance->MT_client.connected()) {
        // It is disconnected. connect to host/port from queue
        instance->MT_client.connect(request->target.host, request->target.port);

        delay(1);  // Give scheduler room to breathe
      }
      // Are we connected (again)?
      if (instance->MT_client.connected()) {
        // Yes. Send the request via IP
        instance->send(request);

        // Get the response - if any
        TCPResponse *response = instance->receive(request);

        // Did we get a normal response?
        if (response->getError()==SUCCESS) {
          // Yes. Do we have an onData handler registered?
          if (instance->onData) {
            // Yes. call it
            instance->onData(response->getServerID(), response->getFunctionCode(), response->data(), response->len(), request->getToken());
          }
        } else {
          // No, something went wrong. All we have is an error
          if (response->getError() == TIMEOUT && retryCounter--) {
            // Serial.println("Retry on timeout...");
            doNotPop = true;
          } else {
            // Do we have an onError handler?
            if (instance->onError) {
              // Yes. Forward the error code to it
              instance->onError(response->getError(), request->getToken());
            }
          }
        }
        //   set lastHost/lastPort tp host/port
        instance->MT_lastTarget = request->target;
        delete response;  // object created in receive()
      } else {
        // Oops. Connection failed
        // Retry, if attempts are left or report error.
        if (retryCounter--) {
          instance->MT_client.stop();
          delay(10);
          // Serial.println("Retry on connect failure...");
          doNotPop = true;
        } else {
          // Do we have an onError handler?
          if (instance->onError) {
            // Yes. Forward the error code to it
            instance->onError(IP_CONNECTION_FAILED, request->getToken());
          }
        }
      }
      // Clean-up time. 
      if (!doNotPop)
      {
        // Safely lock the queue
        lock_guard<mutex> lockGuard(instance->qLock);
        // Remove the front queue entry
        instance->requests.pop();
        retryCounter = RETRIES;
        // Delete request
        delete request;   // object created from addRequest()
      }
      lastRequest = millis();
    } else {
      delay(1);  // Give scheduler room to breathe
    }
  }
}

// makeHead: helper function to set up a MSB TCP header
bool ModbusClientTCP::makeHead(uint8_t *data, uint16_t dataLen, uint16_t TID, uint16_t PID, uint16_t LEN) {
  uint16_t headlong = 6;
  uint16_t offs = 0;
  uint16_t ptr = 0;

  if (dataLen < headlong) return false;   // Will not fit
  if (data == nullptr) return false;      // No data allocated?

  offs = addValue(data + ptr, headlong, TID);
  headlong -= offs;
  ptr += offs;
  offs = addValue(data + ptr, headlong, PID);
  headlong -= offs;
  ptr += offs;
  offs = addValue(data + ptr, headlong, LEN);
  headlong -= offs;
  // headlong should be 0 here!
  if (headlong) return false;
  return true;
}

// send: send request via Client connection
void ModbusClientTCP::send(TCPRequest *request) {
  // We have a established connection here, so we can write right away.
  // Wait...: tcpHead is not yet in MSB order:
  uint8_t head[6];
  if (makeHead(head, 6, request->tcpHead.transactionID, request->tcpHead.protocolID, request->tcpHead.len)) {
    // Write TCP header first
    MT_client.write(head, 6);
    // Request comes next
    MT_client.write(request->data(), request->len());
    // Done. Are we?
    MT_client.flush();
  }
  // else????
}

// receive: get response via Client connection
TCPResponse* ModbusClientTCP::receive(TCPRequest *request) {
  uint32_t lastMillis = millis();     // Timer to check for timeout
  bool hadData = false;               // flag data received
  const uint16_t dataLen(300);          // Modbus Packet supposedly will fit (260<300)
  uint8_t data[dataLen];              // Local buffer to collect received data
  uint16_t dataPtr = 0;               // Pointer into data
  TCPResponse *response = nullptr;    // Response structure to be returned

  // wait for packet data, overflow or timeout
  while (millis() - lastMillis < request->target.timeout && dataPtr < dataLen && !hadData) {
    // Is there data waiting?
    if (MT_client.available()) {
      // Yes. catch as much as is there and fits into buffer
      while (MT_client.available() && dataPtr < dataLen) {
        data[dataPtr++] = MT_client.read();
      }
      // Register data received
      hadData = true;
      // Rewind EOT and timeout timers
      lastMillis = millis();
    }
    delay(1); // Give scheduler room to breathe
  }
  // Did we get some data?
  if (hadData) {
    // Yes. check it for validity
    uint8_t head[6];
    makeHead(head, 6, request->tcpHead.transactionID, request->tcpHead.protocolID, dataPtr - 6);
    // First transactionID and protocolID shall be identical, length has to match the remainder. Are they?
    if (memcmp(head, data, 6)) {
      // No. return Error response
      response = errorResponse(TCP_HEAD_MISMATCH, request);
      // If the server id does not match that of the request, report error
    } else if (data[6] != request->getServerID()) {
      response = errorResponse(SERVER_ID_MISMATCH, request);
      // If the function code does not match that of the request, report error
    } else if ((data[7] & 0x7F) != request->getFunctionCode()) {
      response = errorResponse(FC_MISMATCH, request);
    } else {
      // Looks good.
      response = new TCPResponse(dataPtr - 6);
      response->add(dataPtr - 6, data + 6);
      response->tcpHead.transactionID = request->tcpHead.transactionID;
      response->tcpHead.protocolID = request->tcpHead.protocolID;
      response->tcpHead.len = dataPtr - 6;
    }
  } else {
    // No, timeout must have struck
    response = errorResponse(TIMEOUT, request);
  }
  return response;
}

TCPResponse* ModbusClientTCP::errorResponse(Error e, TCPRequest *request) {
  TCPResponse *errResponse = new TCPResponse(3);
  
  errResponse->add(request->getServerID());
  errResponse->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
  errResponse->add(static_cast<uint8_t>(e));
  errResponse->tcpHead.transactionID = request->tcpHead.transactionID;
  errResponse->tcpHead.protocolID = request->tcpHead.protocolID;
  errResponse->tcpHead.len = 3;

  return errResponse;
}
