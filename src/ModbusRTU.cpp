#include "ModbusRTU.h"

// Constructor takes Serial reference and optional DE/RE pin
ModbusRTU::ModbusRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit) :
  PhysicalInterface(2000),
  MR_serial(serial),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MR_rtsPin(rtsPin),
  MR_qLimit(queueLimit) {
}

// Destructor: clean up queue, task etc.
ModbusRTU::~ModbusRTU() {
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
}

// begin: start worker task
void ModbusRTU::begin(int coreID) {
  // If rtsPin is >=0, the RS485 adapter needs send/receive toggle
  if (MR_rtsPin >= 0) {
    pinMode(MR_rtsPin, OUTPUT);
    digitalWrite(MR_rtsPin, LOW);
  }

  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, "ModbusRTU", 4096, this, 5, &worker, coreID >= 0 ? coreID : NULL);

  // silent interval is at least 3.5x character time
  // MR_interval = 35000000UL / MR_serial->baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud
  MR_interval = 40000000UL / MR_serial.baudRate();  // 4 * 10 bits * 1000 µs * 1000 ms / baud

  // The following is okay for sending at any baud rate, but problematic at receiving with baud rates above 35000,
  // since the calculated interval will be below 1000µs!
  // f.i. 115200bd ==> interval=304µs
  if (MR_interval < 1000) MR_interval = 1000;  // minimum of 1msec interval
}

// Methods to set up requests
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, token);

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

// 2. one uint16_t parameter (FC 0x18)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, token);

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

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, token);

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

// 4. three uint16_t parameters (FC 0x16)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, p3, token);

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

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfWords, token);

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

// 6. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfBytes, token);

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

// 7. generic constructor for preformatted data ==> count is counting bytes!
Error ModbusRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, count, arrayOfBytes, token);

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

