#ifndef _MODBUS_TCP_H
#define _MODBUS_TCP_H
#include <Arduino.h>
#include "ModbusMessageTCP.h"
#include "PhysicalInterface.h"
#include "Client.h"
#include <queue>
#include <mutex>

using std::queue;
using std::mutex;
using std::lock_guard;

class ModbusTCP : public PhysicalInterface {
public:
  // Constructor takes reference to Client (EthernetClient or WiFiClient)
  ModbusTCP(Client& client, uint16_t queueLimit = 100);

  // Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
  ModbusTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusTCP();

  // begin: start worker task
  void begin(int coreID = -1);

  // Switch target host (if necessary)
  bool setTarget(IPAddress host, uint16_t port);

  // Set pause time between two consecutive requests to the same target host
  void setSameHostInterval(uint32_t intervalMs);

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

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

  void isInstance() { return; }   // make class instantiable
  queue<TCPRequest *> requests;   // Queue to hold requests to be processed
  mutex qLock;                    // Mutex to protect queue
  Client& MT_client;              // Client reference for Internet connections (EthernetClient or WifiClient)
  IPAddress MT_lastHost;          // target host from previous request processed - to keep connection open if host/port stays the same
  uint16_t MT_lastPort;           // target port from previous request processed - to keep connection open if host/port stays the same
  IPAddress MT_targetHost;        // target host for next request(s) to be processed
  uint16_t MT_targetPort;         // target port for next request(s) to be processed
  uint16_t MT_qLimit;             // Maximum number of requests to accept in queue
  uint32_t MT_sameHostInterval;   // Time in ms to wait between consecutive connections to the same target host
};

#endif