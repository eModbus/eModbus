// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_TCP_H
#define _MODBUS_TCP_H
#include <Arduino.h>
#include "ModbusMessageTCP.h"
#include "PhysicalInterface.h"
#include "Client.h"
#include <queue>
#include <vector>
#include <mutex>                    // NOLINT

using std::queue;
using std::mutex;
using std::lock_guard;
using TCPMessage = std::vector<uint8_t>;

#define TARGETHOSTINTERVAL 10
#define DEFAULTTIMEOUT 2000

class ModbusTCP : public PhysicalInterface {
public:
  // Constructor takes reference to Client (EthernetClient or WiFiClient)
  explicit ModbusTCP(Client& client, uint16_t queueLimit = 100);

  // Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
  ModbusTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusTCP();

  // begin: start worker task
  void begin(int coreID = -1);

  // Set default timeout value (and interval)
  void setTimeout(uint32_t timeout, uint32_t interval = TARGETHOSTINTERVAL);

  // Switch target host (if necessary)
  bool setTarget(IPAddress host, uint16_t port, uint32_t timeout = 0, uint32_t interval = 0);

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // Method to generate an error response - properly enveloped for TCP
  TCPMessage generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode);

protected:
// makeHead: helper function to set up a MSB TCP header
  bool makeHead(uint8_t *data, uint16_t dataLen, uint16_t TID, uint16_t PID, uint16_t LEN);

  // addToQueue: send freshly created request to queue
  bool addToQueue(TCPRequest *request);

  // handleConnection: worker task method
  static void handleConnection(ModbusTCP *instance);

  // send: send request via Client connection
  void send(TCPRequest *request);

  // receive: get response via Client connection
  TCPResponse* receive(TCPRequest *request);

  // Create standard error response 
  TCPResponse* errorResponse(Error e, TCPRequest *request);

  // Move complete message data including tcpHead into a std::vector
  TCPMessage vectorize(uint16_t transactionID, TCPRequest *request, Error err);

  void isInstance() { return; }   // make class instantiable
  queue<TCPRequest *> requests;   // Queue to hold requests to be processed
  mutex qLock;                    // Mutex to protect queue
  Client& MT_client;              // Client reference for Internet connections (EthernetClient or WifiClient)
  TargetHost MT_lastTarget;       // last used server
  TargetHost MT_target;           // Description of target server
  uint32_t MT_defaultTimeout;     // Standard timeout value taken if no dedicated was set
  uint32_t MT_defaultInterval;    // Standard interval value taken if no dedicated was set
  uint16_t MT_qLimit;             // Maximum number of requests to accept in queue
};

#endif
