// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the eModbus library. 
// Please refer to root/Readme.md for a full description.

// Note: this is an example for the "EASTRON SDM630 Modbus-V2" power meter!

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusClient RTU style
#include "ModbusClientRTU.h"
#include "Logging.h"

// Definitions for this special case
#define RXPIN GPIO_NUM_16
#define TXPIN GPIO_NUM_17
#define REDEPIN GPIO_NUM_4
#define BAUDRATE 9600
#define FIRST_REGISTER 0x002A
#define NUM_VALUES 21
#define READ_INTERVAL 10000

bool data_ready = false;
float values[NUM_VALUES];
uint32_t request_time;

// Create a ModbusRTU client instance
// The RS485 module has no halfduplex, so the parameter with the DE/RE pin is required!
ModbusClientRTU MB(REDEPIN);

// Define an onData handler function to receive the regular responses
// Arguments are received response message and the request's token
void handleData(ModbusMessage response, uint32_t token) 
{
  // First value is on pos 3, after server ID, function code and length byte
  uint16_t offs = 3;
  // The device has values all as IEEE754 float32 in two consecutive registers
  // Read the requested in a loop
  for (uint8_t i = 0; i < NUM_VALUES; ++i) {
    offs = response.get(offs, values[i]);
  }
  // Signal "data is complete"
  request_time = token;
  data_ready = true;
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
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(BAUDRATE, SERIAL_8N1, RXPIN, TXPIN);

// Set up ModbusRTU client.
// - provide onData handler function
  MB.onDataHandler(&handleData);
// - provide onError handler function
  MB.onErrorHandler(&handleError);
// Set message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin(Serial2);
}

// loop() - cyclically request the data
void loop() {
  static unsigned long next_request = millis();

  // Shall we do another request?
  if (millis() - next_request > READ_INTERVAL) {
    // Yes.
    data_ready = false;
    // Issue the request
    Error err = MB.addRequest((uint32_t)millis(), 1, READ_INPUT_REGISTER, FIRST_REGISTER, NUM_VALUES * 2);
    if (err!=SUCCESS) {
      ModbusError e(err);
      LOG_E("Error creating request: %02X - %s\n", (int)e, (const char *)e);
    }
    // Save current time to check for next cycle
    next_request = millis();
  } else {
    // No, but we may have another response
    if (data_ready) {
      // We do. Print out the data
      Serial.printf("Requested at %8.3fs:\n", request_time / 1000.0);
      for (uint8_t i = 0; i < NUM_VALUES; ++i) {
        Serial.printf("   %04X: %8.3f\n", i * 2 + FIRST_REGISTER, values[i]);
      }
      Serial.printf("----------\n\n");
      data_ready = false;
    }
  }
}
