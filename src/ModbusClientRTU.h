// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_RTU_H
#define _MODBUS_CLIENT_RTU_H
#include "ModbusMessageRTU.h"
#include "ModbusClient.h"
#include "HardwareSerial.h"
#include "RTUutils.h"
#include <queue>
#include <mutex>                  // NOLINT
#include <vector>

using std::queue;
using std::mutex;
using std::lock_guard;
using RTUMessage = std::vector<uint8_t>;

#define DEFAULTTIMEOUT 2000

class ModbusClientRTU : public ModbusClient {
public:

template <typename... Args>
Error addRequest(Args&&... args) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, std::forward<Args>(args) ...);

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

template <typename... Args>
RTUMessage generateRequest(Args&&... args) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, std::forward<Args>(args) ..., 0xDEADDEAD);

  // Was the message generated?
  if (rc != SUCCESS) {
    // No. Return the Error code only - vector size is 1
    rv.reserve(1);
    rv.push_back(rc);
  // If it was successful - did we get a message?
  } else if (r) {
    // Yes, obviously. 
    // Resize the vector to take message proper plus CRC (2 bytes)
    rv.reserve(r->len() + 2);
    rv.resize(r->len() + 2);

    // Do a fast (non-C++-...) copy
    uint8_t *cp = rv.data();
    // Copy in message contents
    memcpy(cp, r->data(), r->len());
    cp[r->len()] = (r->CRC) & 0xFF;
    cp[r->len() + 1] = (r->CRC >> 8) & 0xFF;
  }
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

  // Constructor takes Serial reference and optional DE/RE pin and queue limit
  explicit ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin = -1, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientRTU();

  // begin: start worker task
  void begin(int coreID = -1);

  // Set default timeout value for interface
  void setTimeout(uint32_t TOV);

  // Method to generate an error response - properly enveloped for TCP
  RTUMessage generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode);

protected:
  // addToQueue: send freshly created request to queue
  bool addToQueue(RTURequest *request);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientRTU *instance);

  // receive: get response via Serial
  RTUResponse* receive(RTURequest *request);

  void isInstance() { return; }   // make class instantiable
  queue<RTURequest *> requests;   // Queue to hold requests to be processed
  mutex qLock;                    // Mutex to protect queue
  HardwareSerial& MR_serial;      // Ptr to the serial interface used
  uint32_t MR_lastMicros;         // Microseconds since last bus activity
  uint32_t MR_interval;           // Modbus RTU bus quiet time
  int8_t MR_rtsPin;               // GPIO pin to toggle RS485 DE/RE line. -1 if none.
  uint16_t MR_qLimit;             // Maximum number of requests to hold in the queue
  uint32_t MR_timeoutValue;       // Interface default timeout

};

#endif
