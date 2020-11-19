// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusMessageRTU.h"
#include <string.h>
#include <Arduino.h>


// Default constructor: call ModbusRequest constructor, then init CRC
RTURequest::RTURequest(uint16_t dataLen, uint32_t token) :
  ModbusRequest(dataLen, token),
  CRC(0) { }

// Factory methods to create valid Modbus messages from the parameters
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(2, token);
    returnPtr->add(serverID, functionCode);
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 2. one uint16_t parameter (FC 0x18)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, p1);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(4, token);
    returnPtr->add(serverID, functionCode, p1);
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, p1, p2);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(6, token);
    returnPtr->add(serverID, functionCode, p1, p2);
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 4. three uint16_t parameters (FC 0x16)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, p1, p2, p3);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(8, token);
    returnPtr->add(serverID, functionCode, p1, p2, p3);
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfWords);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(7 + count, token);
    returnPtr->add(serverID, functionCode, p1, p2);
    returnPtr->add(count);
    for (uint8_t i = 0; i < (count >> 1); ++i) {
      returnPtr->add(arrayOfWords[i]);
    }
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 6. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfBytes);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(7 + count, token);
    returnPtr->add(serverID, functionCode, p1, p2);
    returnPtr->add(count);
    for (uint8_t i = 0; i < count; ++i) {
      returnPtr->add(arrayOfBytes[i]);
    }
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
RTURequest *RTURequest::createRTURequest(Error& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token) {
  RTURequest *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = checkData(serverID, functionCode, count, arrayOfBytes);
  // No error? 
  if (returnCode == SUCCESS)
  {
    // Yes, all fine. Create new RTURequest instance
    returnPtr = new RTURequest(2 + count, token);
    returnPtr->add(serverID, functionCode);
    for (uint8_t i = 0; i < count; ++i) {
      returnPtr->add(arrayOfBytes[i]);
    }
    returnPtr->CRC = RTUutils::calcCRC(returnPtr->data(), returnPtr->len());
  }
  return returnPtr;
}

// Default constructor for RTUResponse: call ModbusResponse constructor
RTUResponse::RTUResponse(uint16_t dataLen) :
  ModbusResponse(dataLen) { }

// isValidCRC: check if CRC value matches CRC calculated over MM_data.
// *** Note: assumption is made that ModbusRTU has already extracted the CRC from the received message!
bool RTUResponse::isValidCRC() {
  if (data() && len()) {
    if (CRC == RTUutils::calcCRC(data(), len())) return true;
  }
  return false;
}

// setCRC: set CRC value without calculation
// To be done by ModbusRTU after receiving a response. CRC shall be extracted from the response!
void RTUResponse::setCRC(uint16_t crc) {
  CRC = crc;
}

#ifdef TESTING
// Test: Output CRC and message to Serial.
void RTURequest::dump(const char *label) {
  char buffer[80];

  snprintf(buffer, 80, "%s CRC:%04X", label, CRC);
  Serial.println(buffer);
  ModbusMessage::dump("Data");
}

// ********************** Test: Output CRC and message to Serial.
void RTUResponse::dump(const char *label) {
  char buffer[80];

  snprintf(buffer, 80, "%s CRC:%04X", label, CRC);
  Serial.println(buffer);
  ModbusMessage::dump("Data");
}
#endif
