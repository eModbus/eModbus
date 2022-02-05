// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientRTU.h"

#if HAS_FREERTOS

#undef LOCAL_LOG_LEVEL
// #define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
#include "Logging.h"

// Constructor takes Serial reference and optional DE/RE pin
ModbusClientRTU::ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(serial),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MR_rtsPin(rtsPin),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT),
  MR_useASCII(false) {
    if (MR_rtsPin >= 0) {
      pinMode(MR_rtsPin, OUTPUT);
      MTRSrts = [this](bool level) {
        digitalWrite(MR_rtsPin, level);
      };
      MTRSrts(LOW);
    } else {
      MTRSrts = RTUutils::RTSauto;
    }
}

// Alternative constructor takes Serial reference and RTS callback function
ModbusClientRTU::ModbusClientRTU(HardwareSerial& serial, RTScallback rts, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(serial),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MTRSrts(rts),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT),
  MR_useASCII(false) {
    MR_rtsPin = -1;
    MTRSrts(LOW);
}

// Destructor: clean up queue, task etc.
ModbusClientRTU::~ModbusClientRTU() {
  // Kill worker task and clean up request queue
  end();
}

// begin: start worker task
void ModbusClientRTU::begin(int coreID, uint32_t interval) {
  // Only start worker if HardwareSerial has been initialized!
  if (MR_serial.baudRate()) {
    // Pull down RTS toggle, if necessary
    MTRSrts(LOW);

    // Set minimum interval time
    MR_interval = RTUutils::calculateInterval(MR_serial, interval);

    // Switch serial FIFO buffer copy threshold to 1 byte (normally is 112!)
    RTUutils::UARTinit(MR_serial, 1);

    // Create unique task name
    char taskName[18];
    snprintf(taskName, 18, "Modbus%02XRTU", instanceCounter);
    // Start task to handle the queue
    xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, taskName, 4096, this, 6, &worker, coreID >= 0 ? coreID : NULL);

    LOG_D("Worker task %d started. Interval=%d\n", (uint32_t)worker, MR_interval);
  } else {
    LOG_E("Worker task could not be started! HardwareSerial not initialized?\n");
  }
}

// end: stop worker task
void ModbusClientRTU::end() {
  if (worker) {
    // Clean up queue
    {
      // Safely lock access
      LOCK_GUARD(lockGuard, qLock);
      // Get all queue entries one by one
      while (!requests.empty()) {
        // Remove front entry
        requests.pop();
      }
    }
    // Kill task
    vTaskDelete(worker);
    LOG_D("Worker task %d killed.\n", (uint32_t)worker);
  }
}

// setTimeOut: set/change the default interface timeout
void ModbusClientRTU::setTimeout(uint32_t TOV) {
  MR_timeoutValue = TOV;
  LOG_D("Timeout set to %d\n", TOV);
}

// Toggle protocol to ModbusASCII
void ModbusClientRTU::useModbusASCII(unsigned long timeout) {
  MR_useASCII = true;
  MR_timeoutValue = timeout; // Switch timeout to ASCII's value
  LOG_D("Protocol mode: ASCII\n");
}

// Toggle protocol to ModbusRTU
void ModbusClientRTU::useModbusRTU() {
  MR_useASCII = false;
  LOG_D("Protocol mode: RTU\n");
}

// Inquire protocol mode
bool ModbusClientRTU::isModbusASCII() {
  return MR_useASCII;
}

// Base addRequest taking a preformatted data buffer and length as parameters
Error ModbusClientRTU::addRequestM(ModbusMessage msg, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  LOG_D("request for %02X/%02X\n", msg.getServerID(), msg.getFunctionCode());

  // Add it to the queue, if valid
  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  }

  LOG_D("RC=%02X\n", rc);
  return rc;
}

