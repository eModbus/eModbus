// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_TCP_TEMP_H
#define _MODBUS_SERVER_TCP_TEMP_H

#include <Arduino.h>

#include "ModbusServer.h"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

using std::vector;
using TCPMessage = std::vector<uint8_t>;

template <typename ST, typename CT>
class ModbusServerTCP : public ModbusServer {
public:
  // Constructor
  ModbusServerTCP();

  // Destructor: closes the connections
  ~ModbusServerTCP();

  // activeClients: return number of clients currently employed
  uint16_t activeClients();

  // start: create task with TCP server to accept requests
  bool start(uint16_t port, uint8_t maxClients, uint32_t timeout, int coreID = -1);

  // stop: drop all connections and kill server task
  bool stop();

protected:
  inline void isInstance() { }

  uint8_t numClients;
  TaskHandle_t serverTask;
  uint16_t serverPort;
  uint32_t serverTimeout;

  struct ClientData {
    ClientData() : task(nullptr), client(0), timeout(0), parent(nullptr) {}
    ClientData(TaskHandle_t t, CT& c, uint32_t to, ModbusServerTCP<ST, CT> *p) : 
      task(t), client(c), timeout(to), parent(p) {}
    TaskHandle_t task;
    CT client;
    uint32_t timeout;
    ModbusServerTCP<ST, CT> *parent;
  };
  ClientData **clients;

  // serve: loop function for server task
  static void serve(ModbusServerTCP<ST, CT> *myself);

  // worker: loop function for client tasks
  static void worker(ClientData *myData);

  // receive: read data from TCP
  TCPMessage receive(CT& client, uint32_t timeWait);

  // accept: start a task to receive requests and respond to a given client
  bool accept(CT& client, uint32_t timeout, int coreID = -1);

  // clientAvailable: return true,. if a client slot is currently unused
  bool clientAvailable() { return (numClients - activeClients()) > 0; }
};

// Constructor
template <typename ST, typename CT>
ModbusServerTCP<ST, CT>::ModbusServerTCP() :
  ModbusServer(),
  numClients(0),
  serverTask(nullptr),
  serverPort(502),
  serverTimeout(20000) {
    clients = new ClientData*[numClients]();
   }

// Destructor: closes the connections
template <typename ST, typename CT>
ModbusServerTCP<ST, CT>::~ModbusServerTCP() {
  for (uint8_t i = 0; i < numClients; ++i) {
    if (clients[i] != nullptr) {
      delete clients[i];
    }
  }
  delete[] clients;
}

// activeClients: return number of clients currently employed
template <typename ST, typename CT>
uint16_t ModbusServerTCP<ST, CT>::activeClients() {
  uint8_t cnt = 0;
  for (uint8_t i = 0; i < numClients; ++i) {
    // Current slot could have been previously used - look for cleared task handles
    if (clients[i] != nullptr) {
      // Empty task handle?
      if (clients[i]->task == nullptr) {
        // Yes. Delete entry and init client pointer
        delete clients[i];
        clients[i] = nullptr;
      }
    }
    if (clients[i] != nullptr) cnt++;
  }
  return cnt;
}

  // start: create task with TCP server to accept requests
template <typename ST, typename CT>
  bool ModbusServerTCP<ST, CT>::start(uint16_t port, uint8_t maxClients, uint32_t timeout, int coreID) {
    // Task already running?
    if (serverTask != nullptr) {
      // Yes. stop it first
      stop();
    }
    // Does the required number of slots fit?
    if (numClients != maxClients) {
      // No. Drop array and allocate a new one
      delete[] clients;
      // Now allocate a new one
      numClients = maxClients;
      clients = new ClientData*[numClients]();
    }
    serverPort = port;
    serverTimeout = timeout;

    // Create unique task name
    char taskName[12];
    snprintf(taskName, 12, "MBserve%04X", port);

    // Start task to handle the client
    xTaskCreatePinnedToCore((TaskFunction_t)&serve, taskName, 4096, this, 5, &serverTask, coreID >= 0 ? coreID : NULL);

    return true;
  }

  // stop: drop all connections and kill server task
