// =================================================================================================
// eModbus: Copyright 2020,2021 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the eModbus library. 
// Please refer to https://emodbus.github.io/ for a full description.

// Includes: <Arduino.h> for Serial etc., WiFi.h for WiFi support
#include <Arduino.h>
#include <WiFi.h>

// Include the header for the ModbusServer WiFi style
#include "ModbusServerWiFi.h"
// Include the header for the coils data type
#include "CoilData.h"

#ifndef MY_SSID
#define MY_SSID "WiFi network ID"
#endif
#ifndef MY_PASS
#define MY_PASS "WiFi network password"
#endif

char ssid[] = MY_SSID;                     // SSID and ...
char pass[] = MY_PASS;                     // password for the WiFi network used

// Create a ModbusTCP server instance
ModbusServerWiFi MB;
// Port number the server shall listen to
const uint16_t PORT(502);

// Set up a coil storage with 35 coils, all initialized to 0
CoilData myCoils(35);

// Just for fun we will set up a trigger whenever a coils was written
bool coilTrigger = false;

// Some functions to be called when function codes 0x01, 0x05 or 0x15 are requested
// FC_01: act on 0x01 requests - READ_COIL
ModbusMessage FC_01(ModbusMessage request) {
  ModbusMessage response;
// Request parameters are first coil and number of coils to read
  uint16_t start = 0;
  uint16_t numCoils = 0;
  request.get(2, start, numCoils);

// Are the parameters valid?
  if (start + numCoils <= myCoils.coils()) {
    // Looks like it. Get the requested coils from our storage
    vector<uint8_t> coilset = myCoils.slice(start, numCoils);
    // Set up response according to the specs: serverID, function code, number of bytes to follow, packed coils
    response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)coilset.size(), coilset);
  } else {
    // Something was wrong with the parameters
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
// Return the response
  return response;
}

// FC05: act on 0x05 requests - WRITE_COIL
ModbusMessage FC_05(ModbusMessage request) {
  ModbusMessage response;
// Request parameters are coil number and 0x0000 (OFF) or 0xFF00 (ON)
  uint16_t start = 0;
  uint16_t state = 0;
  request.get(2, start, state);

// Is the coil number valid?
  if (start <= myCoils.coils()) {
    // Looks like it. Is the ON/OFF parameter correct?
    if (state == 0x0000 || state == 0xFF00) {
      // Yes. We can set the coil
      if (myCoils.set(start, state)) {
        // All fine, coil was set.
        response = ECHO_RESPONSE;
        // Pull the trigger
        coilTrigger = true;
      } else {
        // Setting the coil failed
        response.setError(request.getServerID(), request.getFunctionCode(), SERVER_DEVICE_FAILURE);
      }
    } else {
      // Wrong data parameter
      response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_VALUE);
    }
  } else {
    // Something was wrong with the coil number
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
// Return the response
  return response;
}

