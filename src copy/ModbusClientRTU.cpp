// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientRTU.h"
#undef LOCAL_LOG_LEVEL
#include "Logging.h"

// Constructor takes Serial reference and optional DE/RE pin
ModbusClientRTU::ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(serial),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MR_rtsPin(rtsPin),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT) {
}

// Destructor: clean up queue, task etc.
ModbusClientRTU::~ModbusClientRTU() {
  // Clean up queue
  {
    // Safely lock access
    lock_guard<mutex> lockGuard(qLock);
    // Get all queue entries one by one
    while (!requests.empty()) {
      // Pull front entry
      RTURequest *r = requests.front();
      // Delete request
      delete r;
      // Remove front entry
      requests.pop();
    }
  }
  // Kill task
  vTaskDelete(worker);
  LOG_D("Worker task %d killed.\n", (uint32_t)worker);
}

// begin: start worker task
void ModbusClientRTU::begin(int coreID) {
  // If rtsPin is >=0, the RS485 adapter needs send/receive toggle
  if (MR_rtsPin >= 0) {
    pinMode(MR_rtsPin, OUTPUT);
    digitalWrite(MR_rtsPin, LOW);
  }

  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "Modbus%02XRTU", instanceCounter);
  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, taskName, 4096, this, 6, &worker, coreID >= 0 ? coreID : NULL);

  // silent interval is at least 3.5x character time
  MR_interval = 35000000UL / MR_serial.baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud

  LOG_D("Worker task %d started. Interval=%d\n", (uint32_t)worker, MR_interval);

}

// setTimeOut: set/change the default interface timeout
void ModbusClientRTU::setTimeout(uint32_t TOV) {
  MR_timeoutValue = TOV;
  LOG_D("Timeout set to %d\n", TOV);
}

// Base addRequest taking a preformatted data buffer and length as parameters
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint8_t *data, uint16_t dataLen, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  LOG_D("request for %02X/%02X\n", serverID, functionCode);
  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, dataLen, data, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  LOG_D("RC=%02X\n", rc);
  return rc;
}

// addToQueue: send freshly created request to queue
bool ModbusClientRTU::addToQueue(RTURequest *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if (requests.size()<MR_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      lock_guard<mutex> lockGuard(qLock);
      requests.push(request);
    }
    messageCount++;
  }

  LOG_D("RC=%02X\n", rc);
  return rc;
}

// Method to generate an error response - properly ende by CRC
RTUMessage ModbusClientRTU::generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode) {
  RTUMessage rv;       // Returned std::vector

  Error rc = RTURequest::checkServerFC(serverID, functionCode);

  if (rc != SUCCESS) {
    rv.reserve(1);
    rv.push_back(rc);
  } else {
    rv.reserve(5);            // 6 bytes TCP header plus serverID, functionCode and error code
    rv.resize(5);

    // Copy in TCP header
    uint8_t *cp = rv.data();

    // Write payload
    *cp++ = serverID;
    *cp++ = (functionCode | 0x80);
    *cp++ = errorCode;
    
    // Calculate CRC16 and add it in
    uint16_t crc = RTUutils::calcCRC(rv.data(), 3);
    *cp++ = (crc & 0xFF);
    *cp++ = ((crc >> 8) & 0xFF);
  }
  return rv;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusClientRTU::handleConnection(ModbusClientRTU *instance) {
  // initially clean the serial buffer
  while (instance->MR_serial.available()) instance->MR_serial.read();

  // Loop forever - or until task is killed
  while (1) {
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      RTURequest *request = instance->requests.front();

      LOG_D("Pulled request from queue\n");

      // Send it via Serial
      RTUutils::send(instance->MR_serial, instance->MR_lastMicros, instance->MR_interval, instance->MR_rtsPin, request->data(), request->len());

      LOG_D("Request sent.\n");

      // Get the response - if any
      RTUResponse *response;
      RTUMessage rv = RTUutils::receive(instance->MR_serial, instance->MR_timeoutValue, instance->MR_lastMicros, instance->MR_interval);

      LOG_D("%s response received.\n", rv.size()>1 ? "Data" : "Error");

      // No error in receive()?
      if (rv.size() > 1) {
        // No. Check message contents
        // Does the serverID match the requested?
        if (request->getServerID() != rv[0]) {
          // No. Return error response
          response = new RTUResponse(3);
          response->add(request->getServerID());
          response->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
          response->add(SERVER_ID_MISMATCH);
        // ServerID ok, but does the FC match as well?
        } else if (request->getFunctionCode() != (rv[1] & 0x7F)) {
          // No. Return error response
          response = new RTUResponse(3);
          response->add(request->getServerID());
          response->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
          response->add(FC_MISMATCH);
        // Both serverID and FC are ok - how about the CRC?
        } else if (!RTUutils::validCRC(rv.data(), rv.size())) {
          // CRC faulty - return error
          response = new RTUResponse(3);
          response->add(request->getServerID());
          response->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
          response->add(CRC_ERROR);
        // Everything seems okay
        } else {
          // Build response from received message
          response = new RTUResponse(rv.size() - 2);
          response->add(rv.data(), rv.size() - 2);
          response->setCRC(rv[rv.size() - 2] | (rv[rv.size() - 1] << 8));
        }
      } else {
        // No, we got an error code from receive()
        // Return it as error response
        response = new RTUResponse(3);
        response->add(request->getServerID());
        response->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
        response->add(rv[0]);
      }

      LOG_D("Response generated.\n");
      HEXDUMP_V("Response packet", response->data(), response->len());

      // Did we get a normal response?
      if (response->getError()==SUCCESS) {
        // Yes. Do we have an onData handler registered?
        if (instance->onData) {
          // Yes. call it
          instance->onData(response->getServerID(), response->getFunctionCode(), response->data(), response->len(), request->getToken());
        }
      } else {
        // No, something went wrong. All we have is an error
        // Do we have an onError handler?
        if (instance->onError) {
          // Yes. Forward the error code to it
          instance->onError(response->getError(), request->getToken());
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
      delete response;  // object created in receive()
    } else {
      delay(1);
    }
  }
}
