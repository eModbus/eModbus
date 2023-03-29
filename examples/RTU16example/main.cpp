// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the eModbus library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusClient RTU style
#include "ModbusClientRTU.h"

// Create a ModbusRTU client instance
// In my case the RS485 module had auto halfduplex, so no parameter with the DE/RE pin is required!
ModbusClientRTU MB;

// Define an onData handler function to receive the regular responses
// Arguments are Modbus server ID, the function code requested, the message data and length of it, 
// plus a user-supplied token to identify the causing request
void handleData(ModbusMessage response, uint32_t token) 
{
  Serial.printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
  for (auto& byte : response) {
    Serial.printf("%02X ", byte);
  }
  Serial.println("");
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  Serial.printf("Error response: %02X - %s\n", (int)me, (const char *)me);
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Set up Serial2 connected to Modbus RTU
// (Fill in your data here!)
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(19200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

// Set up ModbusRTU client.
// - provide onData handler function
  MB.onDataHandler(&handleData);
// - provide onError handler function
  MB.onErrorHandler(&handleError);
// Set message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin(Serial2);

// We will first read the registers, then write to them and finally read them again to verify the change

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 33
// - data words to read = 6
// - token to match the response with the request.
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued
  uint32_t Token = 1111;

  Error err = MB.addRequest(Token++, 1, READ_HOLD_REGISTER, 33, 6);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x16 (write multiple registers)
// - address to write = word 33ff
// - data words to write = see below
// - data bytes to write = see below
// - token to match the response with the request.
//
  uint16_t wData[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666 };

  err = MB.addRequest(Token++, 1, WRITE_MULT_REGISTERS, 33, 6, 12, wData);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 33
// - data words to read = 6
// - token to match the response with the request.
//
  err = MB.addRequest(Token++, 1, READ_HOLD_REGISTER, 33, 6);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// The output on the Serial Monitor will be (depending on your Modbus the data will be different):
//      __ OK __
//      Response: serverID=1, FC=3, Token=00000457, length=15:
//      01 03 0C 60 61 62 63 64 65 66 67 68 69 6A 6B
//      Response: serverID=1, FC=16, Token=00000458, length=19:
//      01 10 00 21 00 06 0C 11 11 22 22 33 33 44 44 55 55 66 66
//      Response: serverID=1, FC=3, Token=00000459, length=15:
//      01 03 0C 11 11 22 22 33 33 44 44 55 55 66 66
}

// loop() - nothing done here today!
void loop() {
}
