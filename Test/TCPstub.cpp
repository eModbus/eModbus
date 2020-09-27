// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "TCPstub.h"

// Constructors
TCPstub::TCPstub() :
  myIP(IPAddress(0, 0, 0, 0)),
  myPort(0),
  worker(0),
  tm(nullptr) { }

TCPstub::TCPstub(TCPstub& t) :
  myIP(t.myIP),
  myPort(t.myPort),
  worker(0),
  tm(nullptr) { }

TCPstub::TCPstub(IPAddress ip, uint16_t port) :
  myIP(ip),
  myPort(port),
  worker(0),
  tm(nullptr) { }

// Destructor
TCPstub::~TCPstub() {
  if (worker) {
    vTaskDelete(worker);
  }
}

// Assignment operator - copy identity only!
TCPstub& TCPstub::operator=(TCPstub& t) {
  // take over host IP and port
  myIP = t.myIP;
  myPort = t.myPort;

  // if we had a worker task running, kill it
  if (worker) {
    vTaskDelete(worker);
    worker = 0;
  }
  // We may not take over the test case map!
  tm = nullptr;
  return *this;
}

// Client.h method set
// Connect will check the identity and only start the worker task if it is matching
int TCPstub::connect(IPAddress ip, uint16_t port) {
  if (ip == myIP && port == myPort) {
    // if we do not have a worker already running
    if (!worker) {
      // Start task to handle the queue
      xTaskCreatePinnedToCore((TaskFunction_t)&workerTask, "TCPstub", 4096, this, 5, &worker, 1);
    }
    return 0;
  }
  // Else we return a EADDRNOTAVAIL
  return 99;
}

// write shoves a byte into the worker's inQueue
size_t TCPstub::write(uint8_t byte) {
  if (inQueue.size() < QUEUELIMIT) {
    // Safely lock access
    lock_guard<mutex> lockGuard(inLock);
    inQueue.push(byte);
    return 1;
  }
  return 0;
}

// ... or a rack of bytes in sequence
size_t TCPstub::write(const uint8_t *buf, size_t size) {
  size_t cnt = 0;
  // Safely lock access
  lock_guard<mutex> lockGuard(inLock);
  while (inQueue.size() < QUEUELIMIT && size--) {
    inQueue.push(*buf++);
    cnt++;
  }
  return cnt;
}

// available returns the number of bytes in the worker's outQueue
int TCPstub::available() {
  return outQueue.size();
}

// read picks a single byte from the worker's outQueue and deletes it
int TCPstub::read() {
  if (outQueue.empty()) return -1;
  uint8_t byte = outQueue.front();
  // Safely lock access
  lock_guard<mutex> lockGuard(outLock);
  outQueue.pop();
  return byte;
}

// The array read does the same for the requested number of bytes or until the outQueue is exhausted
int TCPstub::read(uint8_t *buf, size_t size) {
  if (outQueue.empty()) return -1;
  size_t cnt = 0;
  // Safely lock access
  lock_guard<mutex> lockGuard(outLock);
  while (!outQueue.empty() && size--) {
    *buf++ = outQueue.front();
    outQueue.pop();
    cnt++;
  }
  return cnt;
}

// peek works as read, without deleting the picked byte
int TCPstub::peek() {
  if (outQueue.empty()) return -1;
  uint8_t byte = outQueue.front();
  return byte;
}

// stop will kill the worker task
void TCPstub::stop() {
  if (worker) {
    vTaskDelete(worker);
    worker = 0;
  }
}

// Special stub methods
// begin connects the TCPstub to a test case map and optionally sets the identity
bool TCPstub::begin(TestMap *mp) {
  tm = mp;
  if (tm && myIP && myPort) return true;
  return false;    
}

bool TCPstub::begin(TestMap *mp, IPAddress ip, uint16_t port) {
  tm = mp;
  setIdentity(ip, port);
  if (tm && myIP && myPort) return true;
  return false;    
}

// setIdentity changes the simulated host/port
void TCPstub::setIdentity(IPAddress ip, uint16_t port) {
  myIP = ip;
  myPort = port;    
}

// handleConnection: worker task method
void TCPstub::workerTask(TCPstub *instance) {
  Serial.println("worker task started");

  // For now, copy inQueue to outQueue
  while (1) {
    if (!instance->inQueue.empty()) {
      // Serial.print(instance->inQueue.front(), HEX);
      // Safely lock access
      lock_guard<mutex> lockIn(instance->inLock);
      lock_guard<mutex> lockOut(instance->outLock);
      instance->outQueue.push(instance->inQueue.front());
      instance->inQueue.pop();
    } else {
      delay(1);
    }
  }
}
