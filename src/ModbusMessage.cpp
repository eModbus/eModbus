// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "ModbusMessage.h"
#undef LOCAL_LOG_LEVEL
// #define LOCAL_LOG_LEVEL LOG_LEVEL_ERROR
#include "Logging.h"

// Default Constructor - takes optional size of MM_data to allocate memory
ModbusMessage::ModbusMessage(uint16_t dataLen) {
  if (dataLen) MM_data.reserve(dataLen);
}

// Special message Constructor - takes a std::vector<uint8_t>
ModbusMessage::ModbusMessage(std::vector<uint8_t> s) :
MM_data(s) { }

// Destructor
ModbusMessage::~ModbusMessage() { 
  // If paranoid, one can use the below :D
  // std::vector<uint8_t>().swap(MM_data);
}

// Assignment operator
ModbusMessage& ModbusMessage::operator=(const ModbusMessage& m) {
  // Do anything only if not self-assigning
  if (this != &m) {
    // Copy data from source to target
    MM_data = m.MM_data;
  }
  return *this;
}

#ifndef NO_MOVE
  // Move constructor
ModbusMessage::ModbusMessage(ModbusMessage&& m) {
  MM_data = std::move(m.MM_data);
}
  
	// Move assignment
ModbusMessage& ModbusMessage::operator=(ModbusMessage&& m) {
  MM_data = std::move(m.MM_data);
  return *this;
}
#endif

// Copy constructor
ModbusMessage::ModbusMessage(const ModbusMessage& m) :
  MM_data(m.MM_data) { }

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

// Conversion to bool
ModbusMessage::operator bool() {
  if (MM_data.size() >= 2) return true;
  return false;
}

// Exposed methods of std::vector
const uint8_t *ModbusMessage::data() { return MM_data.data(); }
uint16_t       ModbusMessage::size() { return MM_data.size(); }
void           ModbusMessage::push_back(const uint8_t& val) { MM_data.push_back(val); }
void           ModbusMessage::clear() { MM_data.clear(); }
// provide restricted operator[] interface
const uint8_t  ModbusMessage::operator[](uint16_t index) {
  if (index < MM_data.size()) {
    return MM_data[index];
  }
  LOG_W("Index %d out of bounds (>=%d).\n", index, MM_data.size());
  return 0;
}
// Resize internal MM_data
uint16_t ModbusMessage::resize(uint16_t newSize) { 
  MM_data.resize(newSize); 
  return MM_data.size(); 
}

// Add append() for two ModbusMessages or a std::vector<uint8_t> to be appended
void ModbusMessage::append(ModbusMessage& m) { 
  MM_data.reserve(size() + m.size()); 
  MM_data.insert(MM_data.end(), m.begin(), m.end()); 
}

void ModbusMessage::append(std::vector<uint8_t>& m) { 
  MM_data.reserve(size() + m.size()); 
  MM_data.insert(MM_data.end(), m.begin(), m.end()); 
}

uint8_t ModbusMessage::getServerID() {
  // Only if we have data and it is at least as long to fit serverID and function code, return serverID
  if (MM_data.size() >= 2) { return MM_data[0]; }
  // Else return 0 - normally the Broadcast serverID, but we will not support that. Full stop. :-D
  return 0;
}

// Get MM_data[0] (server ID) and MM_data[1] (function code)
uint8_t ModbusMessage::getFunctionCode() {
  // Only if we have data and it is at least as long to fit serverID and function code, return FC
  if (MM_data.size() >= 2) { return MM_data[1]; }
  // Else return 0 - which is no valid Modbus FC.
  return 0;
}

// getError() - returns error code
Error ModbusMessage::getError() {
  // Do we have data long enough?
  if (MM_data.size() > 2) {
    // Yes. Does it indicate an error?
    if (MM_data[1] & 0x80)
    {
      // Yes. Get it.
      return static_cast<Modbus::Error>(MM_data[2]);
    }
  }
  // Default: everything OK - SUCCESS
  return SUCCESS;
}

// Modbus data manipulation
void    ModbusMessage::setServerID(uint8_t serverID) {
  // We accept here that [0] may allocate a byte!
  MM_data[0] = serverID;
}

void    ModbusMessage::setFunctionCode(uint8_t FC) {
  // We accept here that [0], [1] may allocate bytes!
  // No serverID set yet? use a 0 to initialize it to an error-generating value
  if (MM_data.size() < 2) MM_data[0] = 0; // intentional invalid server ID!
  MM_data[1] = FC;
}

// add() variant to copy a buffer into MM_data. Returns updated size
uint16_t ModbusMessage::add(const uint8_t *arrayOfBytes, uint16_t count) {
  // Copy it
  while (count--) {
    MM_data.push_back(*arrayOfBytes++);
  }
  // Return updated size (logical length of message so far)
  return MM_data.size();
}

