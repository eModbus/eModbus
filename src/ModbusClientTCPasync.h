#ifndef _MODBUS_TCPASYNC_H
#define _MODBUS_TCPASYNC_H
#include <Arduino.h>
#include <AsyncTCP.h>
#include "ModbusMessageTCP.h"
#include "ModbusClient.h"
#include <list>
#include <vector>
#include <mutex>

using std::list;
using std::mutex;
using std::lock_guard;
using std::vector;

#define DEFAULTTIMEOUT 10000
#define DEFAULTIDLETIME 60000

class ModbusClientTCPasync : public ModbusClient {
public:
  // Constructor takes address and port
  explicit ModbusClientTCPasync(IPAddress address, uint16_t port = 502, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientTCPasync();

  // optionally manually connect to modbus server. Otherwise connection will be made upon first request
  void connect();

  // manually disconnect from modbus server. Connection will also auto close after idle time
  void disconnect(bool force = false);

  // Set timeout value
  void setTimeout(uint32_t timeout);

  // Set idle timeout value (time before connection auto closes after being idle)
  void setIdleTimeout(uint32_t timeout);

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  vector<uint8_t> generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

protected:
// makeHead: helper function to set up a MSB TCP header
  bool makeHead(uint8_t *data, uint16_t dataLen, uint16_t TID, uint16_t PID, uint16_t LEN);

  // addToQueue: send freshly created request to queue
  bool addToQueue(TCPRequest *request);

  // send: send request via Client connection
  bool send(TCPRequest *request);

  // receive: get response via Client connection
  // TCPResponse* receive(uint8_t* data, size_t length);

  // Create standard error response 
  TCPResponse* errorResponse(Error e, TCPRequest *request);

  // Move complete message data including tcpHead into a std::vector
  vector<uint8_t> vectorize(uint16_t transactionID, TCPRequest *request, Error err);

  void isInstance() { return; }     // make class instantiable

  // TCP handling code, all static taking a class instancs as param
  void onConnected();
  void onDisconnected();
  // void onError(int8_t error);
  // void onTimeout(uint32_t time);
  // void onAck(size_t len, uint32_t time);
  void onPacket(uint8_t* data, size_t length);
  void onPoll();
  void handleSendingQueue();

  list<TCPRequest*> txQueue;        // Queue to hold requests to be sent
  list<TCPRequest*> rxQueue;        // Queue to hold requests to be processed
  mutex sLock;                      // Mutex to protect state
  mutex qLock;                      // Mutex to protect queues
  AsyncClient MTA_client;           // Async TCP client
  uint32_t MTA_timeout;             // Standard timeout value taken
  uint32_t MTA_idleTimeout;         // Standard timeout value taken
  uint16_t MTA_qLimit;              // Maximum number of requests to accept in queue
  uint32_t MTA_lastActivity;        // Last time there was activity (disabled when queues are not empty)
  enum {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
  } MTA_state;                      // TCP connection state
  TargetHost MTA_target;            // dummy target
};

#endif
