// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the ModbusClient library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusMessage class
#include "ModbusMessage.h"

// A helper function to print out the messages
void printMsg(ModbusMessage msg) {
  Serial.printf("Message size: %d\n", msg.size());
  for (auto& byte : msg) {
    Serial.printf("%02X ", byte);
  }
  if (msg.getError() != SUCCESS) {
    Serial.printf("Is an error message: %02d - %s\n", msg.getError(), (const char *)ModbusError(msg.getError()));
  }
  Serial.println("\n");
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Generate some messages in different ways

// Define message
  ModbusMessage msg1;

// Set it to a 0x03 read holding register request
  msg1.setMessage(1, READ_HOLD_REGISTER, 22, 7);
  printMsg(msg1);

// Reuse it for an error response
  msg1.setError(1, WRITE_COIL, ILLEGAL_FUNCTION);
  printMsg(msg1);

// Another message variable, immediately initialized to a request message
  ModbusMessage msg2(19, READ_INPUT_REGISTER, 16, 9);
  printMsg(msg2);

// Some imaginary user function code response
  msg2.setMessage(3, USER_DEFINED_46);
  msg2.add((uint16_t)0xBEEF, (uint16_t)0x0101);
  printMsg(msg2);

// Let us get some data from the message
  uint16_t word1, word2;
  msg2.get(2, word1);
  msg2.get(4, word2);
  Serial.printf("%04X %04X\n\n", word2, word1);

// Some larger data required for WRITE_MULTIPLE_REGISTER
  uint16_t data[8] = { 11, 22, 33, 44, 55, 66, 77, 88 };
  ModbusMessage msg3(1, WRITE_MULT_REGISTERS, 16, 8, 16, data);
  printMsg(msg3);

// Your output should like the following
//      __ OK __
//      Message size: 6
//      01 03 00 16 00 07
//      
//      Message size: 3
//      01 85 01 Is an error message: 01 - Illegal function code
//      
//      
//      Message size: 6
//      13 04 00 10 00 09
//      
//      Message size: 6
//      03 46 BE EF 01 01 
//      
//      0101 BEEF
// 
//      Message size: 23
//      01 10 00 10 00 08 10 00 0B 00 16 00 21 00 2C 00 37 00 42 00 4D 00 58 
}

// loop() - nothing done here today!
void loop() {
}
