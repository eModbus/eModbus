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

// Data validation methods for the different factory calls
// 0. serverID and function code - used by all of the below
Error ModbusRequest::checkServerFC(uint8_t serverID, uint8_t functionCode) {
  if (serverID == 0)      return INVALID_SERVER;   // Broadcast - not supported here
  if (serverID > 247)     return INVALID_SERVER;   // Reserved server addresses
  if (functionCode == 0)  return INVALID_FUNCTION; // FC 0 does not exist
  if (functionCode == 9)  return INVALID_FUNCTION; // FC 9 does not exist
  if (functionCode == 10) return INVALID_FUNCTION; // FC 10 does not exist
  if (functionCode == 13) return INVALID_FUNCTION; // FC 13 does not exist
  if (functionCode == 14) return INVALID_FUNCTION; // FC 14 does not exist
  if (functionCode == 18) return INVALID_FUNCTION; // FC 18 does not exist
  if (functionCode == 19) return INVALID_FUNCTION; // FC 19 does not exist
  if (functionCode > 127) return INVALID_FUNCTION; // FC only defined up to 127
  return SUCCESS;
}

// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    [[fallthrough]] case 0x02:
    [[fallthrough]] case 0x03:
    [[fallthrough]] case 0x04:
    [[fallthrough]] case 0x05:
    [[fallthrough]] case 0x06:
    // [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    // [[fallthrough]] case 0x0b:
    // [[fallthrough]] case 0x0c:
    [[fallthrough]] case 0x0f:
    [[fallthrough]] case 0x10:
    // [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 2. one uint16_t parameter (FC 0x18)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    [[fallthrough]] case 0x02:
    [[fallthrough]] case 0x03:
    [[fallthrough]] case 0x04:
    [[fallthrough]] case 0x05:
    [[fallthrough]] case 0x06:
    [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    [[fallthrough]] case 0x0b:
    [[fallthrough]] case 0x0c:
    [[fallthrough]] case 0x0f:
    [[fallthrough]] case 0x10:
    [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    // [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    // 0x18: any value acceptable
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    case 0x02:
      if ((p2 > 0x7d0) || (p2 == 0)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    [[fallthrough]] case 0x03:
    case 0x04:
      if ((p2 > 0x7d) || (p2 == 0)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    case 0x05:
      if ((p2 != 0) && (p2 != 0xff)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    // case 0x06: all values are acceptable for p1 and p2
    [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    [[fallthrough]] case 0x0b:
    [[fallthrough]] case 0x0c:
    [[fallthrough]] case 0x0f:
    [[fallthrough]] case 0x10:
    [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 4. three uint16_t parameters (FC 0x16)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    [[fallthrough]] case 0x02:
    [[fallthrough]] case 0x03:
    [[fallthrough]] case 0x04:
    [[fallthrough]] case 0x05:
    [[fallthrough]] case 0x06:
    [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    [[fallthrough]] case 0x0b:
    [[fallthrough]] case 0x0c:
    [[fallthrough]] case 0x0f:
    [[fallthrough]] case 0x10:
    [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    // [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    // 0x18: any value acceptable for p1, p2 and p3
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    [[fallthrough]] case 0x02:
    [[fallthrough]] case 0x03:
    [[fallthrough]] case 0x04:
    [[fallthrough]] case 0x05:
    [[fallthrough]] case 0x06:
    [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    [[fallthrough]] case 0x0b:
    [[fallthrough]] case 0x0c:
    [[fallthrough]] case 0x0f:
    // [[fallthrough]] case 0x10:
    [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    case 0x10:
      if ((p2 == 0) || (p2 > 0x7b)) returnCode = PARAMETER_LIMIT_ERROR;
      else if (count != (p2 * 2)) returnCode = ILLEGAL_DATA_VALUE;
      break;
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes) {
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    [[fallthrough]] case 0x01:
    [[fallthrough]] case 0x02:
    [[fallthrough]] case 0x03:
    [[fallthrough]] case 0x04:
    [[fallthrough]] case 0x05:
    [[fallthrough]] case 0x06:
    [[fallthrough]] case 0x07:
    [[fallthrough]] case 0x08:
    [[fallthrough]] case 0x0b:
    [[fallthrough]] case 0x0c:
    // [[fallthrough]] case 0x0f:
    [[fallthrough]] case 0x10:
    [[fallthrough]] case 0x11:
    [[fallthrough]] case 0x14:
    [[fallthrough]] case 0x15:
    [[fallthrough]] case 0x16:
    [[fallthrough]] case 0x17:
    [[fallthrough]] case 0x18:
    case 0x2b:
      returnCode = PARAMETER_COUNT_ERROR;
      break;
    case 0x0f:
      if ((p2 == 0) || (p2 > 0x7b0)) returnCode = PARAMETER_LIMIT_ERROR;
      else if (count != ((p2 / 8 + (p2 % 8 ? 1 : 0)))) returnCode = ILLEGAL_DATA_VALUE;
      break;
    default:
      returnCode = SUCCESS;
      break;
    }
  }
  return returnCode;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
Error ModbusRequest::checkData(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes) {
  return checkServerFC(serverID, functionCode);
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

  // setData: fill received data into MM_data as is
  uint16_t ModbusResponse::setData(uint16_t dataLen, uint8_t *data) {
    // Regardless waht was in MM_data before - discard it
    MM_index = 0;
    // Does data fit into MM_data?
    if (dataLen <= MM_len) {
      // Yes. Copy it.
      memcpy(MM_data, data, dataLen);
      MM_index = dataLen;
    }
    return MM_index;
  }