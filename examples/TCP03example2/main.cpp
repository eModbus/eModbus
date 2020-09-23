// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the ModbusClient library. 
// Please refer to root/Readme.md for a full description.

// Includes: <Arduino.h> for Serial etc., EThernet.h for Ethernet support
#include <Arduino.h>
#include <Ethernet.h>

// W5500 reset pin 
#define RESET_P GPIO_NUM_26

byte mac[] = { 0x30, 0x2B, 0x2D, 0x2F, 0x61, 0xE2 }; // MAC address (fill your own data here!)
IPAddress lIP;                      // IP address after Ethernet.begin()

// Include the header for the ModbusClient TCP style
#include "ModbusTCP.h"

EthernetClient theClient;                          // Set up a client

// Create a ModbusTCP client instance
ModbusTCP MB(theClient);

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

// Reset W5500 module
void WizReset() {
    Serial.print("Resetting Wiz W5500 Ethernet Board...  ");
    pinMode(RESET_P, OUTPUT);
    digitalWrite(RESET_P, HIGH);
    delay(250);
    digitalWrite(RESET_P, LOW);
    delay(50);
    digitalWrite(RESET_P, HIGH);
    delay(350);
    Serial.print("Done.\n");
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// SPI CS line is GPIO_NUM_5
  Ethernet.init(GPIO_NUM_5);
  // Hard boot W5500 module
  WizReset();

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    // No. DHCP did not work.
    Serial.print("Failed to configure Ethernet using DHCP\n");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.print("Ethernet shield was not found.  Sorry, can't run without hardware. :(\n");
    } else {
      if (Ethernet.linkStatus() == LinkOFF) {
        Serial.print("Ethernet cable is not connected.\n");
      }
    }
    while (1) {}  // Do nothing any more
  }

  // print local IP address:
  lIP = Ethernet.localIP();
  Serial.printf("My IP address: %u.%u.%u.%u\n", lIP[0], lIP[1], lIP[2], lIP[3]);

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
// - server ID = 20
// - function code = 0x03 (read holding register)
// - start address to read = word 10
// - number of words to read = 4
// - token to match the response with the request. We take the current millis() value for it.
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued
  Error err = MB.addRequest(20, READ_HOLD_REGISTER, 10, 4, (uint32_t)millis());
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }

// Else the request is processed in the background task and the onData/onError handler functions will get the result.
//
// The output on the Serial Monitor will be (depending on your network and Modbus the data will be different):
//     __ OK __
//     Resetting Wiz W5500 Ethernet Board...  Done.
//     My IP address: 192.168.178.81
//     Response: serverID=20, FC=3, Token=00000564, length=11:
//     14 03 04 01 F6 FF FF FF 00 C0 A8
}

// loop() - nothing done here today!
void loop() {
}
