// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "TCPstub.h"

// Constructors
TCPstub::TCPstub() :
  myIP(IPAddress(0, 0, 0, 0)),
  myPort(0),
  worker(nullptr),
  tm(nullptr) { }

TCPstub::TCPstub(TCPstub& t) :
  myIP(t.myIP),
  myPort(t.myPort),
  worker(nullptr),
  tm(nullptr) { }

TCPstub::TCPstub(IPAddress ip, uint16_t port) :
  myIP(ip),
  myPort(port),
  worker(nullptr),
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
    worker = nullptr;
  }
  // We may not take over the test case map!
  tm = nullptr;
  return *this;
}

// Client.h method set
// Connect will check the identity and only start the worker task if it is matching
int TCPstub::connect(IPAddress ip, uint16_t port) {
  /*
  Serial.print("Identity: ");
  Serial.print(myIP);
  Serial.print("/");
  Serial.print(myPort);
  Serial.print(" - connect requests ");
  Serial.print(ip);
  Serial.print("/");
  Serial.println(port);
  */
  if (ip == myIP && port == myPort) {
    // if we do not have a worker already running
    if (!worker) {
      // Start task to handle the queue
      xTaskCreatePinnedToCore((TaskFunction_t)&workerTask, "TCPstub", 4096, this, 6, &worker, 1);
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

// flush will do nothing
void TCPstub::flush() {
}

void TCPstub::clear() {
  // Delete inQueue, if anything is still in it
  if (!inQueue.empty()) {
    lock_guard<mutex> Lin(inLock);
    while (!inQueue.empty()) {
      inQueue.pop();
    }
  }
}

// stop will kill the worker task
void TCPstub::stop() {
  if (worker) {
    vTaskDelete(worker);
    worker = nullptr;
  }
  // Delete inQueue, if anything is still in it
  if (!inQueue.empty()) {
    lock_guard<mutex> Lin(inLock);
    while (!inQueue.empty()) {
      inQueue.pop();
    }
  }
}

// Special stub methods
// begin connects the TCPstub to a test case map and optionally sets the identity
bool TCPstub::begin(TidMap *mp) {
  tm = mp;
  if (tm && myIP && myPort) return true;
  return false;    
}

bool TCPstub::begin(TidMap *mp, IPAddress ip, uint16_t port) {
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
  while (1) {
    // Do we have at least 6 bytes in the inQueue (TCP header)?
    if (instance->inQueue.size() >= 6) {
      // Yes. This should be led in by the transactionID. To read it, lock the queue
      uint16_t tid = 0;
      uint8_t TCPhead[6];
      {
        lock_guard<mutex> lockIn(instance->inLock);
        uint16_t i = 0;
        // Serial.print("Read  ");
        // Keep the TCPhead and discard the rest.
        while (!instance->inQueue.empty()) {
          uint8_t byte = instance->inQueue.front();
          if (i < 6) {
            TCPhead[i++] = byte;
          }
          instance->inQueue.pop();
          // Serial.printf("%02X ", byte);
        }
        // Serial.println((*instance->tm).size());
      }
      // Get the TID
      tid = ((TCPhead[0] << 8) & 0xFF) | (TCPhead[1] & 0xFF);

      // Look for the tid in the TestCase map
      auto tc = (*instance->tm).find(tid);
      if (tc != (*instance->tm).end()) {
        // Get a handier pointer for the TestCase found
        TestCase *myTest(tc->second);

        // Does the test case prescribe an initial delay?
        if (myTest->delayTime) {
          // Yes. idle until time has passed
          // Serial.printf("Wait for %d\n", myTest->delayTime);
          delay(myTest->delayTime);
        }
        // Do we have to send a response?
        if (myTest->response.size() > 0) {
          // Yes, we do. Lock the outQueue, since we ar egoing to write to it
          lock_guard<mutex> lockOut(instance->outLock);

          // Are we asked to fake the transaction ID?
          if (myTest->fakeTransactionID == true) {
            TCPhead[0] += 13;
          }

          // Set the response size in the TCP header
          TCPhead[4] = (myTest->response.size() << 8) & 0xFF;
          TCPhead[5] = myTest->response.size() & 0xFF;

          // Serial.print("Write ");
          // Write the TCP header
          for (uint8_t i = 0; i < 6; ++i) {
            instance->outQueue.push(TCPhead[i]);
            // Serial.printf("%02X ", TCPhead[i]);
          }

          // Now write the response
          const uint8_t *cp = myTest->response.data();
          size_t cnt = myTest->response.size();
          while (cnt--) {
            instance->outQueue.push(*cp);
            // Serial.printf("%02X ", *cp);
            cp++;
          }
          // Serial.println();
        }
        // Are we to stop ourselves after response has been sent?
        if (myTest->stopAfterResponding == true) {
          instance->worker = nullptr;
          vTaskDelete(NULL);
        }
      } else {
        Serial.printf("No test case for TID %04X\n", tid);
      }
    } else {
      delay(10);
    }
  }
}