// Base syncRequest follows the same pattern
ModbusMessage ModbusClientRTU::syncRequestM(ModbusMessage msg, uint32_t token) {
  ModbusMessage response;

  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg, true)) {
      // No. Return error after deleting the allocated request.
      response.setError(msg.getServerID(), msg.getFunctionCode(), REQUEST_QUEUE_FULL);
    } else {
      // Request is queued - wait for the result.
      response = waitSync(msg.getServerID(), msg.getFunctionCode(), token);
    }
  } else {
    response.setError(msg.getServerID(), msg.getFunctionCode(), EMPTY_MESSAGE);
  }
  return response;
}

// addToQueue: send freshly created request to queue
bool ModbusClientRTU::addToQueue(uint32_t token, ModbusMessage request, bool syncReq) {
  bool rc = false;
  // Did we get one?
  if (request) {
    RequestEntry re(token, request, syncReq);
    if (requests.size()<MR_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      LOCK_GUARD(lockGuard, qLock);
      requests.push(re);
    }
    messageCount++;
  }

  LOG_D("RC=%02X\n", rc);
  return rc;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusClientRTU::handleConnection(ModbusClientRTU *instance) {
  // initially clean the serial buffer
  while (instance->MR_serial.available()) instance->MR_serial.read();
  delay(100);

  // Loop forever - or until task is killed
  while (1) {
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      RequestEntry request = instance->requests.front();

      LOG_D("Pulled request from queue\n");

      // Send it via Serial
      RTUutils::send(instance->MR_serial, instance->MR_lastMicros, instance->MR_interval, instance->MTRSrts, request.msg, instance->MR_useASCII);

      LOG_D("Request sent.\n");
      // HEXDUMP_V("Data", request.msg.data(), request.msg.size());

      // Get the response - if any
      ModbusMessage response = RTUutils::receive(instance->MR_serial, instance->MR_timeoutValue, instance->MR_lastMicros, instance->MR_interval, instance->MR_useASCII);

      LOG_D("%s response (%d bytes) received.\n", response.size()>1 ? "Data" : "Error", response.size());
      HEXDUMP_V("Data", response.data(), response.size());

      // No error in receive()?
      if (response.size() > 1) {
        // No. Check message contents
        // Does the serverID match the requested?
        if (request.msg.getServerID() != response.getServerID()) {
          // No. Return error response
          response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), SERVER_ID_MISMATCH);
        // ServerID ok, but does the FC match as well?
        } else if (request.msg.getFunctionCode() != (response.getFunctionCode() & 0x7F)) {
          // No. Return error response
          response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), FC_MISMATCH);
        } 
      } else {
        // No, we got an error code from receive()
        // Return it as error response
        response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), static_cast<Error>(response[0]));
      }

      LOG_D("Response generated.\n");
      HEXDUMP_V("Response packet", response.data(), response.size());

      // Was it a synchronous request?
      if (request.isSyncRequest) {
        // Yes. Put it into the response map
        {
          LOCK_GUARD(sL, instance->syncRespM);
          instance->syncResponse[request.token] = response;
        }
      // No, an async request. Do we have an onResponse handler?
      } else if (instance->onResponse) {
        // Yes. Call it
        instance->onResponse(response, request.token);
      } else {
        // No, but we may have onData or onError handlers
        // Did we get a normal response?
        if (response.getError()==SUCCESS) {
          // Yes. Do we have an onData handler registered?
          if (instance->onData) {
            // Yes. call it
            instance->onData(response, request.token);
          }
        } else {
          // No, something went wrong. All we have is an error
          // Do we have an onError handler?
          if (instance->onError) {
            // Yes. Forward the error code to it
            instance->onError(response.getError(), request.token);
          }
        }
      }
      // Clean-up time. 
      {
        // Safely lock the queue
        LOCK_GUARD(lockGuard, instance->qLock);
        // Remove the front queue entry
        instance->requests.pop();
      }
    } else {
      delay(1);
    }
  }
}

#endif  // HAS_FREERTOS
