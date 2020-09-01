#ifndef _MODBUS_MESSAGE_H
#define _MODBUS_MESSAGE_H
#include <stdint.h>
#include <stddef.h>

// Definition of the classes for MODBUS messages - Request and Response
// all classes are abstract, a concrete class has to be derived from these.

class ModbusMessage {
public:
  // Constructor - takes size of MM_data
  ModbusMessage(size_t dataLen);
  // Destructor - takes care of MM_data deletion
  ~ModbusMessage();
  // data() - return address of MM_data or NULL
  uint8_t   *data();
  // len() - return length of MM_data
  size_t    len();
  // getSlaveID() - return MM_slaveID or 0
  uint8_t   getSlaveID();
  // getFunctionCode() - return fc or 0
  uint8_t   getFunctionCode();
  // resizeData() - delete MM_data, if allocated and allocate a new MM_data size. Copy old contents, if copy==true
  bool      resizeData(size_t dataLen, bool copy = false);
  // Assignment operator - take care of MM_data
  ModbusMessage& operator=(const ModbusMessage& m);
  // Copy constructor - take care of MM_data
  ModbusMessage(const ModbusMessage& m);
  // Equality comparison
  bool operator==(const ModbusMessage& m);
  // Inequality comparison
  bool operator!=(const ModbusMessage& m);
  // add8() - add a single byte to MM_data. Returns updated MM_index or 0
  uint16_t add8(uint8_t c);
  // add16() - add two bytes MSB first to MM_data. Returns updated MM_index or 0
  uint16_t add16(uint16_t s);
  // add32() - add four bytes MSB first to MM_data. Returns updated MM_index or 0
  uint16_t add32(uint32_t w);
protected:
  virtual void isInstance() = 0;   // Make this class abstract
  uint8_t   *MM_data;            // Message data buffer
  size_t    MM_len;              // Allocated length of MM_data
  uint16_t  MM_index;            // Pointer into MM_data
  uint8_t   MM_slaveID;          // slave id from message - MM_data[0]
  uint8_t   MM_functionCode;     // function code from message - MM_data[1]
};

class ModbusRequest : public ModbusMessage {
public:
  // getToken() - return this request's token value
  uint32_t getToken();
protected:
  uint32_t MRQ_token;            // User defined token to uniquely identify request
  uint32_t MRQ_timeout;          // timeout value waiting for response
};

class ModbusResponse : public ModbusMessage {
public:
  // getError() - returns error code
  uint8_t getError();
protected:
  ModbusRequest *MRS_request;    // Pointer to the request for this response
  uint8_t MRS_error;             // Error code (0 if ok)
};

#endif