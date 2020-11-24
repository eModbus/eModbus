// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusMessageTCP.h"

// Default constructor: call ModbusRequest constructor, then init TCP header
ModbusMessageTCP::ModbusMessageTCP(ModbusMessageType t, TargetHost target, uint16_t dataLen, uint32_t token) :
  ModbusMessage(t, dataLen, token),
  target(target)
  { 
    tcpHead.transactionID = 0x0001;  // pro forma: will be set by ModbusTCP
    tcpHead.protocolID = 0x0000;     // constant Modbus value
    tcpHead.len = dataLen;           // same as packet size
  }

// Helper function to check IP address and port for validity
Error ModbusMessageTCP::isValidHost(TargetHost target) {
  if (target.port == 0) return ILLEGAL_IP_OR_PORT;
  if (target.host == IPAddress(0, 0, 0, 0)) return ILLEGAL_IP_OR_PORT;
  if (target.host[3] == 255) return ILLEGAL_IP_OR_PORT; // Most probably a broadcast address.
  return SUCCESS;
}

// Factory methods to create valid Modbus messages from the parameters
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 2, token);
      returnPtr->add(serverID, functionCode);
    }
  }
  return returnPtr;
}

// 2. one uint16_t parameter (FC 0x18)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, p1);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 4, token);
      returnPtr->add(serverID, functionCode, p1);
    }
  }
  return returnPtr;
}

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, p1, p2);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 6, token);
      returnPtr->add(serverID, functionCode, p1, p2);
    }
  }
  return returnPtr;
}

// 4. three uint16_t parameters (FC 0x16)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, p1, p2, p3);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 8, token);
      returnPtr->add(serverID, functionCode, p1, p2, p3);
    }
  }
  return returnPtr;
}

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfWords);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 7 + count, token);
      returnPtr->add(serverID, functionCode, p1, p2);
      returnPtr->add(count);
      for (uint8_t i = 0; i < (count >> 1); ++i) {
        returnPtr->add(arrayOfWords[i]);
      }
    }
  }
  return returnPtr;
}

// 6. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, p1, p2, count, arrayOfBytes);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 7 + count, token);
      returnPtr->add(serverID, functionCode, p1, p2);
      returnPtr->add(count);
      for (uint8_t i = 0; i < count; ++i) {
        returnPtr->add(arrayOfBytes[i]);
      }
    }
  }
  return returnPtr;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
ModbusMessageTCP *ModbusMessageTCP::createTCPRequest(Error& returnCode, TargetHost target, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token) {
  ModbusMessageTCP *returnPtr = nullptr;
  // Check parameter for validity
  returnCode = isValidHost(target);
  if (returnCode == SUCCESS) {
    returnCode = checkData(serverID, functionCode, count, arrayOfBytes);
    // No error? 
    if (returnCode == SUCCESS)
    {
      // Yes, all fine. Create new ModbusMessageTCP instance
      returnPtr = new ModbusMessageTCP(MMT_REQUEST, target, 2 + count, token);
      returnPtr->add(serverID, functionCode);
      for (uint8_t i = 0; i < count; ++i) {
        returnPtr->add(arrayOfBytes[i]);
      }
    }
  }
  return returnPtr;
}
