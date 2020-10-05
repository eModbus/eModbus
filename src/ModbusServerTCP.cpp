// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusServerTCP.h"

uint8_t ModbusServerTCP::clientCounter = 0;

// Constructor
ModbusServerTCP::ModbusServerTCP() :
  ModbusServer() { }

// Destructor: closes the connections
ModbusServerTCP::~ModbusServerTCP() {

}

// accept: start a task to receive requests and respond to a given client
bool ModbusServerTCP::accept(Client& client, uint32_t timeout, int coreID) {
  // Add the new client to the list
  clients.push_back( { 0, &client, timeout, this } );
  // get pointer to its data to give to the task
  ClientData *cd = &clients.back();

  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "MBsrv%02XTCP", ++clientCounter);

  // Start task to handle the client
  xTaskCreatePinnedToCore((TaskFunction_t)&worker, taskName, 4096, cd, 5, &(cd->task), coreID >= 0 ? coreID : NULL);

  return (cd->task) ? true : false;
}

// updateClients: kill disconnected clients
bool ModbusServerTCP::updateClients() {
  bool hadOne = false;

  // Loop over all clients entries...
  //for (auto checkC = clients.begin(); checkC != clients.end(); checkC++) {
  for (uint16_t checkC = 0; checkC < clients.size(); checkC++) {
    // ...to find a disconnected one. If we found one...
    if (clients[checkC].client->connected()) {
      // ...kill the task
      vTaskDelete(clients[checkC].task);
      // ...and remove it from the list
      clients.erase(clients.begin() + checkC);
      hadOne = true;
    }
  }
  return hadOne;
}

void ModbusServerTCP::worker(ClientData *myData) {
  // Get own reference data in handier form
  Client *myClient = myData->client;
  uint32_t myTimeOut = myData->timeout;
  TaskHandle_t myTask = myData->task;
  ModbusServerTCP *myParent = myData->parent;
  uint32_t myLastMessage = millis();

  // loop forever, if timeout is 0, or until timeout was hit
  while (!myTimeOut || (millis() - myLastMessage < myTimeOut)) {
    delay(1);
  }

  // Timeout!
  // We will only disconnect the client - the task gets killed by the server
  myClient->stop();

  // Wait for the hammer to fall...
  while (true) { delay(1); }
}

