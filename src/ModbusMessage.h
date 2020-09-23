// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_MESSAGE_H
#define _MODBUS_MESSAGE_H
#include "ModbusTypeDefs.h"
#include "ModbusError.h"

using ModbusClient::Error;

// Definition of the classes for MODBUS messages - Request and Response
// all classes are abstract, a concrete class has to be derived from these.

class ModbusMessage {
public:
// Service method to fill a given byte array with Modbus MSB-first values. Returns number of bytes written.
template <class T> static uint16_t addValue(uint8_t *target, uint16_t targetLength, T v) {
  uint16_t sz = sizeof(v);    // Size of value to be added
  uint16_t index = 0;         // Byte pointer in target

  // Will it fit?
  if (target && sz <= targetLength) {
    // Yes. Copy it MSB first
    while (sz) {
      sz--;
      target[index++] = (v >> (sz << 3)) & 0xFF;
    }
  }
  return index;
}

protected:
  // Default Constructor - takes size of MM_data
  explicit ModbusMessage(uint16_t dataLen);
  
  // Destructor - takes care of MM_data deletion
  virtual ~ModbusMessage();

  // Assignment operator - take care of MM_data
  ModbusMessage& operator=(const ModbusMessage& m);
  
  // Copy constructor - take care of MM_data
  ModbusMessage(const ModbusMessage& m);
  
  // Equality comparison
  bool operator==(const ModbusMessage& m);
  
  // Inequality comparison
  bool operator!=(const ModbusMessage& m);
  
  // data() - return address of MM_data or NULL
  const uint8_t   *data();
  
  // len() - return MM_index (used length of MM_data)
  uint16_t    len();

  // Get MM_data[0] (server ID) and MM_data[1] (function code)
  uint8_t getFunctionCode();  // returns 0 if MM_data is invalid/nullptr
  uint8_t getServerID();      // returns 0 if MM_data is invalid/nullptr
  
  virtual void isInstance() = 0;   // Make this class abstract

#ifdef TESTING
  void dump(const char *header);
#endif
  
  uint8_t   *MM_data;            // Message data buffer
  uint16_t    MM_len;            // Allocated length of MM_data
  uint16_t  MM_index;            // Pointer into MM_data
  
  // add() - add a single data element MSB first to MM_data. Returns updated MM_index or 0
  template <class T> uint16_t add(T v) {
    uint16_t sz = sizeof(T);    // Size of value to be added

    // Will it fit?
    if (MM_data && sz <= (MM_len - MM_index)) {
      // Yes. Copy it MSB first
      while (sz) {
        sz--;
        MM_data[MM_index++] = (v >> (sz << 3)) & 0xFF;
      }
      // Return updated MM_index (logical length of message so far)
      return MM_index;
    }
    // No, will not fit - return 0
    return 0;
  }

  // add() variant to copy a buffer into MM_data. Returns updated MM_index or 0
  uint16_t add(uint16_t count, uint8_t *arrayOfBytes);

};

class ModbusRequest : public ModbusMessage {
protected:
  // Default constructor
  explicit ModbusRequest(uint16_t dataLen, uint32_t token = 0);

  // Get token
  uint32_t getToken();

  // check token to find a match
  bool isToken(uint32_t token);

  // Data validation methods for the different factory calls
  // 0. serverID and function code - used by all of the below
  static Error checkServerFC(uint8_t serverID, uint8_t functionCode);

  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  static Error checkData(uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  static Error checkData(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  uint32_t MRQ_token;            // User defined token to uniquely identify request
};

class ModbusResponse : public ModbusMessage {
protected:
  // Default constructor
  explicit ModbusResponse(uint16_t dataLen);

  // getError() - returns error code
  Error getError();

  Error MRS_error;             // Error code (0 if ok)
};

#endif
