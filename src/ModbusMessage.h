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
  // Destructor - takes care of MM_data deletion
  ~ModbusMessage();

  // data() - return address of MM_data or NULL
  uint8_t   *data();
  
  // len() - return MM_index (used length of MM_data)
  size_t    len();

  // Get MM_data[0] (server ID) and MM_data[1] (function code)
  uint8_t getFunctionCode();  // returns 0 if MM_data is invalid/nullptr
  uint8_t getServerID();      // returns 0 if MM_data is invalid/nullptr
  
  // Default Constructor - takes size of MM_data
  ModbusMessage(size_t dataLen);
  
  // Assignment operator - take care of MM_data
  ModbusMessage& operator=(const ModbusMessage& m);
  
  // Copy constructor - take care of MM_data
  ModbusMessage(const ModbusMessage& m);
  
  // Equality comparison
  bool operator==(const ModbusMessage& m);
  
  // Inequality comparison
  bool operator!=(const ModbusMessage& m);
  
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

  // check token to find a match
  bool isToken(uint32_t token);

  uint32_t MRQ_token;            // User defined token to uniquely identify request
};

class ModbusResponse : public ModbusMessage {
protected:
  // Default constructor
  ModbusResponse(size_t dataLen, ModbusRequest *request);

  // getError() - returns error code
  Error getError();

  ModbusRequest *MRS_request;    // Pointer to the request for this response
  Error MRS_error;             // Error code (0 if ok)
};

#endif