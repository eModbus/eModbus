// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusServerRTU.h"

uint8_t ModbusServerRTU::instanceCounter = 0;

// Constructor
ModbusServerRTU::ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, int rtsPin) :
  ModbusServer(),
  serverTask(nullptr),
  serverTimeout(20000),
  MSRserial(serial),
  MSRinterval(2000),
  MSRlastMicros(0),
  MSRrtsPin(rtsPin) {
  instanceCounter++;
  if (MSRrtsPin >= 0) {
    pinMode(MSRrtsPin, OUTPUT);
    digitalWrite(MSRrtsPin, LOW);
  }
}

// Destructor
ModbusServerRTU::~ModbusServerRTU() {
}

// start: create task with RTU server
bool ModbusServerRTU::start(int coreID) {
  // Task already running?
  if (serverTask != nullptr) {
    // Yes. stop it first
    stop();
  }

  // silent interval is at least 3.5x character time
  // MSRinterval = 35000000UL / MSRserial->baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud
  MSRinterval = 40000000UL / MSRserial.baudRate();  // 4 * 10 bits * 1000 µs * 1000 ms / baud

  // The following is okay for sending at any baud rate, but problematic at receiving with baud rates above 35000,
  // since the calculated interval will be below 1000µs!
  // f.i. 115200bd ==> interval=304µs
  if (MSRinterval < 1000) MSRinterval = 1000;  // minimum of 1msec interval

  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "MBsrv%02XRTU", instanceCounter);

  // Start task to handle the client
  xTaskCreatePinnedToCore((TaskFunction_t)&serve, taskName, 4096, this, 5, &serverTask, coreID >= 0 ? coreID : NULL);

  Serial.printf("Created server task %d\n", (uint32_t)serverTask);

  return true;
}

// stop: kill server task
bool ModbusServerRTU::stop() {
  if (serverTask != nullptr) {
    vTaskDelete(serverTask);
    serverTask = nullptr;
  }
  return true;
}

// serve: loop until killed and receive messages from the RTU interface
void ModbusServerRTU::serve(ModbusServerRTU *myData) {
  RTUMessage request;                  // received request message
  ResponseType m;                      // Application's response data
  ResponseType response;               // Response proper to be sent

  myData->MSRlastMicros = micros();

  while (true) {
    // Initialize all temporary vectors
    request.clear();
    response.clear();
    m.clear();

    // Wait for and read an request
    request = RTUutils::receive(myData->MSRserial, myData->serverTimeout, myData->MSRlastMicros, myData->MSRinterval);

    // Request longer than 1 byte? 
    if (request.size() > 1) {
      // Yes. Do we have a callback function registered for it?
      MBSworker callBack = myData->getWorker(request[0], request[1]);
      if (callBack) {
        // Yes, we do. Is the request valid (CRC correct)?
        if (RTUutils::validCRC(request.data(), request.size())) {
          // Yes, is valid. First count it
          {
            lock_guard<mutex> cntLock(myData->m);
            myData->messageCount++;
          }
          // Get the user's response
          m = callBack(request[0], request[1], request.size() - 4, request.data() + 2);
          // Process Response
          // One of the predefined types?
          if (m[0] == 0xFF && (m[1] == 0xF0 || m[1] == 0xF1 || m[1] == 0xF2 || m[1] == 0xF3)) {
            // Yes. Check it
            switch (m[1]) {
            case 0xF0: // NIL
              response.clear();
              break;
            case 0xF1: // ECHO
              response = request;
              break;
            case 0xF2: // ERROR
              response.push_back(request[0]);
              response.push_back(request[1] | 0x80);
              response.push_back(m[2]);
              break;
            case 0xF3: // DATA
              response.push_back(request[0]);
              response.push_back(request[1]);
              for (auto byte = m.begin() + 2; byte < m.end(); ++byte) {
                response.push_back(*byte);
              }
              break;
            default:   // Will not get here!
              break;
            }
          } else {
            // No. User provided data in free format
            for (auto& byte : m) {
              response.push_back(byte);
            }
          }
        } else {
          // No, CRC is wrong. Send error response
          response.push_back(request[0]);
          response.push_back(request[1] | 0x80);
          response.push_back(CRC_ERROR);
        }
      } else {
        // No callback. Is at least the serverID valid?
        if (myData->isServerFor(request[0])) {
          // Yes. Send back a ILLEGAL_FUNCTION error
          response.push_back(request[0]);
          response.push_back(request[1] | 0x80);
          response.push_back(ILLEGAL_FUNCTION);
        }
        // Else we will ignore the request, as it is not meant for us!
      }
      // Do we have gathered a valid response now?
      if (response.size() >= 3) {
        RTUutils::send(myData->MSRserial, myData->MSRlastMicros, myData->MSRinterval, myData->MSRrtsPin, response);
      }
    } else {
      // No, we got a 1-byte request, meaning an error has happened in receive()
      // This is a server, so we will ignore TIMEOUT.
      if (request[0] != TIMEOUT) {
        ModbusError me((Error)request[0]);
        Serial.printf("RTU receive: %02X - %s\n", (int)me, (const char *)me);
      }
    }
    // Give scheduler room to breathe
    delay(1);
  }
}