template <typename ST, typename CT>
  bool ModbusServerTCP<ST, CT>::stop() {
    // Check for clients still connected
    for (uint8_t i = 0; i < numClients; ++i) {
      // Client is alive?
      if (clients[i] != nullptr) {
        // Yes. Close the connection
        clients[i]->client.stop();
        delay(50);
        // Kill the client task
        vTaskDelete(clients[i]->task);
        delete clients[i];
        clients[i] = nullptr;
      }
    }
    if (serverTask != nullptr) {
      vTaskDelete(serverTask);
      serverTask = nullptr;
    }
    return true;
  }

// accept: start a task to receive requests and respond to a given client
template <typename ST, typename CT>
bool ModbusServerTCP<ST, CT>::accept(CT& client, uint32_t timeout, int coreID) {
  // Look for an empty client slot
  for (uint8_t i = 0; i < numClients; ++i) {
    // Empty slot?
    if (clients[i] == nullptr) {
      // Yes. allocate new client data in slot
      clients[i] = new ClientData(0, client, timeout, this);

      // Create unique task name
      char taskName[12];
      snprintf(taskName, 12, "MBsrv%02Xclnt", i);

      // Start task to handle the client
      xTaskCreatePinnedToCore((TaskFunction_t)&worker, taskName, 4096, clients[i], 5, &clients[i]->task, coreID >= 0 ? coreID : NULL);

      return true;
    }
  }
  return false;
}

template <typename ST, typename CT>
void ModbusServerTCP<ST, CT>::serve(ModbusServerTCP<ST, CT> *myself) {
  // Set up server with given port
  ST server(myself->serverPort);

  // Start it
  server.begin();

  // Loop until being killed
  while (true) {
    // Do we have clients left to use?
    if (myself->clientAvailable()) {
      // Yes. accept one, when it connects
      CT ec = server.accept();
      // Did we get a connection?
      if (ec) {
        // Yes. Forward it to the Modbus server
        myself->accept(ec, myself->serverTimeout, 0);
        // Serial.printf("Accepted connection - %d clients running\n", myself->activeClients());
      }
    }
    // Give scheduler room to breathe
    delay(10);
  }
}

