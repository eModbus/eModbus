// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include <WiFi.h>

// Modbus server include
#include "ModbusServerWiFi.h"
// Modbus TCP client include
#include "ModbusClientTCP.h"

// We will be using the loopback interface, so no SSID or PASSWORD required here!
uint16_t port = 502;                      // port of modbus server

// Create a ModbusTCP client instance
WiFiClient wc;
ModbusClientTCP MB(wc);

// Create server
ModbusServerWiFi MBserver;

// Server function to handle FC 0x03 and 0x04
ModbusMessage FC03(ModbusMessage request) {
  ModbusMessage response;      // The Modbus message we are going to give back
  uint16_t addr = 0;           // Start address
  uint16_t words = 0;          // # of words requested
  request.get(2, addr);        // read address from request
  request.get(4, words);       // read # of words from request

// Address overflow?
  if ((addr + words) > 20) {
    // Yes - send respective error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  } else {
    // Set up response
    response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));
    // Request for FC 0x03?
    if (request.getFunctionCode() == READ_HOLD_REGISTER) {
      // Yes. Complete response
      for (uint8_t i = 0; i < words; ++i) {
        // send increasing data values
        response.add((uint16_t)(addr + i));
      }
    } else {
      // No, this is for FC 0x04. Response is random
      for (uint8_t i = 0; i < words; ++i) {
        // send increasing data values
        response.add((uint16_t)random(1, 65535));
      }
    }
  }
  // Send response back
  return response;
}

// Data and error handlers for the client
void handleData(ModbusMessage rsp, uint32_t token) {
  Serial.printf("Response #%d: ", token);
  for (auto& byte : rsp) {
    Serial.printf("%02X ", byte);
  }
  Serial.println("");
}
void handleError(Error err, uint32_t token) {
  // Print out error using color emphasis from internal Logging.h
  Serial.printf("Response #%d: Error %02X - " LL_RED "%s\n" LL_NORM, token, (uint8_t)err, (const char *)ModbusError(err));
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Fake WiFi start to initialize internal loopback
  WiFi.begin("foo", "bar");

// Set up TCP server to react on FCs 0x03 and 0x04
  MBserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);
  MBserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);
// Start server
  MBserver.start(port, 2, 2000);

// Set up client - register data and error handlers
  MB.onDataHandler(&handleData);
  MB.onErrorHandler(&handleError);
// Set Modbus client message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin();
// Set target to loopback interface
  MB.setTarget(IPAddress(127, 0, 0, 1), port);
}

// loop() - Generate random requests
void loop() {
  static uint32_t Token = 1;
  static ModbusMessage request;
  static Error e;

  // Set up request message
  // The random parameters intentionally will sometimes be wrong to trigger error messages!
  e = request.setMessage(random(1, 2), random(2, 5), random(0, 20), random(0, 10));
  if (e != SUCCESS) {
    // Creation of the message failed.
    // Print out error using color emphasis from internal Logging.h
    Serial.printf("Error %02X - " LL_CYAN "%s\n" LL_NORM, (uint8_t)e, (const char *)ModbusError(e));
  } else {
    // Message was created okay.
    Serial.printf("Request  #%d: ", Token);
    for (auto& byte : request) {
      Serial.printf("%02X ", byte);
    }
    Serial.println("");
    // Send it
    MB.addRequest(request, Token++);
  }

  delay(10000);
}
