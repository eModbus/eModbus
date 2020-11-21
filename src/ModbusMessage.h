// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_MESSAGE_H
#define _MODBUS_MESSAGE_H
#include "ModbusTypeDefs.h"
#include "ModbusError.h"
#include <type_traits>
#include <vector>

using Modbus::Error;
using std::vector;

// Definition of the classes for MODBUS messages - Request and Response
// all classes are abstract, a concrete class has to be derived from these.

// Service method to fill a given byte array with Modbus MSB-first values. Returns number of bytes written.
template <typename T> uint16_t addValue(uint8_t *target, uint16_t targetLength, T v) {
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

// Service method to fill a given std::vector<uint8_t> with Modbus MSB-first values. Returns number of bytes written.
template <typename T> uint16_t addValue(std::vector<uint8_t> target, T v) {
  uint16_t sz = sizeof(v);    // Size of value to be added
  uint16_t index = 0;              // Byte pointer in target

  while (sz) {
    sz--;
    target.push_back((v >> (sz << 3)) & 0xFF);
    index++;
  }
  return index;
}

// Service method to read a MSB-first value
template <typename T> uint16_t getValue(uint8_t *target, uint16_t targetLength, T& retval) {
  uint16_t sz = sizeof(retval);    // Size of value to be added
  uint16_t index = 0;              // Byte pointer in target

  retval = 0;                      // return value

  // Will it fit?
  if (target && sz <= targetLength) {
    // Yes. Copy it MSB first
    while (sz) {
      sz--;
      retval <<= 8;
      retval |= target[index++];
    }
  }
  return index;
}

class ModbusMessage {
protected:
  // Default Constructor - takes expected size of MM_data
  explicit ModbusMessage(uint16_t dataLen = 0);
  
  // Destructor
  virtual ~ModbusMessage();

  // Assignment operator - take care of MM_data
  ModbusMessage& operator=(const ModbusMessage& m);
  
  // Copy constructor - take care of MM_data
  ModbusMessage(const ModbusMessage& m);

  // Move constructor
	ModbusMessage(ModbusMessage&& m) noexcept;
  
	// Move assignment
	ModbusMessage& operator=(ModbusMessage&& m) noexcept;

  // Equality comparison
  bool operator==(const ModbusMessage& m);
  
  // Inequality comparison
  bool operator!=(const ModbusMessage& m);
  
  // data() - return address of MM_data
  const uint8_t   *data() { return MM_data.data(); }
  
  // len() - return used length in MM_data
  // size() - return used length in MM_data
  uint16_t    len()  { return MM_data.size(); }
  uint16_t    size() { return MM_data.size(); }

  // Export std::vector functions
  void push_back(const uint8_t& val) { MM_data.push_back(val); }

  // Get MM_data[0] (server ID) and MM_data[1] (function code)
  uint8_t getFunctionCode();  // returns 0 if MM_data is shorter than 3
  uint8_t getServerID();      // returns 0 if MM_data is shorter than 3
  
  virtual void isInstance() = 0;   // Make this class abstract
  
  // add() - add a single data element MSB first to MM_data. Returns updated size
  template <class T> uint16_t add(T v) {
    uint16_t sz = sizeof(T);    // Size of value to be added

    // Copy it MSB first
    while (sz) {
      sz--;
      MM_data.push_back((v >> (sz << 3)) & 0xFF);
    }
    // Return updated size (logical length of message so far)
    return MM_data.size();
  }

  // add() variant to copy a buffer into MM_data. Returns updated MM_index or 0
  uint16_t add(uint8_t *arrayOfBytes, uint16_t count);

  // Template function to extend add(A) to add(A, B, C, ...)
  template <class T, class... Args> 
  typename std::enable_if<!std::is_pointer<T>::value, uint16_t>::type
  add(T v, Args... args) {
      return add(v) + add(args...);
  }

private:
  std::vector<uint8_t> MM_data;  // Message data buffer
};

class ModbusRequest : public ModbusMessage {
protected:
  // Default constructor
  explicit ModbusRequest(uint16_t dataLen = 0, uint32_t token = 0);

  // Assignment operator - take care of MRQ_token
  ModbusRequest& operator=(const ModbusRequest& m);
  
  // Copy constructor - take care of RQ_token
  ModbusRequest(const ModbusRequest& m);

  // Move constructor
	ModbusRequest(ModbusRequest&& m) noexcept;
  
	// Move assignment
	ModbusRequest& operator=(ModbusRequest&& m) noexcept;

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

private:
  uint32_t MRQ_token;            // User defined token to uniquely identify request
};

class ModbusResponse : public ModbusMessage {
protected:
  // Default constructor
  explicit ModbusResponse(uint16_t dataLen = 0);

  // Assignment operator - take care of MRS_error
  ModbusResponse& operator=(const ModbusResponse& m);
  
  // Copy constructor - take care of MRS_error
  ModbusResponse(const ModbusResponse& m);

  // Move constructor
	ModbusResponse(ModbusResponse&& m) noexcept;
  
	// Move assignment
	ModbusResponse& operator=(ModbusResponse&& m) noexcept;

  // getError() - returns error code
  Error getError();

private:
  Error MRS_error;             // Error code (0 if ok)
};

#endif
