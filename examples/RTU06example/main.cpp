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

// For demonstration, use the LOG statements for output
#include "Logging.h"

// Create a ModbusRTU client instance
// In my case the RS485 module had auto halfduplex, so no parameter with the DE/RE pin is required!
ModbusClientRTU MB;

// Define an onData handler function to receive the regular responses
// Arguments are Modbus server ID, the function code requested, the message data and length of it, 
// plus a user-supplied token to identify the causing request
void handleData(ModbusMessage response, uint32_t token) 
{
  // Only print out result of the "real" example - not the request preparing the field
  if (token > 1111) {
    LOG_N("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
    HEXDUMP_N("Data", response.data(), response.size());
  }
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  LOG_E("Error response: %02X - %s\n", (int)me, (const char *)me);
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

// We will first set the register to a known state, read the register, 
// then write to it and finally read it again to verify the change

// Set defined conditions first - write 0x1234 to the register
// The Token value is used in handleData to avoid the output for this first preparation request!
  uint32_t Token = 1111;

  Error err = MB.addRequest(Token++, 1, WRITE_HOLD_REGISTER, 10, 0x1234);
  if (err!=SUCCESS) {
    ModbusError e(err);
    LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Create request by
// (Fill in your data here!)
// - token to match the response with the request.
// - server ID = 1
// - function code = 0x03 (read holding register)
// - address to read = word 10
// - data words to read = 1
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued

// Read register
  err = MB.addRequest(Token++, 1, READ_HOLD_REGISTER, 10, 1);
  if (err!=SUCCESS) {
    ModbusError e(err);
    LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Now write 0xBEEF to it
  err = MB.addRequest(Token++, 1, WRITE_HOLD_REGISTER, 10, 0xBEEF);
  if (err!=SUCCESS) {
    ModbusError e(err);
    LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Read it again to verify
  err = MB.addRequest(Token++, 1, READ_HOLD_REGISTER, 10, 1);
  if (err!=SUCCESS) {
    ModbusError e(err);
    LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Provoke an error just to show it
  err = MB.addRequest(Token++, 1, USER_DEFINED_44, 10, 1);
  if (err!=SUCCESS) {
    ModbusError e(err);
    LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// The output on the Serial Monitor will be (depending on your Modbus the data will be different):
//     __ OK __
//     [N] 163| main.cpp             [  29] handleData: Response: serverID=1, FC=3, Token=00000458, length=5:
//     [N] Data: @3FFB9808/5:
//       | 0000: 01 03 02 12 34                                    |....4           |
//     [N] 185| main.cpp             [  29] handleData: Response: serverID=1, FC=6, Token=00000459, length=6:
//     [N] Data: @3FFB9808/6:
//       | 0000: 01 06 00 0A BE EF                                 |......          |
//     [N] 209| main.cpp             [  29] handleData: Response: serverID=1, FC=3, Token=0000045A, length=5:
//     [N] Data: @3FFB9808/5:
//       | 0000: 01 03 02 BE EF                                    |.....           |
//     [E] 231| main.cpp             [  40] handleError: Error response: 01 - Illegal function code
}

// loop() - nothing done here today!
void loop() {
}
