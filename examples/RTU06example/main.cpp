// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the ModbusClient library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusClient RTU style
#include "ModbusRTU.h"

// Create a ModbusRTU client instance
// In my case the RS485 module had auto halfduplex, so no second parameter with the DE/RE pin is required!
ModbusRTU MB(Serial2);

// Define an onData handler function to receive the regular responses
// Arguments are Modbus server ID, the function code requested, the message data and length of it, 
// plus a user-supplied token to identify the causing request
void handleData(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token) 
{
  Serial.printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", (unsigned int)serverAddress, (unsigned int)fc, token, length);
  const uint8_t *cp = data;
  uint16_t      i = length;
  while (i--) {
    Serial.printf("%02X ", *cp++);
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
  Serial2.begin(19200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

// Set up ModbusRTU client.
// - provide onData handler function
  MB.onDataHandler(&handleData);
// - provide onError handler function
  MB.onErrorHandler(&handleError);
// Set message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin();

// We will first read the register, then write to it and finally read it again to verify the change

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 10
// - data words to read = 1
// - token to match the response with the request.
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued
  uint32_t Token = 1111;

  Error err = MB.addRequest(1, READ_HOLD_REGISTER, 10, 1, Token++);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x06 (write holding register)
// - address to write = word 10
// - data word to write = 0xBEEF
// - token to match the response with the request.
//
  err = MB.addRequest(1, WRITE_HOLD_REGISTER, 10, 0xBEEF, Token++);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Create request for
// (Fill in your data here!)
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 10
// - data words to read = 1
// - token to match the response with the request.
//
  err = MB.addRequest(1, READ_HOLD_REGISTER, 10, 1, Token++);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// The output on the Serial Monitor will be (depending on your Modbus the data will be different):
//      Response: serverID=1, FC=3, Token=00000458, length=5:
//      01 03 02 32 33
//      Response: serverID=1, FC=6, Token=00000459, length=6:
//      01 06 00 0A BE EF
//      Response: serverID=1, FC=3, Token=0000045A, length=5:
//      01 03 02 BE EF
}

// loop() - nothing done here today!
void loop() {
}
