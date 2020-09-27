// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _TCPSTUB_H
#define _TCPSTUB_H
#include <Client.h>
#include <map>
#include <vector>
#include <queue>
#include <mutex>

using std::mutex;
using std::lock_guard;
using std::vector;
using std::queue;

#define QUEUELIMIT 500

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
}

// Struct holding the data for a test case
// These will be <map>ped to the transactionID for the TCPstub worker to identify the request
struct TestCase {
  uint16_t transactionID;        // For easy reference, again the transactionID
  uint32_t delayTime;            // A time in ms to wait before the response is sent
  vector<uint8_t> response;      // byte sequence of the response
  vector<uint8_t> expected;      // byte sequence to be expected in onError/onData handlers
};

// Short name for the test cases' map
using TestMap = std::map<uint16_t, TestCase *>;

class TCPstub : public Client {
public:
// Constructors
  TCPstub();
  TCPstub(TCPstub& t);
  TCPstub(IPAddress ip, uint16_t port);

// Destructor
  ~TCPstub();

// Assignment operator
  TCPstub& operator= (TCPstub& t);

// Client.h method set
  // Connect will check the identity and only start the worker task if it is matching
  int connect(IPAddress ip, uint16_t port);

  // We do not need hostnames here, so we will return EADDRNOTAVAIL (=99) to prevent use
  inline int connect(const char *host, uint16_t port) { return 99; }

  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);

  int available();
  int read();
  int read(uint8_t *buf, size_t size);
  int peek();

  // flush makes no sense, so let it do nothing
  inline void flush() { }

  // stop will kill the worker task
  void stop();

  // connected will return true (=1) as soon as we have a running worker task
  inline uint8_t connected() { return (worker ? 1 : 0); }
  inline operator bool() { return (worker ? true : false); }

// Special stub methods
  // begin connects the TCPstub to a test case map and optionally sets the identity
  bool begin(TestMap* mp);
  bool begin(TestMap* mp, IPAddress ip, uint16_t port);

  // setIdentity changes the simulated host/port
  void setIdentity(IPAddress ip, uint16_t port);

protected:
  IPAddress myIP;
  uint16_t  myPort;
  TaskHandle_t worker;
  TestMap* tm;
  queue<uint8_t> inQueue;
  queue<uint8_t> outQueue;
  mutex inLock;
  mutex outLock;

  // handleConnection: worker task method
  static void workerTask(TCPstub *instance);

};

#endif
