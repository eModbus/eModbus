// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientTCP.h"
#undef LOCAL_LOG_LEVEL
#include "Logging.h"

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
      ModbusMessageTCP *r = requests.front();
      // Delete request
      delete r;
      // Remove front entry
      requests.pop();
    }
  }
  LOG_D("TCP client worker killed.\n");
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
  LOG_D("TCP client worker %s started\n", taskName);
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
  LOG_D("Target set: %d.%d.%d.%d:%d\n", host[0], host[1], host[2], host[3], port);
  if (MT_target.host == MT_lastTarget.host && MT_target.port == MT_lastTarget.port) return false;
  return true;
}

Error ModbusClientTCP::addRequest(uint8_t serverID, uint8_t functionCode, uint8_t *data, uint16_t dataLen, uint32_t token) {
    Error rc = SUCCESS;        // Return value

    // Create request, if valid
    ModbusMessageTCP *r = ModbusMessageTCP::createTCPRequest(rc, MT_target, serverID, functionCode, dataLen, data, token);

    // Add it to the queue, if valid
    if (r) {
      // Queue add successful?
      if (!addToQueue(r)) {
        // No. Return error after deleting the allocated request.
        rc = REQUEST_QUEUE_FULL;
        delete r;
      }
    }

    LOG_D("Add TCP request result: %02X\n", rc);
    return rc;
  }

// addToQueue: send freshly created request to queue
bool ModbusClientTCP::addToQueue(ModbusMessageTCP *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if (requests.size()<MT_qLimit) {
      // inject proper transactionID
      request->tcpHead.transactionID = messageCount++;
      // Yes. Safely lock queue and push request to queue
      rc = true;
      lock_guard<mutex> lockGuard(qLock);
      requests.push(request);
    }
  }

  return rc;
}


