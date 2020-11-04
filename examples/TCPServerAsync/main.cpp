// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include <WiFi.h>

// Modbus server TCP
#include "ModbusServerTCPasync.h"

char ssid[] = "xxx";                     // SSID and ...
char pass[] = "xxx";                     // password for the WiFi network used

// Create server
ModbusServerTCPasync MBserver;

uint16_t memo[32];                     // Test server memory: 32 words

// Worker function for function code 0x03
ResponseType FC03(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read

  // Get addr and words from data array. Values are MSB-first, getValue() will convert to binary
  // Returned: number of bytes eaten up 
  uint16_t offs = getValue(data, dataLen, addr);
  offs += getValue(data + offs, dataLen - offs, wrds);

  // address valid?
  if (!addr || addr > 32) {
    // No. Return error response
    return ModbusServer::ErrorResponse(ILLEGAL_DATA_ADDRESS);
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Number of words valid?
  if (!wrds || (addr + wrds) > 32) {
    // No. Return error response
    return ModbusServer::ErrorResponse(ILLEGAL_DATA_ADDRESS);
  }

  // Data buffer for returned values. +1 for the leading length byte!
  uint8_t rdata[wrds * 2 + 1];

  // Set length byte
  rdata[0] = wrds * 2;
  offs = 1;

  // Loop over all words to be sent
  for (uint16_t i = 0; i < wrds; i++) {
    // Add word MSB-first to response buffer
    // serverID 1 gets the real values, all others the inverted values
    offs += addValue(rdata + offs, wrds * 2 - offs + 1, (uint16_t)((serverID == 1) ? memo[addr + i] : ~memo[addr + i]));
  }

  // Return the data response
  return ModbusServer::DataResponse(wrds * 2 + 1, rdata);
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
    delay(500);
  }
  IPAddress wIP = WiFi.localIP();
  Serial.printf("WIFi IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);

// Set up test memory
  for (uint16_t i = 0; i < 32; ++i) {
    memo[i] = (i * 2) << 8 | ((i * 2) + 1);
  }

// Define and start RTU server
  MBserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=1
  MBserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);     // FC=04 for serverID=1
  MBserver.registerWorker(2, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=2
  MBserver.start(502, 1, 20000);
}

// loop() - nothing done here today!
void loop() {
  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();
    Serial.printf("free heap: %d\n", ESP.getFreeHeap());
  }
}