// addToQueue: send freshly created request to queue
bool ModbusRTU::addToQueue(RTURequest *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if(requests.size()<MR_qLimit) {
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
void ModbusRTU::handleConnection(ModbusRTU *instance) {
  // Loop forever - or until task is killed
  while (1) {
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      RTURequest *request = instance->requests.front();
      // Send it via Serial
      instance->send(request);
      // Get the response - if any
      RTUResponse *response = instance->receive(request);
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
    }
    else {
      delay(1);
    }
  }
}

// send: send request via Serial
void ModbusRTU::send(RTURequest *request) {
  while (micros() - MR_lastMicros < MR_interval) delayMicroseconds(1);  // respect _interval
  // Toggle rtsPin, if necessary
  if (MR_rtsPin >= 0) digitalWrite(MR_rtsPin, HIGH);
  MR_serial.write(request->data(), request->len());
  MR_serial.write(request->CRC & 0xFF);
  MR_serial.write((request->CRC >> 8) & 0xFF);
  MR_serial.flush();
  // Toggle rtsPin, if necessary
  if (MR_rtsPin >= 0) digitalWrite(MR_rtsPin, LOW);
  MR_lastMicros = micros();
}

// receive: get response via Serial
RTUResponse* ModbusRTU::receive(RTURequest *request) {
  // Allocate initial buffer size
  const uint16_t BUFBLOCKSIZE(128);
  uint8_t *buffer = new uint8_t[BUFBLOCKSIZE];
  uint8_t bufferBlocks = 1;

  // Index into buffer
  register uint16_t bufferPtr = 0;

  // State machine states
  enum STATES : uint8_t { WAIT_INTERVAL = 0, WAIT_DATA, IN_PACKET, DATA_READ, ERROR_EXIT, FINISHED };
  register STATES state = WAIT_INTERVAL;

  // Timeout tracker
  uint32_t TimeOut = millis();

  // Error code
  Error errorCode = SUCCESS;

  // Return data object
  RTUResponse* response = nullptr;

  while (state != FINISHED) {
    switch (state) {
    // WAIT_INTERVAL: spend the remainder of the bus quiet time waiting
    case WAIT_INTERVAL:
      // Time passed?
      if (micros() - MR_lastMicros >= MR_interval) {
        // Yes, proceed to reading data
        state = WAIT_DATA;
      } else {
        // No, wait a little longer
        delayMicroseconds(1);
      }
      break;
    // WAIT_DATA: await first data byte, but watch timeout
    case WAIT_DATA:
      if (MR_serial.available()) {
        state = IN_PACKET;
        MR_lastMicros = micros();
      } else if (millis() - TimeOut >= timeOutValue) {
        errorCode = TIMEOUT;
        state = ERROR_EXIT;
      }
      delay(1);
      break;
    // IN_PACKET: read data until a gap of at least _interval time passed without another byte arriving
    case IN_PACKET:
      // Data waiting and space left in buffer?
      while (MR_serial.available()) {
        // Yes. Catch the byte
        buffer[bufferPtr++] = MR_serial.read();
        // Buffer full?
        if (bufferPtr >= bufferBlocks * BUFBLOCKSIZE) {
          // Yes. Extend it by another block
          bufferBlocks++;
          uint8_t *temp = new uint8_t[bufferBlocks * BUFBLOCKSIZE];
          memcpy(temp, buffer, (bufferBlocks - 1) * BUFBLOCKSIZE);
          // Use intermediate pointer temp2 to keep cppcheck happy
          delete[] buffer;
          buffer = temp;
        }
        // Rewind timer
        MR_lastMicros = micros();
      }
      // Gap of at least _interval micro seconds passed without data?
      // ***********************************************
      // Important notice!
      // Due to an implementation decision done in the ESP32 Arduino core code,
      // the correct time to detect a gap of _interval µs is not effective, as
      // the core FIFO handling takes much longer than that.
      //
      // Workaround: uncomment the following line to wait for 16ms(!) for the handling to finish:
      if (micros() - MR_lastMicros >= 16000) {
      //
      // Alternate solution: is to modify the uartEnableInterrupt() function in
      // the core implementation file 'esp32-hal-uart.c', to have the line
      //    'uart->dev->conf1.rxfifo_full_thrhd = 1; // 112;'
      // This will change the number of bytes received to trigger the copy interrupt
      // from 112 (as is implemented in the core) to 1, effectively firing the interrupt
      // for any single byte.
      // Then you may uncomment the line below instead:
      // if (micros() - _lastMicros >= _interval) {
      //
        state = DATA_READ;
      }
      break;
    // DATA_READ: successfully gathered some data. Prepare return object.
    case DATA_READ:
      // Allocate response object - without CRC
      response = new RTUResponse(bufferPtr - 2, request);
      // Move gathered data into it
      response->setData(bufferPtr - 2, buffer);
      // Extract CRC value
      response->setCRC(buffer[bufferPtr - 2] | (buffer[bufferPtr - 1] << 8));
      // Check CRC - OK?
      if(response->isValidCRC()) {
        // Yes, move on
        state = FINISHED;
      }
      else {
        // No! Delete received response, set error code and proceed to ERROR_EXIT.
        delete response;
        errorCode = CRC_ERROR;
        state = ERROR_EXIT;
      }
      break;
    // ERROR_EXIT: We had a timeout or CRC error. Prepare error return object
    case ERROR_EXIT:
      response = new RTUResponse(3, request);
      {
        uint8_t *cp = response->data();
        *cp++ = request->getServerID();
        *cp++ = request->getFunctionCode() | 0x80;
        *cp++ = errorCode;
        response->setCRC(RTUCRC::calcCRC(response->data(), 3));
      }
      state = FINISHED;
      break;
    // FINISHED: we are done, keep the compiler happy by pseudo-treating it.
    case FINISHED:
      break;
    }
  }

  // Deallocate buffer
  delete[] buffer;
  MR_lastMicros = micros();

  return response;
}
