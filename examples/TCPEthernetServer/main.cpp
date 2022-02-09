// =================================================================================================
// eModbus: Copyright 2021 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>
#include "ModbusServerEthernet.h"

// Define Ethernet-based server
ModbusServerEthernet MBserver;

// The WIZ5500 module needs the reset line connected to a GPIO
#define RESET_P GPIO_NUM_26

// Create server
uint16_t memo[32]; // Test server memory: 32 words

// Server function to handle FC 0x03 and 0x04
ModbusMessage FC03(ModbusMessage request)
{
  Serial.println(request);
  ModbusMessage response; // The Modbus message we are going to give back
  uint16_t addr = 0;      // Start address
  uint16_t words = 0;     // # of words requested
  request.get(2, addr);   // read address from request
  request.get(4, words);  // read # of words from request

  // Address overflow?
  if ((addr + words) > 20)
  {
    // Yes - send respective error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
  // Set up response
  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));
  // Request for FC 0x03?
  if (request.getFunctionCode() == READ_HOLD_REGISTER)
  {
    // Yes. Complete response
    for (uint8_t i = 0; i < words; ++i)
    {
      // send increasing data values
      response.add((uint16_t)(memo[addr + i]));
    }
  }
  else
  {
    // No, this is for FC 0x04. Response is random
    for (uint8_t i = 0; i < words; ++i)
    {
      // send increasing data values
      response.add((uint16_t)random(1, 65535));
    }
  }
  // Send response back
  return response;
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
void setup()
{
  // Init Serial monitor
  Serial.begin(115200);
  Serial.println("__ OK __");
  
  // Fire up Ethernet
  Ethernet.init(5);
  IPAddress lIP;
  byte mac[6]={0xAA,0xAB,0xAC,0xAD,0xAE,0xAF}; // Fill in appropriate values here!

  // prime the WIZ5500 module
  WizReset();

  // Try to get an IP via DHCP
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

  // We seem to have a connection to the router
  lIP = Ethernet.localIP();
  Serial.printf("My IP address: %u.%u.%u.%u\n", lIP[0], lIP[1], lIP[2], lIP[3]);

  // Set up test memory
  for (uint16_t i = 0; i < 32; ++i)
  {
    memo[i] = 100 + i;
  }
  
  // Now set up the server for some function codes
  MBserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=1
  MBserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);     // FC=04 for serverID=1
  MBserver.registerWorker(2, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=2
  
  // Start the server
  MBserver.start(502, 2, 20000);
}

// loop() - print out the free heap size every 5 seconds
void loop()
{
  static uint32_t lastMillis = 0;

  if (millis() - lastMillis > 5000)
  {
    lastMillis = millis();
    Serial.printf("Millis: %10d - free heap: %d\n", lastMillis, ESP.getFreeHeap());
  }
}
