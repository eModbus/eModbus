// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <string.h>
#include <stdio.h>
#include <Arduino.h>
#include "ModbusMessage.h"

// ****************************************
// ModbusMessage class implementations
// ****************************************

// Default Constructor - takes optional size of MM_data to allocate memory
ModbusMessage::ModbusMessage(uint16_t dataLen) {
  if (dataLen) MM_data.reserve(dataLen);
}

// Destructor
ModbusMessage::~ModbusMessage() { }

// Assignment operator - take care of MM_data
ModbusMessage& ModbusMessage::operator=(const ModbusMessage& m) {
  // Do anything only if not self-assigning
  if (this != &m) {
    // Copy data from source to target
    MM_data = m.MM_data;
  }
  return *this;
}

// Copy constructor - take care of MM_data
ModbusMessage::ModbusMessage(const ModbusMessage& m) {
  // Copy data from m
  MM_data = m.MM_data;
}

// Move constructor
// Transfer ownership of m.MM_data
ModbusMessage::ModbusMessage(ModbusMessage&& m) noexcept : 
  MM_data(m.MM_data) {
  m.MM_data.clear();
}

// Move assignment
// Transfer ownership of m.MM_data
ModbusMessage& ModbusMessage::operator=(ModbusMessage&& m) noexcept
{
  // Self-assignment detection
  if (&m != this) {
    // Transfer ownership 
    MM_data = m.MM_data;
    m.MM_data.clear();
  }
  return *this;
}

// Equality comparison
bool ModbusMessage::operator==(const ModbusMessage& m) {
  // Prevent self-compare
  if (this == &m) return true;
  // If size is different, we assume inequality
  if (MM_data.size() != m.MM_data.size()) return false;
  // We will compare bytes manually - for uint8_t it should work out-of-the-box,
  // but the data type might be changed later.
  // If we find a difference byte, we found inequality
  for (uint16_t i = 0; i < MM_data.size(); ++i) {
    if (MM_data[i] != m.MM_data[i]) return false;
  }
  // Both tests passed ==> equality
  return true;
}

// Inequality comparison
bool ModbusMessage::operator!=(const ModbusMessage& m) {
  return (!(*this == m));
}

// Get MM_data[0] (server ID) and MM_data[1] (function code)
uint8_t ModbusMessage::getFunctionCode() {
  // Only if we have data and it is at least as long to fit serverID and function code, return FC
  if (MM_data.size() > 2) { return MM_data[1]; }
  // Else return 0 - which is no valid Modbus FC.
  return 0;
}

uint8_t ModbusMessage::getServerID() {
  // Only if we have data and it is at least as long to fit serverID and function code, return serverID
  if (MM_data.size() > 2) { return MM_data[0]; }
  // Else return 0 - normally the Broadcast serverID, but we will not support that. Full stop. :-D
  return 0;
}

// add() variant to copy a buffer into MM_data. Returns updated size
uint16_t ModbusMessage::add(uint8_t *arrayOfBytes, uint16_t count) {
  // Copy it
  while (count--) {
    MM_data.push_back(*arrayOfBytes++);
  }
  // Return updated size (logical length of message so far)
  return MM_data.size();
}

// ****************************************
// ModbusRequest class implementations
// ****************************************
// Default constructor
ModbusRequest::ModbusRequest(uint16_t dataLen, uint32_t token) :
  ModbusMessage(dataLen),
  MRQ_token(token) {}

// Assignment operator - take care of MRQ_token
ModbusRequest& ModbusRequest::operator=(const ModbusRequest& m) {
  // Self-assignment detection
  if (&m != this) {
    // Transfer ownership
    ModbusMessage::operator=(m);
    MRQ_token = m.MRQ_token;
  }
  return *this;
}

// Copy constructor - take care of MRQ_token
ModbusRequest::ModbusRequest(const ModbusRequest& m) : 
  ModbusMessage(m),
  MRQ_token(m.MRQ_token) {
}

// Move constructor
// Transfer ownership of m.MM_data
ModbusRequest::ModbusRequest(ModbusRequest&& m) noexcept : 
  ModbusMessage(m), 
  MRQ_token(m.MRQ_token)
{
  m.MRQ_token = 0;
}

// Move assignment
ModbusRequest& ModbusRequest::operator=(ModbusRequest&& m) noexcept
{
  // Self-assignment detection
  if (&m != this) {
    // Transfer ownership
    ModbusMessage::operator=(m);
    MRQ_token = m.MRQ_token;
    m.MRQ_token = 0;
  }
  return *this;
}

// Get token
uint32_t ModbusRequest::getToken() {
  return MRQ_token;
}

// check token to find a match
bool ModbusRequest::isToken(uint32_t token) {
  return (token == MRQ_token);
}

// Data validation methods for the different factory calls
// 0. serverID and function code - used by all of the below
Error ModbusRequest::checkServerFC(uint8_t serverID, uint8_t functionCode) {
  if (serverID == 0)      return INVALID_SERVER;   // Broadcast - not supported here
  if (serverID > 247)     return INVALID_SERVER;   // Reserved server addresses
  if (functionCode == 0)  return ILLEGAL_FUNCTION; // FC 0 does not exist
  if (functionCode == 9)  return ILLEGAL_FUNCTION; // FC 9 does not exist
  if (functionCode == 10) return ILLEGAL_FUNCTION; // FC 10 does not exist
  if (functionCode == 13) return ILLEGAL_FUNCTION; // FC 13 does not exist
  if (functionCode == 14) return ILLEGAL_FUNCTION; // FC 14 does not exist
  if (functionCode == 18) return ILLEGAL_FUNCTION; // FC 18 does not exist
  if (functionCode == 19) return ILLEGAL_FUNCTION; // FC 19 does not exist
  if (functionCode > 127) return ILLEGAL_FUNCTION; // FC only defined up to 127
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
      if ((p2 != 0) && (p2 != 0xff00)) returnCode = PARAMETER_LIMIT_ERROR;
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
ModbusResponse::ModbusResponse(uint16_t dataLen) :
  ModbusMessage(dataLen),
  MRS_error(SUCCESS) {}

// Assignment operator - take care of MRS_error
ModbusResponse& ModbusResponse::operator=(const ModbusResponse& m) {
  // Self-assignment detection
  if (&m != this) {
    // Transfer ownership
    ModbusMessage::operator=(m);
    MRS_error = m.MRS_error;
  }
  return *this;
}

// Copy constructor - take care of MRS_error
ModbusResponse::ModbusResponse(const ModbusResponse& m) :
  ModbusMessage(m),
  MRS_error(m.MRS_error) {
}

// Move constructor
// Transfer ownership of m.MM_data
ModbusResponse::ModbusResponse(ModbusResponse&& m) noexcept :
  ModbusMessage(m), 
  MRS_error(m.MRS_error) {
  m.MRS_error = SUCCESS;
}

// Move assignment
ModbusResponse& ModbusResponse::operator=(ModbusResponse&& m) noexcept
{
  // Self-assignment detection
  if (&m != this) {
    // Transfer ownership
    ModbusMessage::operator=(m);
    MRS_error = m.MRS_error;
    m.MRS_error = SUCCESS;
  }
  return *this;
}

// getError() - returns error code
Error ModbusResponse::getError() {
  // Default: everything OK - SUCCESS
  // Do we have data long enough?
  if (size() > 2) {
    // Yes. Does it indicate an error?
    if (getFunctionCode() > 0x80)
    {
      // Yes. Get it.
      MRS_error = static_cast<Modbus::Error>(data()[2]);
    }
  }
  return MRS_error;
}

