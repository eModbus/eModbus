// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "ModbusRTU.h"
#include "ModbusTCP.h"
#include <Wifi.h>
#include <vector>
using std::vector;

#define STRINGIFY(x) #x
#define LNO(x) "line " STRINGIFY(x) " "

// Test prerequisites
WiFiClient wc;
ModbusTCP TestTCP(wc);                 // ModbusTCP test instance. Will never be started with begin()!
ModbusRTU TestRTU(Serial);             // ModbusRTU test instance. Will never be started with begin()!
uint16_t testsExecuted = 0;            // Global test cases counter. Incremented in testOutput().
uint16_t testsPassed = 0;              // Global passed test cases counter. Incremented in testOutput().

// testOutput:  takes the test function name called, the test case name and expected and recieved messages,
// compares both and prints out the result.
// If the test passed, true is returned - else false.
bool testOutput(const char *testname, const char *name, vector<uint8_t> expected, vector<uint8_t> received) {
  testsExecuted++;
  Serial.printf("%s, %s - ", testname, name);

  if (expected == received) {
    testsPassed++;
    Serial.println("passed.");
    return true;
  } 

  Serial.println("failed:");
  Serial.print("   Expected:");
  for (auto& b : expected) {
    Serial.printf(" %02X", b);
  }
  Serial.println();
  Serial.print("   Received:");
  for (auto& b : received) {
    Serial.printf(" %02X", b);
  }
  Serial.println();
  
  return false;
}

// Helper function to convert hexadecimal ([0-9A-F]) digits in a char array into a vector of bytes
vector<uint8_t> makeVector(const char *text) {
  vector<uint8_t> rv;            // The vector to be returned
  uint8_t byte = 0;
  uint8_t nibble = 0;
  bool tick = false;             // Counting nibbles
  bool useIt = false;            // true, if a hex digit was read
  const char *cp = text;

  // Loop the char array
  while (*cp) {
    // Is it a decimal digit?
    if ((*cp >= '0' && *cp <= '9')) {
      // Yes. Convert it to a number 0-9
      nibble = (*cp - '0');
      // Flag to use the digit
      useIt = true;
    // No decimal, but a hex digit A-F?
    } else if (*cp >= 'A' && *cp <= 'F') {
      // Yes. Convert it to a number 10-15
      nibble = (*cp - 'A' + 10);
      // Flag to use the digit
      useIt = true;
    // No hexadecimal digit, ignore it.
    } else {
      useIt = false;
    }
    // Shall we use the digit?
    if (useIt) {
      // Yes. Move the previous 4 bits up
      byte <<= 4;
      // add in the digit
      byte |= nibble;
      // Are we at the second nibble of a byte?
      if (tick) {
        // Yes. put the byte into the vector and clear it to be re-used.
        rv.push_back(byte);
        byte = 0;
      }
      // Count up used nibbles
      tick = !tick;
    }
    cp++;
  }
  return rv;
}

// The test functions are named as follows: "RTU" or "TCP" and a 2-digit number denoting the 
// underlying generateRequest() function. The "XXX08()" functions will call generateErrorResponse() instead
//
// The last three calling arguments always are:
// interface: a reference to the used ModbusTCP or ModbusRTU instance
// name: pointer to a char array holding the test case description
// expected: the byte sequence, that normally should result from the call. There are two variants for each
//           of the test functions, one accepting the sequence as a vector<uint8_t>, the other will expect a char array
//           with hexadecimal digits like ("12 34 56 78 9A BC DE F0"). All characters except [0-9A-F] are ignored!
//
// The leading [n] calling arguments are the same that will be used for the respective generateRequest() calls.
bool RTU01(uint8_t serverID, uint8_t functionCode, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode);
  return testOutput(__func__, name, expected, msg);
}

bool RTU01(uint8_t serverID, uint8_t functionCode, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU02(uint8_t serverID, uint8_t functionCode, uint16_t p1, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1);
  return testOutput(__func__, name, expected, msg);
}

bool RTU02(uint8_t serverID, uint8_t functionCode, uint16_t p1, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU03(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2);
  return testOutput(__func__, name, expected, msg);
}

