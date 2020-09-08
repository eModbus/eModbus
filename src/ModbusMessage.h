#ifndef _MODBUS_MESSAGE_H
#define _MODBUS_MESSAGE_H
#include "ModbusTypeDefs.h"

using namespace ModbusClient;

// Definition of the classes for MODBUS messages - Request and Response
// all classes are abstract, a concrete class has to be derived from these.

class ModbusMessage {
public:
  // Service method to fill a given byte array with Modbus MSB-first values. Returns number of bytes written.
 template <class T> uint16_t addValue(uint8_t *target, uint16_t targetLength, T v);

protected:
  // Default Constructor - takes size of MM_data
  ModbusMessage(size_t dataLen);
  
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
  uint8_t   *data();
  
  // len() - return MM_index (used length of MM_data)
  size_t    len();

  // Get MM_data[0] (server ID) and MM_data[1] (function code)
  uint8_t getFunctionCode();  // returns 0 if MM_data is invalid/nullptr
  uint8_t getServerID();      // returns 0 if MM_data is invalid/nullptr
  
  virtual void isInstance() = 0;   // Make this class abstract
  
  uint8_t   *MM_data;            // Message data buffer
  size_t    MM_len;              // Allocated length of MM_data
  uint16_t  MM_index;            // Pointer into MM_data
  
  // add() - add a single data element MSB first to MM_data. Returns updated MM_index or 0
  template <class T> uint16_t add(T v);
  // add() variant to copy a buffer into MM_data. Returns updated MM_index or 0
  uint16_t add(uint16_t count, uint8_t *arrayOfBytes);

};

class ModbusRequest : public ModbusMessage {
protected:
  // Default constructor
  ModbusRequest(size_t dataLen, uint32_t token = 0);

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
  ModbusResponse(size_t dataLen, ModbusRequest *request);

  // getError() - returns error code
  Error getError();

  // setData: fill received data into MM_data
  uint16_t setData(uint16_t dataLen, uint8_t *data);

  ModbusRequest *MRS_request;    // Pointer to the request for this response
  Error MRS_error;             // Error code (0 if ok)
};

#endif