// Method to generate an error response - properly enveloped for TCP
ModbusMessageTCP ModbusClientTCP::generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode) {
  ModbusMessageTCP *rv;       // Returned std::vector
  ModbusMessageTCP::TargetHost dummyHost;

  Error rc = ModbusMessageTCP::checkServerFC(serverID, functionCode);
  if (rc != SUCCESS) {
    rv = new ModbusMessageTCP(MMT_RESPONSE, dummyHost, 1);
    rv->push_back(rc);
  } else {
    // 6 bytes TCP header plus serverID, functionCode and error code
    rv = new ModbusMessageTCP(MMT_RESPONSE, dummyHost, 9);

    // Copy in TCP header
    ModbusMessageTCP::ModbusTCPhead head(transactionID, 0, 3);
    rv->add((const uint8_t *)head, 6);

    // Add payload
    rv->add(serverID);
    rv->add(static_cast<uint8_t>(functionCode | 0x80));
    rv->add(errorCode);
  }
  return *rv;
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
      ModbusMessageTCP *request = instance->requests.front();
      doNotPop = false;
      LOG_D("Got request from queue\n");

      // Do we have a connection open?
      if (instance->MT_client.connected()) {
        // check if lastHost/lastPort!=host/port off the queued request
        if (instance->MT_lastTarget.host != request->target.host || instance->MT_lastTarget.port != request->target.port) {
          // It is different. Disconnect it.
          instance->MT_client.stop();
          LOG_D("Target different, disconnect\n");
          delay(1);  // Give scheduler room to breathe
        } else {
          // it is the same host/port.
          // Empty the RX buffer in case there is a stray response left
          if (instance->MT_client.connected()) {
            while (instance->MT_client.available()) { instance->MT_client.read(); }
          }
          // Give it some slack to get ready again
          while (millis() - lastRequest < request->target.interval) { delay(1); }
        }
      }
      // if client is disconnected (we will have to switch hosts)
      if (!instance->MT_client.connected()) {
        // Serial.println("Client reconnecting");
        // It is disconnected. connect to host/port from queue
        instance->MT_client.connect(request->target.host, request->target.port);
        LOG_D("Target connect.\n");

        delay(1);  // Give scheduler room to breathe
      }
      // Are we connected (again)?
      if (instance->MT_client.connected()) {
        LOG_D("Is connected. Send request.\n");
        // Yes. Send the request via IP
        instance->send(request);

        // Get the response - if any
        ModbusMessageTCP *response = instance->receive(request);

        // Did we get a normal response?
        if (response->getError()==SUCCESS) {
          // Yes. Do we have an onData handler registered?
          LOG_D("Data response.\n");
          if (instance->onData) {
            // Yes. call it
            instance->onData(response->getServerID(), response->getFunctionCode(), response->data(), response->size(), request->getToken());
          } else {
            LOG_D("No onData handler\n");
          }
        } else {
          // No, something went wrong. All we have is an error
          if (response->getError() == TIMEOUT && retryCounter--) {
            LOG_D("Retry on timeout...\n");
            doNotPop = true;
          } else {
            // Do we have an onError handler?
            LOG_D("Error response.\n");
            if (instance->onError) {
              // Yes. Forward the error code to it
              instance->onError(response->getError(), request->getToken());
            } else {
              LOG_D("No onError handler\n");
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
          LOG_D("Retry on connect failure...\n");
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
        LOG_D("Request popped from queue.\n");
      }
      lastRequest = millis();
    } else {
      delay(1);  // Give scheduler room to breathe
    }
  }
}

// send: send request via Client connection
void ModbusClientTCP::send(ModbusMessageTCP *request) {
  // We have a established connection here, so we can write right away.
  // Move tcpHead and request into one continuous buffer, since the very first request tends to 
  // take too long to be sent to be recognized.
  uint16_t rl = request->size();
  uint8_t sbuf[rl + 6];
  memcpy(sbuf, (const uint8_t *)request->tcpHead, 6);
  memcpy(sbuf + 6, request->data(), rl);
  MT_client.write(sbuf, rl + 6);
  // Done. Are we?
  MT_client.flush();
  HEXDUMP_V("Request packet", sbuf, rl + 6);
}

// receive: get response via Client connection
ModbusMessageTCP* ModbusClientTCP::receive(ModbusMessageTCP *request) {
  uint32_t lastMillis = millis();     // Timer to check for timeout
  bool hadData = false;               // flag data received
  const uint16_t dataLen(300);          // Modbus Packet supposedly will fit (260<300)
  uint8_t data[dataLen];              // Local buffer to collect received data
  uint16_t dataPtr = 0;               // Pointer into data
  ModbusMessageTCP *response = nullptr;    // Response structure to be returned

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
    LOG_D("Received response.\n");
    HEXDUMP_V("Response packet", data, dataPtr);
    // Yes. check it for validity
    // First transactionID and protocolID shall be identical, length has to match the remainder.
    ModbusMessageTCP::ModbusTCPhead head(request->tcpHead.transactionID, request->tcpHead.protocolID, dataPtr - 6);
    // Matching head?
    if (memcmp((const uint8_t *)head, data, 6)) {
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
      response = new ModbusMessageTCP(MMT_RESPONSE, request->target, dataPtr - 6, request->getToken());
      response->add(data + 6, dataPtr - 6);
      response->tcpHead = head;
    }
  } else {
    // No, timeout must have struck
    response = errorResponse(TIMEOUT, request);
  }
  return response;
}

ModbusMessageTCP* ModbusClientTCP::errorResponse(Error e, ModbusMessageTCP *request) {
  ModbusMessageTCP *errResponse = new ModbusMessageTCP(MMT_RESPONSE, request->target, 3, request->getToken());
  
  errResponse->add(request->getServerID());
  errResponse->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
  errResponse->add(static_cast<uint8_t>(e));
  errResponse->tcpHead = request->tcpHead;
  errResponse->tcpHead.len = 3;

  return errResponse;
}