bool RTU03(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU04(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, p3);
  return testOutput(__func__, name, expected, msg);
}

bool RTU04(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, p3);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU05(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, count, arrayOfWords);
  return testOutput(__func__, name, expected, msg);
}

bool RTU05(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, count, arrayOfWords);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU06(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, count, arrayOfBytes);
  return testOutput(__func__, name, expected, msg);
}

bool RTU06(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, p1, p2, count, arrayOfBytes);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU07(uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, count, arrayOfBytes);
  return testOutput(__func__, name, expected, msg);
}

bool RTU07(uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(serverID, functionCode, count, arrayOfBytes);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool RTU08(uint8_t serverID, uint8_t functionCode, Error error, ModbusRTU& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateErrorResponse(serverID, functionCode, error);
  return testOutput(__func__, name, expected, msg);
}

bool RTU08(uint8_t serverID, uint8_t functionCode, Error error, ModbusRTU& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateErrorResponse(serverID, functionCode, error);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP01(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode);
  return testOutput(__func__, name, expected, msg);
}

bool TCP01(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP02(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1);
  return testOutput(__func__, name, expected, msg);
}

bool TCP02(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP03(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2);
  return testOutput(__func__, name, expected, msg);
}

bool TCP03(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP04(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, p3);
  return testOutput(__func__, name, expected, msg);
}

bool TCP04(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, p3);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP05(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfWords);
  return testOutput(__func__, name, expected, msg);
}

bool TCP05(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfWords);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP06(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfBytes);
  return testOutput(__func__, name, expected, msg);
}

bool TCP06(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfBytes);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP07(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, count, arrayOfBytes);
  return testOutput(__func__, name, expected, msg);
}

