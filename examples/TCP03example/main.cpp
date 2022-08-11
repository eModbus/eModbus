// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the eModbus library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include <WiFi.h>

// Include the header for the ModbusClient TCP style
#include "ModbusClientTCP.h"

WiFiClient theClient;                          // Set up a client
#ifndef MY_SSID
#define MY_SSID "WiFi network ID"
#endif
#ifndef MY_PASS
#define MY_PASS "WiFi network password"
#endif

char ssid[] = MY_SSID;                     // SSID and ...
char pass[] = MY_PASS;                     // password for the WiFi network used

// Create a ModbusTCP client instance
ModbusClientTCP MB(theClient);

// Define an onData handler function to receive the regular responses
// Arguments are the message plus a user-supplied token to identify the causing request
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

// Connect to WiFi
  WiFi.begin(ssid, pass);
  delay(200);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(". ");
    delay(1000);
  }
  IPAddress wIP = WiFi.localIP();
  Serial.printf("WIFi IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);

// Set up ModbusTCP client.
// - provide onData handler function
  MB.onDataHandler(&handleData);
// - provide onError handler function
  MB.onErrorHandler(&handleError);
// Set message timeout to 2000ms and interval between requests to the same host to 200ms
  MB.setTimeout(2000, 200);
// Start ModbusTCP background task
  MB.begin();

// Issue a request
// Set Modbus TCP server address and port number
// (Fill in your data here!)
  MB.setTarget(IPAddress(0, 0, 0, 0), 0);

// Create request for
// (Fill in your data here!)
// - token to match the response with the request. We take the current millis() value for it.
// - server ID = 20
// - function code = 0x03 (read holding register)
// - start address to read = word 10
// - number of words to read = 4
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued
  Error err = MB.addRequest((uint32_t)millis(), 20, READ_HOLD_REGISTER, 10, 4);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Else the request is processed in the background task and the onData/onError handler functions will get the result.
//
// The output on the Serial Monitor will be (depending on your WiFi and Modbus the data will be different):
//     __ OK __
//     . WIFi IP address: 192.168.178.74
//     Response: serverID=20, FC=3, Token=0000056C, length=11:
//     14 03 04 01 F6 FF FF FF 00 C0 A8
}

// loop() - nothing done here today!
void loop() {
}
