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
  RTURequest *r = RTURequest::createRTURequest(rc, std::forward<Args>(args) ...);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
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

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  // Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  // RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // Method to generate an error response - properly enveloped for TCP
  RTUMessage generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode);

protected:
  // addToQueue: send freshly created request to queue
  bool addToQueue(RTURequest *request);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientRTU *instance);

  // receive: get response via Serial
  RTUResponse* receive(RTURequest *request);

  // Move complete message data including CRC into a std::vector
  RTUMessage vectorize(RTURequest *request, Error err);

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
