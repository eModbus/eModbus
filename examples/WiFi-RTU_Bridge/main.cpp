// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
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

char ssid[] = "xxx";                     // SSID and ...
char pass[] = "xxx";                     // password for the WiFi network used
uint16_t port = 502;                      // port of modbus server

// Create a ModbusRTU client instance
ModbusClientRTU MB(Serial2);

// Create server
ModbusBridgeWiFi MBbridge(5000);

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

// Init Serial2 conneted to the RTU Modbus
// (Fill in your data here!)
  Serial2.begin(19200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

// Connect to WiFi
  WiFi.begin(ssid, pass);
  delay(200);
  int status = WiFi.status();
  while (status != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
    status = WiFi.status();
  }
  IPAddress wIP = WiFi.localIP();
  Serial.printf("IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);

// Set RTU Modbus message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin();

// Define and start WiFi bridge
// ServerID 4: TCP-Server with remote serverID 1, accessed through RTU client MB
//             All FCs accepted, with the exception of FC 06
  MBbridge.attachServer(4, 1, ANY_FUNCTION_CODE, &MB);
  MBbridge.denyFunctionCode(4, WRITE_HOLD_REGISTER);

// ServerID 5: TCP-Server with remote serverID 4, accessed through RTU client MB
//             Only FCs 03 and 04 accepted
  MBbridge.attachServer(5, 4, READ_HOLD_REGISTER, &MB);
  MBbridge.addFunctionCode(5, READ_INPUT_REGISTER);

// Check: print out all combinations served to Serial
  MBbridge.listServer();

// Start the bridge. Port 502, 4 simultaneous clients allowed, 600ms inactivity to disconnect client
  MBbridge.start(port, 4, 600);
}

// loop() - nothing done here today!
void loop() {
  delay(10000);
}
