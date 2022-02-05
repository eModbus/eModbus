// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_RTU_H
#define _MODBUS_CLIENT_RTU_H

#include "options.h"

#if HAS_FREERTOS

#include "ModbusClient.h"
#include "HardwareSerial.h"
#include "RTUutils.h"
#include <queue>
#include <vector>

using std::queue;

#define DEFAULTTIMEOUT 2000

class ModbusClientRTU : public ModbusClient {
public:
  // Constructor takes Serial reference and optional DE/RE pin and queue limit
  explicit ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin = -1, uint16_t queueLimit = 100);

  // Alternative Constructor takes Serial reference and RTS line toggle callback
  explicit ModbusClientRTU(HardwareSerial& serial, RTScallback rts, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientRTU();

  // begin: start worker task
  void begin(int coreID = -1, uint32_t interval = 0);

  // end: stop the worker
  void end();

  // Set default timeout value for interface
  void setTimeout(uint32_t TOV);

  // Toggle protocol to ModbusASCII
  void useModbusASCII(unsigned long timeout = 1000);

  // Toggle protocol to ModbusRTU
  void useModbusRTU();

  // Inquire protocol mode
  bool isModbusASCII();

protected:
  struct RequestEntry {
    uint32_t token;
    ModbusMessage msg;
    bool isSyncRequest;
    RequestEntry(uint32_t t, ModbusMessage m, bool syncReq = false) :
      token(t),
      msg(m),
      isSyncRequest(syncReq) {}
  };

  // Base addRequest and syncRequest must be present
  Error addRequestM(ModbusMessage msg, uint32_t token);
  ModbusMessage syncRequestM(ModbusMessage msg, uint32_t token);

  // addToQueue: send freshly created request to queue
  bool addToQueue(uint32_t token, ModbusMessage msg, bool syncReq = false);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientRTU *instance);

  // receive: get response via Serial
  ModbusMessage receive(const ModbusMessage request);

  void isInstance() { return; }   // make class instantiable
  queue<RequestEntry> requests;   // Queue to hold requests to be processed
  #if USE_MUTEX
  mutex qLock;                    // Mutex to protect queue
  #endif
  HardwareSerial& MR_serial;      // Ptr to the serial interface used
  unsigned long MR_lastMicros;    // Microseconds since last bus activity
  uint32_t MR_interval;           // Modbus RTU bus quiet time
  int8_t MR_rtsPin;               // GPIO pin to toggle RS485 DE/RE line. -1 if none.
  RTScallback MTRSrts;            // RTS line callback function
  uint16_t MR_qLimit;             // Maximum number of requests to hold in the queue
  uint32_t MR_timeoutValue;       // Interface default timeout
  bool MR_useASCII;               // true=ModbusASCII, false=ModbusRTU

};

#endif  // HAS_FREERTOS

#endif  // INCLUDE GUARD
