// =================================================================================================
// eModbus: Copyright 2024 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include "HardwareSerial.h"
#include <WiFi.h>

// Modbus bridge include
#include "ModbusBridgeWiFi.h"
// Modbus RTU client include
#include "ModbusClientRTU.h"

#ifndef MY_SSID
#define MY_SSID "WiFi network ID"
#endif
#ifndef MY_PASS
#define MY_PASS "WiFi network password"
#endif

char ssid[] = MY_SSID;                     // SSID and ...
char pass[] = MY_PASS;                     // password for the WiFi network used
uint16_t port = 502;                       // port of modbus server

// Create a ModbusRTU client instance
ModbusClientRTU MB;

// Create bridge
ModbusBridgeWiFi MBbridge;

// This is a variant of the WiFi-RTU_Bridge example.
// The difference is: the optional request/response filtering is demonstrated here.
// Requests will all be sent through a request filter before being issued,
// responses will be given to another filter before returning those to the caller.

// filterRequest() - to be applied to incoming requests
ModbusMessage filterRequest(ModbusMessage request) {
  // We will change 0x03 function codes to 0x04 - for demo purposes
  HEXDUMP_N("Request before filter", request.data(), request.size());
  if (request.getFunctionCode() == READ_HOLD_REGISTER) {
    request.setFunctionCode(READ_INPUT_REGISTER);
  }
  HEXDUMP_N("Request after filter", request.data(), request.size());
  // In any case we must forward the request
  return request;
}

// filterResponse() - to be appliued to returning responses
ModbusMessage filterResponse(ModbusMessage response) {
  // Just dump out every response
  HEXDUMP_N("Response in filter", response.data(), response.size());
  // In any case forward the response
  return response;
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Init Serial2 conneted to the RTU Modbus
// (Fill in your data here!)
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(19200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

// Connect to WiFi
  WiFi.begin(ssid, pass);
  delay(200);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  IPAddress wIP = WiFi.localIP();
  Serial.printf("IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);

// Set RTU Modbus message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task on core 1
  MB.begin(Serial2, 1);

// Define and start WiFi bridge
// ServerID 4: Server with remote serverID 1, accessed through RTU client MB
//             All FCs accepted, with the exception of FC 06
  MBbridge.attachServer(4, 1, ANY_FUNCTION_CODE, &MB);
  MBbridge.denyFunctionCode(4, 6);
  
// ServerID 5: Server with remote serverID 4, accessed through RTU client MB
//             Only FCs 03 and 04 accepted
  MBbridge.attachServer(5, 4, READ_HOLD_REGISTER, &MB);
  MBbridge.addFunctionCode(5, READ_INPUT_REGISTER);

// Check: print out all combinations served to Serial
  MBbridge.listServer();

// Now install the filter functions to server 4
  MBbridge.addRequestFilter(4, filterRequest);
  MBbridge.addResponseFilter(4, filterResponse);

// Start the bridge. Port 502, 4 simultaneous clients allowed, 600ms inactivity to disconnect client
  MBbridge.start(port, 4, 600);

  Serial.printf("Use the shown IP and port %d to send requests!\n", port);

// Your output on the Serial monitor should start like seen below
// Upon each request arriving you will see the responses dumped.
// __ OK __
// IP address: 192.168.178.139
// [N] 338| ModbusServer.cpp     [ 200] listServer: Server   4:  00 06
// [N] 338| ModbusServer.cpp     [ 200] listServer: Server   5:  03 04
// Use the shown IP and port 502 to send requests!
// [N] Request before filter: @3FFCFDD8/6:
//   | 0000: 04 03 00 00 00 0C                                 |......          |
// [N] Request after filter: @3FFCFDD8/6:
//   | 0000: 04 04 00 00 00 0C                                 |......          |
// [N] Response in filter: @3FFCFDC0/3:
//   | 0000: 01 84 E0                                          |...             |
}

// loop() - nothing done here today!
void loop() {
  delay(10000);
}
