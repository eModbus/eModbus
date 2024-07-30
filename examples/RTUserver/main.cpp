// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include "HardwareSerial.h"

// Modbus server include
#include "ModbusServerRTU.h"

// Create a ModbusRTU server instance listening with 2000ms timeout
ModbusServerRTU MBserver(2000);

// FC03: worker do serve Modbus function code 0x03 (READ_HOLD_REGISTER)
ModbusMessage FC03(ModbusMessage request) {
  uint16_t address;           // requested register address
  uint16_t words;             // requested number of registers
  ModbusMessage response;     // response message to be sent back

  // get request values
  request.get(2, address);
  request.get(4, words);

  // Address and words valid? We assume 10 registers here for demo
  if (address && words && (address + words) <= 10) {
    // Looks okay. Set up message with serverID, FC and length of data
    response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));
    // Fill response with requested data
    for (uint16_t i = address; i < address + words; ++i) {
      response.add(i);
    }
  } else {
    // No, either address or words are outside the limits. Set up error response.
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
  return response;
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Init Serial2 connected to the RTU Modbus
// (Fill in your data here!)
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(19200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

// Register served function code worker for server 1, FC 0x03
  MBserver.registerWorker(0x01, READ_HOLD_REGISTER, &FC03);

// Start ModbusRTU background task
  MBserver.begin(Serial2);
}

// loop() - nothing done here today!
void loop() {
  delay(10000);
}