template <typename ST, typename CT>
void ModbusServerTCP<ST, CT>::worker(ClientData *myData) {
  // Get own reference data in handier form
  CT myClient = myData->client;
  uint32_t myTimeOut = myData->timeout;
  // TaskHandle_t myTask = myData->task;
  ModbusServerTCP<ST, CT> *myParent = myData->parent;
  uint32_t myLastMessage = millis();
  ResponseType response;               // Data buffer to hold prepared response

  // loop forever, if timeout is 0, or until timeout was hit
  while (myClient.connected() && (!myTimeOut || (millis() - myLastMessage < myTimeOut))) {
    // Get a request
    if (myClient.available()) {
      response.clear();
      TCPMessage m = myParent->receive(myClient, 100);

      // Serial.print(" Request received: ");
      // for (auto& byte : m) { Serial.printf(" %02X", byte); }
      // Serial.println();

      // has it the minimal length (6 bytes TCP header plus serverID plus FC)?
      if (m.size() >= 8) {
        {
          lock_guard<mutex> cntLock(myParent->m);
          myParent->messageCount++;
        }
        // Yes. Take over TCP header for later response
        for (uint8_t i = 0; i < 6; ++i) {
          response.push_back(m[i]);
        }
        // ServerID shall be at [6], FC at [7]. Check both
        if (myParent->isServerFor(m[6])) {
          // Server is correct - in principle. Do we serve the FC?
          MBSworker callBack = myParent->getWorker(m[6], m[7]);
          if (callBack) {
            // Yes, we do. Invoke the worker method to get a response
            ResponseType data = callBack(m[6], m[7], m.size() - 8, m.data() + 8);
            // for (auto& byte : data) {
            //   Serial.printf("%02X ", byte);
            // }
            // Serial.println("data received");
            // Process Response
            // One of the predefined types?
            if (data[0] == 0xFF && (data[1] == 0xF0 || data[1] == 0xF1 || data[1] == 0xF2 || data[1] == 0xF3)) {
              // Yes. Check it
              switch (data[1]) {
              case 0xF0: // NIL
                response.clear();
                break;
              case 0xF1: // ECHO
                response = m;
                break;
              case 0xF2: // ERROR
                response[4] = 0;
                response[5] = 3;
                response.push_back(m[6]);
                response.push_back(m[7] | 0x80);
                response.push_back(data[2]);
                break;
              case 0xF3: // DATA
                response[4] = (data.size() >> 8) & 0xFF;
                response[5] = data.size() & 0xFF;
                response.push_back(m[6]);
                response.push_back(m[7]);
                for (auto byte = data.begin() + 2; byte < data.end(); ++byte) {
                  response.push_back(*byte);
                }
                // for (auto& byte : response) {
                //   Serial.printf("%02X ", byte);
                // }
                // Serial.println("Response generated");
                break;
              default:   // Will not get here!
                break;
              }
            } else {
              // No. User provided data in free format
              response[4] = (data.size() >> 8) & 0xFF;
              response[5] = data.size() & 0xFF;
              for (auto& byte : data) {
                response.push_back(byte);
              }
            }
          } else {
            // No, function code is not served here
            response[4] = 0;
            response[5] = 3;
            response.push_back(m[6]);
            response.push_back(m[7] | 0x80);
            response.push_back(ILLEGAL_FUNCTION);
          }
        } else {
          // No, serverID is not served here
          response[4] = 0;
          response[5] = 3;
          response.push_back(m[6]);
          response.push_back(m[7] | 0x80);
          response.push_back(INVALID_SERVER);
        }
      }
      delay(1);
      // Do we have a response to send?

      // Serial.print(" Response: ");
      // for (auto& byte : response) { Serial.printf(" %02X", byte); }
      // Serial.println();

      if (response.size() >= 8) {
        // Yes. Do it now.
        myClient.write(response.data(), response.size());
        myClient.flush();
      }
      // We did something communicationally - rewind timeout timer
      myLastMessage = millis();
    }

    delay(1);
  }

  // Timeout!
  // Read away all that may still hang in the buffer
  while (myClient.available()) { myClient.read(); }
  // Now stop the client
  myClient.stop();

  // Hack to remove the response vector from memory
  vector<uint8_t>().swap(response);

  myData->task = nullptr;

  delay(50);
  vTaskDelete(NULL);
}

// receive: get request via Client connection
template <typename ST, typename CT>
TCPMessage ModbusServerTCP<ST, CT>::receive(CT& client, uint32_t timeWait) {
  uint32_t lastMillis = millis();     // Timer to check for timeout
  TCPMessage m;                       // vector to take read data
  register uint16_t lengthVal = 0;
  register uint16_t cnt = 0;
  uint8_t buffer[300];

  // wait for packet data or timeout
  while (millis() - lastMillis < timeWait) {
    // Is there data waiting?
    if (client.available()) {
      // Yes. catch as much as is there and fits into buffer
      while (client.available() && ((cnt < 6) || (cnt < lengthVal))) {
        buffer[cnt] = client.read();
        if (cnt == 4) lengthVal = buffer[cnt] << 8;
        if (cnt == 5) {
          lengthVal |= buffer[cnt];
          lengthVal += 6;
        }
        cnt++;
      }
      delay(1);
      // Rewind EOT and timeout timers
      lastMillis = millis();
    }
    delay(1); // Give scheduler room to breathe
  }
  if (cnt) {
    uint16_t i = 0;
    while (cnt--) {
      m.push_back(buffer[i++]);
    }
  }
  return m;
}

#endif