// FC0F: act on 0x0F requests - WRITE_MULT_COILS
ModbusMessage FC_0F(ModbusMessage request) {
  ModbusMessage response;
// Request parameters are first coil to be set, number of coils, number of bytes and packed coil bytes
  uint16_t start = 0;
  uint16_t numCoils = 0;
  uint8_t numBytes = 0;
  uint16_t offset = 2;    // Parameters start after serverID and FC
  offset = request.get(offset, start, numCoils, numBytes);

  // Check the parameters so far
  if (numCoils && start + numCoils <= myCoils.coils()) {
    // Packed coils will fit in our storage
    if (numBytes == ((numCoils - 1) >> 3) + 1) {
      // Byte count seems okay, so get the packed coil bytes now
      vector<uint8_t> coilset;
      request.get(offset, coilset, numBytes);
      // Now set the coils
      if (myCoils.set(start, numCoils, coilset)) {
        // All fine, return shortened echo response, like the standard says
        response.add(request.getServerID(), request.getFunctionCode(), start, numCoils);
        // Pull trigger
        coilTrigger = true;
      } else {
        // Oops! Setting the coils seems to have failed
        response.setError(request.getServerID(), request.getFunctionCode(), SERVER_DEVICE_FAILURE);
      }
    } else {
      // numBytes had a wrong value
      response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_VALUE);
    }
  } else {
    // The given set will not fit to our coil storage
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
  return response;
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

// Register function codes/server IDs the server shall react on
  MB.registerWorker(1, READ_COIL, FC_01);
  MB.registerWorker(1, WRITE_COIL, FC_05);
  MB.registerWorker(1, WRITE_MULT_COILS, FC_0F);

// Set some coils to 1
  myCoils.set(3, true);
  myCoils.set(5, true);
  myCoils.set(6, true);
  myCoils.set(7, true);
  myCoils.set(13, true);
  myCoils.set(14, true);
  myCoils.set(28, true);
  myCoils.set(30, true);
  myCoils.set(31, true);
  myCoils.set(33, true);

// Print out coils to Serial for reference
  Serial.println("Coil index          0    4    8    12   16   20   24   28   32");
  Serial.println("                    |    |    |    |    |    |    |    |    | ");
  myCoils.print("Initial coil state: ", Serial);

// Start the server on port PORT, max. 2 concurrent clients served, 10s timeout to close a connection
  MB.start(PORT, 2, 2000);

// Now let us do some local requests for fun. The server is running in parallel and may be requested via TCP!
  ModbusMessage resp;      // The response message we will receive
  ModbusMessage req;       // The request we will send
  CoilData cd(12);         // To make display easier, we will use our own coil set
  Error err = SUCCESS;

// Read a slice of 12 coils, starting at 13
  req.setMessage(1, READ_COIL, 13, 12);
  resp = MB.localRequest(req);
// Let us see what we got!
  if ((err = resp.getError()) == SUCCESS) {
    // We nonchalantly are ignoring here potential errors in byte count etc.
    // and directly read the expected bytes for 12 coils!
    cd.set(0, 12, (uint8_t *)resp.data() + 3);
    // Print coil set to see the result.
    cd.print("Received                          : ", Serial);
  } else {
    ModbusError me(err);
    Serial.printf("Error reading coils: %02d - %s\n", err, (const char *)me);
  }

// next set a single coil at 8
  req.setMessage(1, WRITE_COIL, 8, 0xFF00);
  resp = MB.localRequest(req);
// Let us see what we got!
  if ((err = resp.getError()) == SUCCESS) {
    // Looks like we were successful, so let us see the coils set
    myCoils.print("   Coil 8 set to 1: ", Serial);
  } else {
    ModbusError me(err);
    Serial.printf("Error  writing single coil: %02d - %s\n", err, (const char *)me);
  }

// Finally set a a bunch of coils starting at 20
  cd = "011010010110";
  req.setMessage(1, WRITE_MULT_COILS, 20, cd.coils(), cd.size(), cd.data());
  resp = MB.localRequest(req);
// Let us see what we got!
  if ((err = resp.getError()) == SUCCESS) {
    // Looks like we were successful, so let us see the coils set
    myCoils.print("Block of coils set: ", Serial);
  } else {
    ModbusError me(err);
    Serial.printf("Error writing block of coils: %02d - %s\n", err, (const char *)me);
  }

  // Your output on the Serial monitor should look similar to:
  //
  // WIFi IP address: 192.168.178.74
  // Coil index          0    4    8    12   16   20   24   28   32       
  //                     |    |    |    |    |    |    |    |    |        
  // Initial coil state: 0001 0111 0000 0110 0000 0000 0000 1011 010      
  // Received                          : 1100 0000 0000
  //    Coil 8 set to 1: 0001 0111 1000 0110 0000 0000 0000 1011 010      
  // Block of coils set: 0001 0111 1000 0110 0000 0110 1001 0110 010    
}

// loop() - watch for the trigger
void loop() {
  static bool lastValue = false;
  // Trigger pulled?
  if (coilTrigger) {
    // Yes. Was it ours?
    if (myCoils[17] != lastValue) {
      // Yes. Print out information
      myCoils.print("Coil 17 changed   : ", Serial);
      lastValue = myCoils[17];
    }
    // Rearm trigger
    coilTrigger = false;
  }
}
