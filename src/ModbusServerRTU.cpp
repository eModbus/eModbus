// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusServerRTU.h"

// Init number of created ModbusServerRTU objects
uint8_t ModbusServerRTU::instanceCounter = 0;

// Constructor
ModbusServerRTU::ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, int rtsPin) :
  ModbusServer(),
  serverTask(nullptr),
  serverTimeout(20000),
  MSRserial(serial),
  MSRinterval(2000),     // will be calculated in start()!
  MSRlastMicros(0),
  MSRrtsPin(rtsPin) {
  // Count instances one up
  instanceCounter++;
  // If we have a GPIO RE/DE pin, configure it.
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
  // Has to be precise for a server:
  MSRinterval = 35000000UL / MSRserial.baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud

  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "MBsrv%02XRTU", instanceCounter);

  // Start task to handle the client
  xTaskCreatePinnedToCore((TaskFunction_t)&serve, taskName, 4096, this, 8, &serverTask, coreID >= 0 ? coreID : NULL);

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
void ModbusServerRTU::serve(ModbusServerRTU *myServer) {
  RTUMessage request;                  // received request message
  ResponseType m;                      // Application's response data
  ResponseType response;               // Response proper to be sent

  // init microseconds timer
  myServer->MSRlastMicros = micros();

  while (true) {
    // Initialize all temporary vectors
    request.clear();
    response.clear();
    m.clear();

    // Wait for and read an request
    request = RTUutils::receive(myServer->MSRserial, myServer->serverTimeout, myServer->MSRlastMicros, myServer->MSRinterval, "SRV REQ");
    /*
    if (request.size() > 1) {
      for (auto& byte : request) {
        Serial.printf("%02X ", byte);
      }
      Serial.println("Requested");
    } else {
      Serial.println("Request size <= 1?");
    }
    */

    // Request longer than 1 byte (that will signal an error in receive())? 
    if (request.size() > 1) {
      // Yes. Do we have a callback function registered for it?
      MBSworker callBack = myServer->getWorker(request[0], request[1]);
      if (callBack) {
        // Yes, we do. Is the request valid (CRC correct)?
        if (RTUutils::validCRC(request.data(), request.size())) {
          // Yes, is valid. First count it
          {
            lock_guard<mutex> cntLock(myServer->m);
            myServer->messageCount++;
          }
          // Get the user's response
          // Length is 4 bytes less than received, to omit serverID, FC and CRC bytes
          // Offset is 2 to skip the serverID and FC
          m = callBack(request[0], request[1], request.size() - 4, request.data() + 2);

          // Process Response. Is it one of the predefined types?
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
            default:   // Will not get here, but lint likes it!
              break;
            }
          } else {
            // No predefined. User provided data in free format
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
        if (myServer->isServerFor(request[0])) {
          // Yes. Send back a ILLEGAL_FUNCTION error
          response.push_back(request[0]);
          response.push_back(request[1] | 0x80);
          response.push_back(ILLEGAL_FUNCTION);
          /*
          for (auto& byte : response) {
            Serial.printf("%02X ", byte);
          }
          Serial.println("illegal FC");
          */
        }
        // Else we will ignore the request, as it is not meant for us!
      }
      // Do we have gathered a valid response now?
      if (response.size() >= 3) {
        // Yes. send it back.
        RTUutils::send(myServer->MSRserial, myServer->MSRlastMicros, myServer->MSRinterval, myServer->MSRrtsPin, response, "SRV RSP");
      }
    } else {
      // No, we got a 1-byte request, meaning an error has happened in receive()
      // This is a server, so we will ignore TIMEOUT.
      if (request[0] != TIMEOUT) {
        // Any other error could be important for debugging, so print it
        ModbusError me((Error)request[0]);
        Serial.printf("RTU receive: %02X - %s\n", (int)me, (const char *)me);
      }
    }
    // Give scheduler room to breathe
    delay(1);
  }
}