bool TCP07(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateRequest(transactionID, serverID, functionCode, count, arrayOfBytes);
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool TCP08(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error error, ModbusTCP& interface, const char *name, vector<uint8_t> expected) {
  vector<uint8_t> msg = interface.generateErrorResponse(transactionID, serverID, functionCode, error);
  return testOutput(__func__, name, expected, msg);
}

bool TCP08(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error error, ModbusTCP& interface, const char *name, const char *expected) {
  vector<uint8_t> msg = interface.generateErrorResponse(transactionID, serverID, functionCode, error);
  return testOutput(__func__, name, makeVector(expected), msg);
}

// setup() called once at startup. 
// We will do all test here to have them run once
void setup()
{
// Init Serial
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

  // ******************************************************************************
  // Write test cases below this line!
  // ******************************************************************************

  // TCP test prerequisites
  uint16_t transactionID = 0x4711;

  // #### TCP, generateRequest(transactionID, serverID, functionCode) #01
  TCP01(transactionID, 0, 0x07, TestTCP, LNO(__LINE__) "invalid server id", "E1");
  TCP01(transactionID, 1, 0x01, TestTCP, LNO(__LINE__) "invalid FC for TCP01", "E6");
  TCP01(transactionID, 1, 0xA2, TestTCP, LNO(__LINE__) "invalid FC>127",       "01");
  TCP01(transactionID, 1, 0x07, TestTCP, LNO(__LINE__) "correct call 0x07",    "47 11 00 00 00 02 01 07");
  TCP01(transactionID, 1, 0x0B, TestTCP, LNO(__LINE__) "correct call 0x0B",    "47 11 00 00 00 02 01 0B");
  TCP01(transactionID, 1, 0x0C, TestTCP, LNO(__LINE__) "correct call 0x0C",    "47 11 00 00 00 02 01 0C");
  TCP01(transactionID, 1, 0x11, TestTCP, LNO(__LINE__) "correct call 0x11",    "47 11 00 00 00 02 01 11");

  // #### TCP, generateRequest(transactionID, serverID, functionCode, p1) #02
  TCP02(transactionID, 0, 0x18, 0x1122, TestTCP, LNO(__LINE__) "invalid server id",    "E1");
  TCP02(transactionID, 1, 0x01, 0x1122, TestTCP, LNO(__LINE__) "invalid FC for TCP02", "E6");
  TCP02(transactionID, 1, 0xA2, 0x1122, TestTCP, LNO(__LINE__) "invalid FC>127",       "01");
  TCP02(transactionID, 1, 0x18, 0x9A20, TestTCP, LNO(__LINE__) "correct call",         "47 11 00 00 00 04 01 18 9A 20");

  // #### TCP, generateRequest(transactionID, serverID, functionCode, p1, p2) #03
  TCP03(transactionID, 0, 0x01, 0x1122, 0x0002, TestTCP, LNO(__LINE__) "invalid server id",        "E1");
  TCP03(transactionID, 1, 0x07, 0x1122, 0x0002, TestTCP, LNO(__LINE__) "invalid FC for TCP03",     "E6");
  TCP03(transactionID, 1, 0xA2, 0x1122, 0x0002, TestTCP, LNO(__LINE__) "invalid FC>127",           "01");
  TCP03(transactionID, 1, 0x01, 0x1020, 2000,   TestTCP, LNO(__LINE__) "correct call 0x01 (2000)", "47 11 00 00 00 06 01 01 10 20 07 D0");
  TCP03(transactionID, 1, 0x01, 0x0300, 2001,   TestTCP, LNO(__LINE__) "illegal # coils 0x01",     "E7");
  TCP03(transactionID, 1, 0x01, 0x0300, 0x0000, TestTCP, LNO(__LINE__) "illegal coils=0 0x01",     "E7");
  TCP03(transactionID, 1, 0x01, 0x1020, 1,      TestTCP, LNO(__LINE__) "correct call 0x01 (1)",    "47 11 00 00 00 06 01 01 10 20 00 01");
  TCP03(transactionID, 1, 0x02, 0x1020, 2000,   TestTCP, LNO(__LINE__) "correct call 0x02 (2000)", "47 11 00 00 00 06 01 02 10 20 07 D0");
  TCP03(transactionID, 1, 0x02, 0x0300, 2001,   TestTCP, LNO(__LINE__) "illegal # inputs 0x02",    "E7");
  TCP03(transactionID, 1, 0x02, 0x0300, 0x0000, TestTCP, LNO(__LINE__) "illegal inputs=0 0x02",    "E7");
  TCP03(transactionID, 1, 0x02, 0x1020, 1,      TestTCP, LNO(__LINE__) "correct call 0x02 (1)",    "47 11 00 00 00 06 01 02 10 20 00 01");
  TCP03(transactionID, 1, 0x03, 0x1020, 125,    TestTCP, LNO(__LINE__) "correct call 0x03 (125)",  "47 11 00 00 00 06 01 03 10 20 00 7D");
  TCP03(transactionID, 1, 0x03, 0x0300, 126,    TestTCP, LNO(__LINE__) "illegal # registers 0x03", "E7");
  TCP03(transactionID, 1, 0x03, 0x0300, 0x0000, TestTCP, LNO(__LINE__) "illegal registers=0 0x03", "E7");
  TCP03(transactionID, 1, 0x03, 0x1020, 1,      TestTCP, LNO(__LINE__) "correct call 0x03 (1)",    "47 11 00 00 00 06 01 03 10 20 00 01");
  TCP03(transactionID, 1, 0x04, 0x1020, 125,    TestTCP, LNO(__LINE__) "correct call 0x04 (125)",  "47 11 00 00 00 06 01 04 10 20 00 7D");
  TCP03(transactionID, 1, 0x04, 0x0300, 126,    TestTCP, LNO(__LINE__) "illegal # registers 0x04", "E7");
  TCP03(transactionID, 1, 0x04, 0x0300, 0x0000, TestTCP, LNO(__LINE__) "illegal registers=0 0x04", "E7");
  TCP03(transactionID, 1, 0x04, 0x1020, 1,      TestTCP, LNO(__LINE__) "correct call 0x04 (1)",    "47 11 00 00 00 06 01 04 10 20 00 01");

  // ******************************************************************************
  // Write test cases above this line!
  // ******************************************************************************

  // Print summary.
  Serial.printf("Tests: %4d, passed: %4d\n", testsExecuted, testsPassed);
}

void loop() {
  // Nothing done here!
  delay(1000);
}
