// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the ModbusClient library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusClient RTU style
#include "ModbusClientRTU.h"

// Create a ModbusRTU client instance
// In my case the RS485 module had auto halfduplex, so no second parameter with the DE/RE pin is required!
// Note: we will generate a message without sending it, so Serial2 needs not to be initialized!
ModbusClientRTU MB(Serial2);

// Note: no onData or onError handler defined here - we will not need these!

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Generate message for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 10
// - data words to read = 1
//
  RTUMessage msg = MB.generateRequest(1, READ_HOLD_REGISTER, 10, 1);

// If something was wrong with our parameters, the message will contain just 1 byte with the Error code
  if (msg.size() == 1) {
    ModbusError e((Error)msg[0]);
    Serial.printf("Error generating message: %02X - %s\n", (int)e, (const char *)e);
  } else {
    Serial.print("Generated message: ");
    for (auto& byte : msg) {
      Serial.printf("%02X ", byte);
    }
    Serial.println("< ");
  }

// Next we will generate an error response
  msg = MB.generateErrorResponse(42, WRITE_MULT_REGISTERS, ILLEGAL_DATA_VALUE);

// If something was wrong with our parameters, the message will contain just 1 byte with the Error code
  if (msg.size() == 1) {
    ModbusError e((Error)msg[0]);
    Serial.printf("Error generating message: %02X - %s\n", (int)e, (const char *)e);
  } else {
    Serial.print("Generated message: ");
    for (auto& byte : msg) {
      Serial.printf("%02X ", byte);
    }
    Serial.println("< ");
  }
// on the Serial monitor output will look similar to:
//     __ OK __
//     Generated message: 01 03 00 0A 00 01 A4 08 <
//     Generated message: 2A 90 03 7C 09 <
//
// Note the trailing two CRC16 bytes in each message required for Modbus RTU!
}

// loop() - nothing done here today!
void loop() {
}
