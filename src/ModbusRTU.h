#ifndef _MODBUS_RTU_H
#define _MODBUS_RTU_H
#include "ModbusMessageRTU.h"
#include "PhysicalInterface.h"
#include "HardwareSerial.h"
#include <queue>
#include <mutex>
#include <vector>

using std::queue;
using std::mutex;
using std::lock_guard;
using std::vector;

#define DEFAULTTIMEOUT 2000

class ModbusRTU : public PhysicalInterface {
public:
  // Constructor takes Serial reference and optional DE/RE pin and queue limit
  ModbusRTU(HardwareSerial& serial, int8_t rtsPin = -1, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusRTU();

  // begin: start worker task
  void begin(int coreID = -1);

  // Set default timeout value for interface
  void setTimeout(uint32_t TOV);

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // Method to generate an error response - properly enveloped for TCP
  vector<uint8_t> generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode);

protected:
  // addToQueue: send freshly created request to queue
  bool addToQueue(RTURequest *request);

  // handleConnection: worker task method
  static void handleConnection(ModbusRTU *instance);

  // send: send request via Serial
  void send(RTURequest *request);

  // receive: get response via Serial
  RTUResponse* receive(RTURequest *request);

  // Move complete message data including CRC into a std::vector
  vector<uint8_t> vectorize(RTURequest *request, Error err);

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