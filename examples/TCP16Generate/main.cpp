// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the ModbusClient library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>
#include <Ethernet.h>

// Include the header for the ModbusClient TCP style
#include "ModbusClientTCP.h"

// Create a ModbusTCP client instance
// Note: we will generate a message without sending it, so the EthernetClient needs not to be initialized!
EthernetClient theClient;
ModbusClientTCP MB(theClient);

// Note: no onData or onError handler defined here - we will not need these!

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Generate message for
// (Fill in your data here!)
// - transaction ID = 0xBEEF
// - server ID = 1
// - function code = 0x10 (write multiple register)
// - address to write = word 61
// - data words to write = 6
//
  uint16_t wData[] = { 0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x1234, 0xFFFF };

  TCPMessage msg = MB.generateRequest(0xBEEF, 1, WRITE_MULT_REGISTERS, 61, 6, 12, wData);

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
  msg = MB.generateErrorResponse(0xDEAD, 1, WRITE_MULT_REGISTERS, PACKET_LENGTH_ERROR);

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
//     Generated message: BE EF 00 00 00 13 01 10 00 3D 00 06 0C 12 34 56 78 9A BC DE F0 12 34 FF FF <
//     Generated message: DE AD 00 00 00 03 01 90 E5 <
//
// Note the leading six TCP header bytes in each message required for Modbus TCP!
}

// loop() - nothing done here today!
void loop() {
}
