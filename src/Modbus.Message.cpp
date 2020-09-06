#include <string.h>
#include "ModbusMessage.h"

// ****************************************
// ModbusMessage class implementations
// ****************************************
// Service method to fill a given byte array with Modbus MSB-first values. Returns number of bytes written.
template <class T> uint16_t ModbusMessage::addValue(uint8_t *target, uint16_t targetLength, T v) {
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

// Default Constructor - takes size of MM_data to allocate memory
ModbusMessage::ModbusMessage(size_t dataLen) :
  MM_len(dataLen),
  MM_index(0) {
  MM_data = new uint8_t[dataLen];
}

// Destructor - takes care of MM_data deletion
ModbusMessage::~ModbusMessage() {
  if (MM_data) delete[] MM_data;
}

// Assignment operator - take care of MM_data
ModbusMessage& ModbusMessage::operator=(const ModbusMessage& m) {
  // Do anything only if not self-assigning
  if (this != &m) {
    // Size of target the same as size of source?
    if (MM_len != m.MM_len) {
        // No. We need to delete the current memory block
        if (MM_data) delete[] MM_data;
        // Allocate another in the right size
        MM_data = new uint8_t[m.MM_len];
        MM_len = m.MM_len;
    }
    // Copy data from source to target
    memcpy(MM_data, m.MM_data, MM_len);
    // MM_index shall point to the identical byte
    MM_index = m.MM_index;
  }
  return *this;
}

// Copy constructor - take care of MM_data
ModbusMessage::ModbusMessage(const ModbusMessage& m) {
  // Take over m's length and index
  MM_len = m.MM_len;
  MM_index = m.MM_index;
  // Allocate memory for MM_data
  MM_data = new uint8_t[MM_len];
  // Copy data from m
  memcpy(MM_data, m.MM_data, MM_len);
}

// Equality comparison
bool ModbusMessage::operator==(const ModbusMessage& m) {
  // We will compare bytes up to the MM_indexth, the remainder up to MM_len is regardless.
  // If MM_index is different, we assume inequality
  if (MM_index != m.MM_index) return false;
  // If we find a difference byte, we found inequality
  if (memcmp(MM_data, m.MM_data, MM_index)) return false;
  // Both tests passed ==> equality
  return true;
}

// Inequality comparison
bool ModbusMessage::operator!=(const ModbusMessage& m) {
  return (!(*this == m));
}

// data() - return address of MM_data or NULL
uint8_t *ModbusMessage::data() {
  // If we have a memory address and a length, return MM_data
  if (MM_data && MM_len) return MM_data;
  // else return NULL
  return nullptr;
}

// len() - return MM_index (used length of MM_data)
size_t ModbusMessage::len() {
  // If we have a memory address and a length, return MM_index
  if (MM_data && MM_len) return MM_index;
  // else return zero
  return 0;
}

// Get MM_data[0] (server ID) and MM_data[1] (function code)
uint8_t ModbusMessage::getFunctionCode() {
  // Only if we have data and it is at lest as long to fit serverID and function code, return FC
  if (MM_data && MM_index > 1) return MM_data[1];
  // Else return 0 - which is no valid Modbus FC.
  return 0;
}

uint8_t ModbusMessage::getServerID() {
  // Only if we have data and it is at lest as long to fit serverID and function code, return serverID
  if (MM_data && MM_index > 1) return MM_data[0];
  // Else return 0 - normally the Broadcast serverID, but we will not support that. Full stop. :-D
  return 0;
}

// add() - add a single data element MSB first to MM_data. Returns updated MM_index or 0
template <class T> uint16_t ModbusMessage::add(T v) {
  uint16_t sz = sizeof(v);    // Size of value to be added

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
uint16_t ModbusMessage::add(uint16_t count, uint8_t *arrayOfBytes) {
  // Will it fit?
  if (MM_data && count <= (MM_len - MM_index)) {
    // Yes. Copy it
    memcpy(MM_data + MM_index, arrayOfBytes, count);
    MM_index += count;
    // Return updated MM_index (logical length of message so far)
    return MM_index;
  }
  // No, will not fit - return 0
  return 0;
}

// ****************************************
// ModbusRequest class implementations
// ****************************************
// Default constructor
ModbusRequest::ModbusRequest(size_t dataLen, uint32_t token) :
  ModbusMessage(dataLen),
  MRQ_token(token) {}

// check token to find a match
bool ModbusRequest::isToken(uint32_t token) {
  return (token == MRQ_token);
}

// ****************************************
// ModbusResponse class implementations
// ****************************************
// Default constructor
ModbusResponse::ModbusResponse(size_t dataLen, ModbusRequest *request) :
  ModbusMessage(dataLen),
  MRS_request(request),
  MRS_error(SUCCESS) {}

// getError() - returns error code
Error ModbusResponse::getError() {
  // Default: everything OK - SUCCESS
  // Do we have data long enough?
  if (MM_data && MM_index >= 3) {
    // Yes. Does it indicate an error?
    if (MM_data[1] > 0x80)
    {
      // Yes. Get it.
      MRS_error = static_cast<ModbusClient::Error>(MM_data[2]);
    }
  }
  return MRS_error;
}
