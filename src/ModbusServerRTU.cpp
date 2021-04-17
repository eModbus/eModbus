// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusServerRTU.h"

#if HAS_FREERTOS

#undef LOG_LEVEL_LOCAL
#include "Logging.h"

// Init number of created ModbusServerRTU objects
uint8_t ModbusServerRTU::instanceCounter = 0;

// Constructor with RTS pin GPIO (or -1)
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
    MRTSrts = [this](bool level) {
      digitalWrite(MSRrtsPin, level);
    };
    MRTSrts(LOW);
  } else {
    MRTSrts = RTUutils::RTSauto;
  }
  // Set the UART FIFO copy threshold to 1 byte
  RTUutils::UARTinit(serial, 1);
}

// Constructor with RTS callback
ModbusServerRTU::ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, RTScallback rts) :
  ModbusServer(),
  serverTask(nullptr),
  serverTimeout(20000),
  MSRserial(serial),
  MSRinterval(2000),     // will be calculated in start()!
  MSRlastMicros(0),
  MRTSrts(rts) {
  // Count instances one up
  instanceCounter++;
  // Configure RTS callback
  MSRrtsPin = -1;
  MRTSrts(LOW);
  // Set the UART FIFO copy threshold to 1 byte
  RTUutils::UARTinit(serial, 1);
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
    LOG_D("Server task was running - stopped.\n");
  }

  // start only if serial interface is initialized!
  if (MSRserial.baudRate()) {
    // silent interval is at least 3.5x character time
    // Has to be precise for a server:
    MSRinterval = 35000000UL / MSRserial.baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud

    // Create unique task name
    char taskName[12];
    snprintf(taskName, 12, "MBsrv%02XRTU", instanceCounter);

    // Start task to handle the client
    xTaskCreatePinnedToCore((TaskFunction_t)&serve, taskName, 4096, this, 8, &serverTask, coreID >= 0 ? coreID : NULL);

    LOG_D("Server task %d started. Interval=%d\n", (uint32_t)serverTask, MSRinterval);
  } else {
    LOG_E("Server task could not be started. HardwareSerial not initialized?\n");
    return false;
  }

  return true;
}

// stop: kill server task
bool ModbusServerRTU::stop() {
  if (serverTask != nullptr) {
    vTaskDelete(serverTask);
    LOG_D("Server task %d stopped.\n", (uint32_t)serverTask);
    serverTask = nullptr;
  }
  return true;
}

// serve: loop until killed and receive messages from the RTU interface
void ModbusServerRTU::serve(ModbusServerRTU *myServer) {
  ModbusMessage request;                // received request message
  ModbusMessage m;                      // Application's response data
  ModbusMessage response;               // Response proper to be sent

  // init microseconds timer
  myServer->MSRlastMicros = micros();

  while (true) {
    // Initialize all temporary vectors
    request.clear();
    response.clear();
    m.clear();

    // Wait for and read an request
    request = RTUutils::receive(myServer->MSRserial, myServer->serverTimeout, myServer->MSRlastMicros, myServer->MSRinterval);

    // Request longer than 1 byte (that will signal an error in receive())? 
    if (request.size() > 1) {
      LOG_D("Request received.\n");

      // Yes. Do we have a callback function registered for it?
      MBSworker callBack = myServer->getWorker(request[0], request[1]);
      if (callBack) {
        LOG_D("Callback found.\n");
        // Yes, we do. Is the request valid (CRC correct)?
        if (RTUutils::validCRC(request.data(), request.size())) {
          LOG_D("CRC okay.\n");
          // Yes, is valid. First count it
          {
            lock_guard<mutex> cntLock(myServer->m);
            myServer->messageCount++;
          }
          // Get the user's response
          // Cut the CRC bytes
          request.resize(request.size() - 2);
          m = callBack(request);
          LOG_D("Callback called.\n");
          HEXDUMP_V("Callback response", m.data(), m.size());

          // Process Response. Is it one of the predefined types?
          if (m[0] == 0xFF && (m[1] == 0xF0 || m[1] == 0xF1)) {
            // Yes. Check it
            switch (m[1]) {
            case 0xF0: // NIL
              response.clear();
              break;
            case 0xF1: // ECHO
              response = request;
              if (request.getFunctionCode() == WRITE_MULT_REGISTERS ||
                  request.getFunctionCode() == WRITE_MULT_COILS) {
                response.resize(6);
              }
              break;
            default:   // Will not get here, but lint likes it!
              break;
            }
          } else {
            // No predefined. User provided data in free format
            response = m;
          }
        } else {
          // No, CRC is wrong. Send error response
          response.setError(request.getServerID(), request.getFunctionCode(), CRC_ERROR);
        }
      } else {
        // No callback. Is at least the serverID valid?
        if (myServer->isServerFor(request[0])) {
          // Yes. Send back a ILLEGAL_FUNCTION error
          response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_FUNCTION);
        }
        // Else we will ignore the request, as it is not meant for us!
      }
      // Do we have gathered a valid response now?
      if (response.size() >= 3) {
        // Yes. send it back.
        RTUutils::send(myServer->MSRserial, myServer->MSRlastMicros, myServer->MSRinterval, myServer->MRTSrts, response);
        LOG_D("Response sent.\n");
      }
    } else {
      // No, we got a 1-byte request, meaning an error has happened in receive()
      // This is a server, so we will ignore TIMEOUT.
      if (request[0] != TIMEOUT) {
        // Any other error could be important for debugging, so print it
        ModbusError me((Error)request[0]);
        LOG_E("RTU receive: %02X - %s\n", (int)me, (const char *)me);
      }
    }
    // Give scheduler room to breathe
    delay(1);
  }
}

#endif
