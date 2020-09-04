#ifndef _MODBUS_MESSAGE_RTU_H
#define _MODBUS_MESSAGE_RTU_H
#include "ModbusMessage.h"

class RTURequest : public ModbusRequest {
  friend class ModbusRTU;
protected:
  uint16_t CRC;          // CRC16 value

  // Default constructor
  RTURequest(uint32_t token = 0);

  // Factory methods to create valid Modbus messages from the parameters
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  
  // 2. one uint16_t parameter (FC 0x18)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  
  // 4. three uint16_t parameters (FC 0x16)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  static RTURequest *createRTURequest(uint8_t& returnCode, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);

  void isInstance() { return; }        // Make class instantiable
};

class RTUResponse : public ModbusResponse {
  friend class ModbusRTU;
protected:
  uint16_t CRC;          // CRC16 value
  void isInstance() { return; }        // Make class instantiable
};

#endif