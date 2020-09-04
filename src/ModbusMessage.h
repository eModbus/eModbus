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
  
  // len() - return length of MM_data
  size_t    len();
  
  // getServerID() - return MM_serverID or 0
  uint8_t   getServerID();
  
  // getFunctionCode() - return fc or 0
  uint8_t   getFunctionCode();
  
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
  uint8_t   MM_serverID;          // server id from message - MM_data[0]
  uint8_t   MM_functionCode;     // function code from message - MM_data[1]
  
  // add() - add a single data element MSB first to MM_data. Returns updated MM_index or 0
  template <class T> uint16_t add(T v);
  // add() variant to copy a buffer into MM_data. Returns updated MM_index or 0
  uint16_t add(uint16_t count, uint8_t *arrayOfBytes);

};

class ModbusRequest : public ModbusMessage {
protected:
  // Default constructor
  ModbusRequest(uint32_t token = 0);

  // getToken() - return this request's token value
  uint32_t getToken();

  uint32_t MRQ_token;            // User defined token to uniquely identify request
  uint32_t MRQ_timeout;          // timeout value waiting for response

};

class ModbusResponse : public ModbusMessage {
protected:
  // getError() - returns error code
  uint8_t getError();

  ModbusRequest *MRS_request;    // Pointer to the request for this response
  uint8_t MRS_error;             // Error code (0 if ok)
};

#endif