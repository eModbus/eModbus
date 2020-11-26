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

// Service method to read a MSB-first value
template <typename T> uint16_t getValue(uint8_t *target, uint16_t targetLength, T& retval) {
  uint16_t sz = sizeof(retval);    // Size of value to be read
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
public:
  // Default empty message Constructor - optionally takes expected size of MM_data
  explicit ModbusMessage(uint16_t dataLen = 0);

  // Message constructors - internally setMessage() is called
  // WARNING: if parameters are invalid, message will _NOT_ be set up!
  template <typename... Args>
  ModbusMessage(Args&&... args) { // NOLINT
    Error e = SUCCESS;
    if ((e = setMessage(std::forward<Args>(args) ...)) != SUCCESS) {
      printError(e);
    }
  }

  // Destructor
  ~ModbusMessage();

  // Assignment operator
  ModbusMessage& operator=(const ModbusMessage& m);
  
  // Copy constructor
  ModbusMessage(const ModbusMessage& m);

  // Move constructor
	ModbusMessage(ModbusMessage&& m) noexcept = default;
  
	// Move assignment
	ModbusMessage& operator=(ModbusMessage&& m) noexcept = default;

  // Comparison operators
  bool operator==(const ModbusMessage& m);
  bool operator!=(const ModbusMessage& m);
  
  // Exposed methods of std::vector
  const uint8_t   *data();  // address of MM_data
  uint16_t   size();  // used length in MM_data
  const uint8_t    operator[](uint16_t index); // provide restricted operator[] interface
  void push_back(const uint8_t& val); // add a byte at the end of MM_data
  void clear();             // delete message contents

  // provide iterator interface on MM_data
  typedef std::vector<uint8_t>::const_iterator const_iterator;
  const_iterator begin() const { return MM_data.begin(); }
  const_iterator end() const   { return MM_data.end(); }

  // Add append() for two ModbusMessages or a std::vector<uint8_t> to be appended
  void append(ModbusMessage& m);
  void append(std::vector<uint8_t>& m);

  // Modbus data extraction
  uint8_t getServerID();      // returns Server ID or 0 if MM_data is shorter than 3
  uint8_t getFunctionCode();  // returns FC or 0 if MM_data is shorter than 3
  Error   getError();         // getError() - returns error code (MM_data[2], if MM_data[1] > 0x7F, else SUCCESS)

  // add() variant to copy a buffer into MM_data. Returns updated size
  uint16_t add(const uint8_t *arrayOfBytes, uint16_t count);

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

  // Template function to extend add(A) to add(A, B, C, ...)
  template <class T, class... Args> 
  typename std::enable_if<!std::is_pointer<T>::value, uint16_t>::type
  add(T v, Args... args) {
      add(v);
      return add(args...);
  }

// get() - read a MSB-first value starting at byte index. Returns updated index
template <typename T> uint16_t get(uint16_t index, T& retval) {
  uint16_t sz = sizeof(retval);    // Size of value to be read

  retval = 0;                      // return value

  // Will it fit?
  if (index < MM_data.size() - sz) {
    // Yes. Copy it MSB first
    while (sz) {
      sz--;
      retval <<= 8;
      retval |= MM_data[index++];
    }
  }
  return index;
}

  // Message generation methods
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error setMessage(uint8_t serverID, uint8_t functionCode);

  // 2. one uint16_t parameter (FC 0x18)
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // 8. error response
  Error setMessage(uint8_t serverID, uint8_t functionCode, Error errorCode);
  
protected:
  // Data validation methods - used by the above!
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

  // Error output in case a message constructor will fail
  static void printError(Error e);

  std::vector<uint8_t> MM_data;  // Message data buffer
};

#endif