// determineFloatOrder: calculate the sequence of bytes in a float value
uint8_t ModbusMessage::determineFloatOrder() {
  constexpr uint8_t floatSize = sizeof(float);
  // Only do it if not done yet
  if (floatOrder[0] == 0xFF) {
    // We need to calculate it.
    // This will only work for 32bit floats, so check that
    if (floatSize != 4) {
      // OOPS! we cannot proceed.
      LOG_E("Oops. float seems to be %d bytes wide instead of 4.\n", floatSize);
      return 0;
    }

    uint32_t i = 77230;                             // int value to go into a float without rounding error
    float f = i;                                    // assign it
    uint8_t *b = (uint8_t *)&f;                     // Pointer to bytes of f
    uint8_t expect[floatSize] = { 0x47, 0x96, 0xd7, 0x00 }; // IEEE754 representation 
    uint8_t matches = 0;                            // number of bytes successfully matched
     
    // Loop over the bytes of the expected sequence
    for (uint8_t inx = 0; inx < floatSize; ++inx) {
      // Loop over the real bytes of f
      for (uint8_t trg = 0; trg < floatSize; ++trg) {
        if (expect[inx] == b[trg]) {
          floatOrder[inx] = trg;
          matches++;
          break;
        }
      }
    }

    // All bytes found?
    if (matches != floatSize) {
      // No! There is something fishy...
      LOG_E("Unable to determine float byte order (matched=%d of %d)\n", matches, floatSize);
      return 0;
    } else {
      HEXDUMP_V("floatOrder", floatOrder, floatSize);
    }
  }
  return floatSize;
}

// determineDoubleOrder: calculate the sequence of bytes in a double value
uint8_t ModbusMessage::determineDoubleOrder() {
  constexpr uint8_t doubleSize = sizeof(double);
  // Only do it if not done yet
  if (doubleOrder[0] == 0xFF) {
    // We need to calculate it.
    // This will only work for 64bit doubles, so check that
    if (doubleSize != 8) {
      // OOPS! we cannot proceed.
      LOG_E("Oops. double seems to be %d bytes wide instead of 8.\n", doubleSize);
      return 0;
    }

    uint64_t i = 5791007487489389;                  // int64 value to go into a double without rounding error
    double f = i;                                   // assign it
    uint8_t *b = (uint8_t *)&f;                     // Pointer to bytes of f
    uint8_t expect[doubleSize] = { 0x43, 0x34, 0x92, 0xE4, 0x00, 0x2E, 0xF5, 0x6D }; // IEEE754 representation 
    uint8_t matches = 0;                            // number of bytes successfully matched
     
    // Loop over the bytes of the expected sequence
    for (uint8_t inx = 0; inx < doubleSize; ++inx) {
      // Loop over the real bytes of f
      for (uint8_t trg = 0; trg < doubleSize; ++trg) {
        if (expect[inx] == b[trg]) {
          doubleOrder[inx] = trg;
          matches++;
          break;
        }
      }
    }

    // All bytes found?
    if (matches != doubleSize) {
      // No! There is something fishy...
      LOG_E("Unable to determine double byte order (matched=%d of %d)\n", matches, doubleSize);
      return 0;
    } else {
      HEXDUMP_V("doubleOrder", doubleOrder, doubleSize);
    }
  }
  return doubleSize;
}

// add() variants for float and double values
// values will be added in IEEE754 byte sequence (MSB first)
uint16_t ModbusMessage::add(float v) {
  // First check if we need to determine byte order
  if (determineFloatOrder()) {
    // If we get here, the floatOrder is known
    uint8_t *bytes = (uint8_t *)&v;
    // Put out the bytes of v in floatOrder sequence
    for (uint8_t i = 0; i < 4; ++i) {
      MM_data.push_back(bytes[floatOrder[i]]);
    }
  }

  return MM_data.size();
}

uint16_t ModbusMessage::add(double v) {
  // First check if we need to determine byte order
  if (determineDoubleOrder()) {
    // If we get here, the doubleOrder is known
    uint8_t *bytes = (uint8_t *)&v;
    // Put out the bytes of v in doubleOrder sequence
    for (uint8_t i = 0; i < 8; ++i) {
      MM_data.push_back(bytes[doubleOrder[i]]);
    }
  }

  return MM_data.size();
}

// get() variants for float and double values
// values will be read in IEEE754 byte sequence (MSB first)
uint16_t ModbusMessage::get(uint16_t index, float& v) {
  // First check if we need to determine byte order
  if (determineFloatOrder()) {
    // If we get here, the floatOrder is known
    // Will it fit?
    if (index <= MM_data.size() - sizeof(float)) {
      // Yes. Put out the bytes of v in floatOrder sequence
      uint8_t *bytes = (uint8_t *)&v;
      for (uint8_t i = 0; i < sizeof(float); ++i) {
        bytes[i] = MM_data[index + floatOrder[i]];
      }
      index += sizeof(float);
    }
  }

  return index;
}

uint16_t ModbusMessage::get(uint16_t index, double& v) {
  // First check if we need to determine byte order
  if (determineDoubleOrder()) {
    // If we get here, the doubleOrder is known
    // Will it fit?
    if (index <= MM_data.size() - sizeof(double)) {
      // Yes. Put out the bytes of v in doubleOrder sequence
      uint8_t *bytes = (uint8_t *)&v;
      for (uint8_t i = 0; i < sizeof(double); ++i) {
        bytes[i] = MM_data[index + doubleOrder[i]];
      }
      index += sizeof(double);
    }
  }

  return index;
}

// Data validation methods for the different factory calls
// 0. serverID and function code - used by all of the below
Error ModbusMessage::checkServerFC(uint8_t serverID, uint8_t functionCode) {
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode) {
  LOG_V("Check data #1\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    // case 0x07:
    case 0x08:
    // case 0x0b:
    // case 0x0c:
    case 0x0f:
    case 0x10:
    // case 0x11:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1) {
  LOG_V("Check data #2\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    case 0x0f:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    // case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2) {
  LOG_V("Check data #3\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
      if ((p2 > 0x7d0) || (p2 == 0)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    case 0x03:
    case 0x04:
      if ((p2 > 0x7d) || (p2 == 0)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    case 0x05:
      if ((p2 != 0) && (p2 != 0xff00)) returnCode = PARAMETER_LIMIT_ERROR;
      break;
    // case 0x06: all values are acceptable for p1 and p2
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    case 0x0f:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3) {
  LOG_V("Check data #4\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    case 0x0f:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    // case 0x16:
    case 0x17:
    case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords) {
  LOG_V("Check data #5\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    case 0x0f:
    // case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes) {
  LOG_V("Check data #6\n");
  Error returnCode = checkServerFC(serverID, functionCode);
  if (returnCode == SUCCESS)
  {
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0b:
    case 0x0c:
    // case 0x0f:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
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
Error ModbusMessage::checkData(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes) {
  LOG_V("Check data #7\n");
  return checkServerFC(serverID, functionCode);
}

// Factory methods to create valid Modbus messages from the parameters
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(2);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode);
  }
  return returnCode;
}

// 2. one uint16_t parameter (FC 0x18)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, p1);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(4);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode, p1);
  }
  return returnCode;
}

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, p1, p2);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(6);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode, p1, p2);
  }
  return returnCode;
}

// 4. three uint16_t parameters (FC 0x16)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, p1, p2, p3);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(8);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode, p1, p2, p3);
  }
  return returnCode;
}

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfWords);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(7 + count * 2);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode, p1, p2);
    add(count);
    for (uint8_t i = 0; i < (count >> 1); ++i) {
      add(arrayOfWords[i]);
    }
  }
  return returnCode;
}

// 6. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfBytes);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(7 + count);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode, p1, p2);
    add(count);
    for (uint8_t i = 0; i < count; ++i) {
      add(arrayOfBytes[i]);
    }
  }
  return returnCode;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
Error ModbusMessage::setMessage(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes) {
  // Check parameter for validity
  Error returnCode = checkData(serverID, functionCode, count, arrayOfBytes);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new ModbusMessage
    MM_data.reserve(2 + count);
    MM_data.shrink_to_fit();
    MM_data.clear();
    add(serverID, functionCode);
    for (uint8_t i = 0; i < count; ++i) {
      add(arrayOfBytes[i]);
    }
  }
  return returnCode;
}

// 8. Error response generator
Error ModbusMessage::setError(uint8_t serverID, uint8_t functionCode, Error errorCode) {
  // No error checking for server ID or function code here, as both may be the cause for the message!? 
  MM_data.reserve(3);
  MM_data.shrink_to_fit();
  MM_data.clear();
  add(serverID, static_cast<uint8_t>((functionCode | 0x80) & 0xFF), static_cast<uint8_t>(errorCode));
  return SUCCESS;
}

// Error output in case a message constructor will fail
void ModbusMessage::printError(const char *file, int lineNo, Error e) {
  LOG_E("(%s, line %d) Error in constructor: %02X - %s\n", file_name(file), lineNo, e, (const char *)(ModbusError(e)));
}

uint8_t ModbusMessage::floatOrder[] = { 0xFF };
uint8_t ModbusMessage::doubleOrder[] = { 0xFF };
