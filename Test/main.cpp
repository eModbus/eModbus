// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "ModbusRTU.h"
#include "ModbusTCP.h"
#include <vector>
using std::vector;

#include "TCPstub.h"

#define STRINGIFY(x) #x
#define LNO(x) "line " STRINGIFY(x) " "

// Test prerequisites
TCPstub stub;
ModbusTCP TestTCP(stub, 2);               // ModbusTCP test instance.
ModbusRTU TestRTU(Serial);             // ModbusRTU test instance. Will never be started with begin()!
uint16_t testsExecuted = 0;            // Global test cases counter. Incremented in testOutput().
uint16_t testsPassed = 0;              // Global passed test cases counter. Incremented in testOutput().
bool printPassed = true;               // If true, testOutput will print passed tests as well.
TidMap testCasesByTID;
TokenMap testCasesByToken;

// testOutput:  takes the test function name called, the test case name and expected and recieved messages,
// compares both and prints out the result.
// If the test passed, true is returned - else false.
bool testOutput(const char *testname, const char *name, vector<uint8_t> expected, vector<uint8_t> received) {
  testsExecuted++;

  if (expected == received) {
    testsPassed++;
    if (printPassed) Serial.printf("%s, %s - passed.\n", testname, name);
    return true;
  } 

  Serial.printf("%s, %s - failed:\n", testname, name);
  Serial.print("   Expected:");
  for (auto& b : expected) {
    Serial.printf(" %02X", b);
  }
  if (expected.size() == 1) {
    ModbusError me((Error)expected[0]);
    Serial.printf(" %s", (const char *)me);
  }
  Serial.println();
  Serial.print("   Received:");
  for (auto& b : received) {
    Serial.printf(" %02X", b);
  }
  if (received.size() == 1) {
    ModbusError me((Error)received[0]);
    Serial.printf(" %s", (const char *)me);
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

void handleData(uint8_t serverID, uint8_t FC, const uint8_t *data, uint16_t len, uint32_t token) 
{
  // Look for the token in the TestCase map
  auto tc = testCasesByToken.find(token);
  if (tc != testCasesByToken.end()) {
    // Get a handier pointer for the TestCase found
    TestCase *myTest(tc->second);
    vector<uint8_t> response;
    response.reserve(len);
    response.resize(len);
    memcpy(response.data(), data, len);
    testOutput(myTest->testname, myTest->name, myTest->expected, response);
  } else {
    Serial.printf("Could not find test case for token %08X\n", token);
  }
}

void handleError(Error err, uint32_t token)
{
  // Look for the token in the TestCase map
  auto tc = testCasesByToken.find(token);
  if (tc != testCasesByToken.end()) {
    // Get a handier pointer for the TestCase found
    TestCase *myTest(tc->second);
    testOutput(myTest->testname, myTest->name, myTest->expected, { err });
  } else {
    Serial.printf("Could not find test case for token %08X\n", token);
  }
}

// setup() called once at startup. 
// We will do all test here to have them run once
void setup()
{
// Init Serial
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

  printPassed = false;          // Omit passed tests in output

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

  TCP03(transactionID, 1, 0x05, 0x1020, 0x0000, TestTCP, LNO(__LINE__) "correct call 0x05 (0)",    "47 11 00 00 00 06 01 05 10 20 00 00");
  TCP03(transactionID, 1, 0x05, 0x0300, 0x00FF, TestTCP, LNO(__LINE__) "illegal coil value 0x05",  "E7");
  TCP03(transactionID, 1, 0x05, 0x0300, 0x0FF0, TestTCP, LNO(__LINE__) "illegal coil value 0x05",  "E7");
  TCP03(transactionID, 1, 0x05, 0x1020, 0xFF00, TestTCP, LNO(__LINE__) "correct call 0x05 (0xFF00)", "47 11 00 00 00 06 01 05 10 20 FF 00");

  TCP03(transactionID, 1, 0x06, 0x0000, 0xFFFF, TestTCP, LNO(__LINE__) "correct call 0x06 (0xFFFF)", "47 11 00 00 00 06 01 06 00 00 FF FF");

  // #### TCP, generateRequest(transactionID, serverID, functionCode, p1, p2, p3) #04
  TCP04(transactionID, 0, 0x01, 0x1122, 0x0002, 0xBEAD, TestTCP, LNO(__LINE__) "invalid server id",        "E1");
  TCP04(transactionID, 1, 0x07, 0x1122, 0x0002, 0xBEAD, TestTCP, LNO(__LINE__) "invalid FC for TCP04",     "E6");
  TCP04(transactionID, 1, 0xA2, 0x1122, 0x0002, 0xBEAD, TestTCP, LNO(__LINE__) "invalid FC>127",           "01");

  TCP04(transactionID, 1, 0x16, 0x0000, 0xFAFF, 0xDEEB, TestTCP, LNO(__LINE__) "correct call 0x16", "47 11 00 00 00 08 01 16 00 00 FA FF DE EB");

  // Prepare arrays for generate() #05, #06 and #07
  uint16_t words[] = { 0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE, 0xFFFF };
  uint8_t bytes[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

  // #### TCP, generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfWords) #05
  TCP05(transactionID, 0, 0x01, 0x1122, 0x0002,  4, words, TestTCP, LNO(__LINE__) "invalid server id",        "E1");
  TCP05(transactionID, 1, 0x07, 0x1122, 0x0002,  4, words, TestTCP, LNO(__LINE__) "invalid FC for TCP05",     "E6");
  TCP05(transactionID, 1, 0xA2, 0x1122, 0x0002,  4, words, TestTCP, LNO(__LINE__) "invalid FC>127",           "01");

  TCP05(transactionID, 1, 0x10, 0x1020, 6, 12, words, TestTCP, LNO(__LINE__) "correct call 0x10", 
        "47 11 00 00 00 13 01 10 10 20 00 06 0C 00 00 11 11 22 22 33 33 44 44 55 55");
  TCP05(transactionID, 1, 0x10, 0x1020, 5, 12, words, TestTCP, LNO(__LINE__) "wrong word count 0x10", "03");
  TCP05(transactionID, 1, 0x10, 0x1020, 0, 12, words, TestTCP, LNO(__LINE__) "illegal word count(0) 0x10", "E7");
  TCP05(transactionID, 1, 0x10, 0x1020, 124, 12, words, TestTCP, LNO(__LINE__) "illegal word count(124) 0x10", "E7");
  TCP05(transactionID, 1, 0x10, 0x1020, 1, 2, words, TestTCP, LNO(__LINE__) "correct call 0x10", 
        "47 11 00 00 00 09 01 10 10 20 00 01 02 00 00");

  // #### TCP, generateRequest(transactionID, serverID, functionCode, p1, p2, count, arrayOfBytes) #06
  TCP06(transactionID, 0, 0x01, 0x1122, 0x0002,  1, bytes, TestTCP, LNO(__LINE__) "invalid server id",        "E1");
  TCP06(transactionID, 1, 0x07, 0x1122, 0x0002,  1, bytes, TestTCP, LNO(__LINE__) "invalid FC for TCP06",     "E6");
  TCP06(transactionID, 1, 0xA2, 0x1122, 0x0002,  1, bytes, TestTCP, LNO(__LINE__) "invalid FC>127",           "01");

  TCP06(transactionID, 1, 0x0F, 0x1020, 31, 4, bytes, TestTCP, LNO(__LINE__) "correct call 0x0F", 
        "47 11 00 00 00 0B 01 0F 10 20 00 1F 04 00 11 22 33");
  TCP06(transactionID, 1, 0x0F, 0x1020, 5, 12, bytes, TestTCP, LNO(__LINE__) "wrong word count 0x0F", "03");
  TCP06(transactionID, 1, 0x0F, 0x1020, 0, 1, bytes, TestTCP, LNO(__LINE__) "illegal word count(0) 0x0F", "E7");
  TCP06(transactionID, 1, 0x0F, 0x1020, 2001, 251, bytes, TestTCP, LNO(__LINE__) "illegal word count(2001) 0x0F", "E7");
  TCP06(transactionID, 1, 0x0F, 0x1020, 1, 1, bytes, TestTCP, LNO(__LINE__) "correct call 0x10", 
        "47 11 00 00 00 08 01 0F 10 20 00 01 01 00");

  // #### TCP, generateRequest(transactionID, serverID, functionCode, count, arrayOfBytes) #07
  TCP07(transactionID, 0, 0x20, 0x0002, bytes, TestTCP, LNO(__LINE__) "invalid server id",        "E1");
  TCP07(transactionID, 1, 0xA2, 0x0002, bytes, TestTCP, LNO(__LINE__) "invalid FC>127",           "01");

  TCP07(transactionID, 1, 0x42, 0x0007, bytes, TestTCP, LNO(__LINE__) "correct call",
       "47 11 00 00 00 09 01 42 00 11 22 33 44 55 66");
  TCP07(transactionID, 1, 0x42, 0x0000, bytes, TestTCP, LNO(__LINE__) "correct call 0 bytes", "47 11 00 00 00 02 01 42");

  // #### TCP, generateErrorResponse(transactionID, serverID, functionCode, errorCode) #08
  TCP08(transactionID, 0, 0x03, (Error)0x02, TestTCP, LNO(__LINE__) "invalid server id",      "E1");
  TCP08(transactionID, 1, 0x9F, (Error)0x02, TestTCP, LNO(__LINE__) "invalid FC>127",         "01");

  TCP08(transactionID, 1, 0x05, (Error)0xE1, TestTCP, LNO(__LINE__) "correct call", "47 11 00 00 00 03 01 85 E1");
  TCP08(transactionID, 1, 0x05, (Error)0x73, TestTCP, LNO(__LINE__) "correct call (unk.err)", "47 11 00 00 00 03 01 85 73");

  // RTU tests starting here

  // #### RTU, generateRequest(serverID, functionCode) #01
  RTU01(0, 0x07, TestRTU, LNO(__LINE__) "invalid server id", "E1");
  RTU01(1, 0x01, TestRTU, LNO(__LINE__) "invalid FC for RTU01", "E6");
  RTU01(1, 0xA2, TestRTU, LNO(__LINE__) "invalid FC>127",       "01");

  RTU01(1, 0x07, TestRTU, LNO(__LINE__) "correct call 0x07",    "01 07 41 E2");
  RTU01(1, 0x0B, TestRTU, LNO(__LINE__) "correct call 0x0B",    "01 0B 41 E7");
  RTU01(1, 0x0C, TestRTU, LNO(__LINE__) "correct call 0x0C",    "01 0C 00 25");
  RTU01(1, 0x11, TestRTU, LNO(__LINE__) "correct call 0x11",    "01 11 C0 2C");

  // #### RTU, generateRequest(serverID, functionCode, p1) #02
  RTU02(0, 0x18, 0x1122, TestRTU, LNO(__LINE__) "invalid server id",    "E1");
  RTU02(1, 0x01, 0x1122, TestRTU, LNO(__LINE__) "invalid FC for RTU02", "E6");
  RTU02(1, 0xA2, 0x1122, TestRTU, LNO(__LINE__) "invalid FC>127",       "01");

  RTU02(1, 0x18, 0x9A20, TestRTU, LNO(__LINE__) "correct call",         "01 18 9A 20 EA A7");

  // #### RTU, generateRequest(serverID, functionCode, p1, p2) #03
  RTU03(0, 0x01, 0x1122, 0x0002, TestRTU, LNO(__LINE__) "invalid server id",        "E1");
  RTU03(1, 0x07, 0x1122, 0x0002, TestRTU, LNO(__LINE__) "invalid FC for RTU03",     "E6");
  RTU03(1, 0xA2, 0x1122, 0x0002, TestRTU, LNO(__LINE__) "invalid FC>127",           "01");

  RTU03(1, 0x01, 0x1020, 2000,   TestRTU, LNO(__LINE__) "correct call 0x01 (2000)", "01 01 10 20 07 D0 3A AC");
  RTU03(1, 0x01, 0x0300, 2001,   TestRTU, LNO(__LINE__) "illegal # coils 0x01",     "E7");
  RTU03(1, 0x01, 0x0300, 0x0000, TestRTU, LNO(__LINE__) "illegal coils=0 0x01",     "E7");
  RTU03(1, 0x01, 0x1020, 1,      TestRTU, LNO(__LINE__) "correct call 0x01 (1)",    "01 01 10 20 00 01 F8 C0");

  RTU03(1, 0x02, 0x1020, 2000,   TestRTU, LNO(__LINE__) "correct call 0x02 (2000)", "01 02 10 20 07 D0 7E AC");
  RTU03(1, 0x02, 0x0300, 2001,   TestRTU, LNO(__LINE__) "illegal # inputs 0x02",    "E7");
  RTU03(1, 0x02, 0x0300, 0x0000, TestRTU, LNO(__LINE__) "illegal inputs=0 0x02",    "E7");
  RTU03(1, 0x02, 0x1020, 1,      TestRTU, LNO(__LINE__) "correct call 0x02 (1)",    "01 02 10 20 00 01 BC C0");

  RTU03(1, 0x03, 0x1020, 125,    TestRTU, LNO(__LINE__) "correct call 0x03 (125)",  "01 03 10 20 00 7D 80 E1");
  RTU03(1, 0x03, 0x0300, 126,    TestRTU, LNO(__LINE__) "illegal # registers 0x03", "E7");
  RTU03(1, 0x03, 0x0300, 0x0000, TestRTU, LNO(__LINE__) "illegal registers=0 0x03", "E7");
  RTU03(1, 0x03, 0x1020, 1,      TestRTU, LNO(__LINE__) "correct call 0x03 (1)",    "01 03 10 20 00 01 81 00");

  RTU03(1, 0x04, 0x1020, 125,    TestRTU, LNO(__LINE__) "correct call 0x04 (125)",  "01 04 10 20 00 7D 35 21");
  RTU03(1, 0x04, 0x0300, 126,    TestRTU, LNO(__LINE__) "illegal # registers 0x04", "E7");
  RTU03(1, 0x04, 0x0300, 0x0000, TestRTU, LNO(__LINE__) "illegal registers=0 0x04", "E7");
  RTU03(1, 0x04, 0x1020, 1,      TestRTU, LNO(__LINE__) "correct call 0x04 (1)",    "01 04 10 20 00 01 34 C0");

  RTU03(1, 0x05, 0x1020, 0x0000, TestRTU, LNO(__LINE__) "correct call 0x05 (0)",    "01 05 10 20 00 00 C8 C0");
  RTU03(1, 0x05, 0x0300, 0x00FF, TestRTU, LNO(__LINE__) "illegal coil value 0x05",  "E7");
  RTU03(1, 0x05, 0x0300, 0x0FF0, TestRTU, LNO(__LINE__) "illegal coil value 0x05",  "E7");
  RTU03(1, 0x05, 0x1020, 0xFF00, TestRTU, LNO(__LINE__) "correct call 0x05 (0xFF00)", "01 05 10 20 FF 00 89 30");

  RTU03(1, 0x06, 0x0000, 0xFFFF, TestRTU, LNO(__LINE__) "correct call 0x06 (0xFFFF)", "01 06 00 00 FF FF 88 7A");

  // #### RTU, generateRequest(serverID, functionCode, p1, p2, p3) #04
  RTU04(0, 0x01, 0x1122, 0x0002, 0xBEAD, TestRTU, LNO(__LINE__) "invalid server id",        "E1");
  RTU04(1, 0x07, 0x1122, 0x0002, 0xBEAD, TestRTU, LNO(__LINE__) "invalid FC for RTU04",     "E6");
  RTU04(1, 0xA2, 0x1122, 0x0002, 0xBEAD, TestRTU, LNO(__LINE__) "invalid FC>127",           "01");

  RTU04(1, 0x16, 0x0000, 0xFAFF, 0xDEEB, TestRTU, LNO(__LINE__) "correct call 0x16", "01 16 00 00 FA FF DE EB EF 01");

  // #### RTU, generateRequest(serverID, functionCode, p1, p2, count, arrayOfWords) #05
  RTU05(0, 0x01, 0x1122, 0x0002,  4, words, TestRTU, LNO(__LINE__) "invalid server id",        "E1");
  RTU05(1, 0x07, 0x1122, 0x0002,  4, words, TestRTU, LNO(__LINE__) "invalid FC for RTU05",     "E6");
  RTU05(1, 0xA2, 0x1122, 0x0002,  4, words, TestRTU, LNO(__LINE__) "invalid FC>127",           "01");

  RTU05(1, 0x10, 0x1020, 6, 12, words, TestRTU, LNO(__LINE__) "correct call 0x10", 
        "01 10 10 20 00 06 0C 00 00 11 11 22 22 33 33 44 44 55 55 A5 44");
  RTU05(1, 0x10, 0x1020, 5, 12, words, TestRTU, LNO(__LINE__) "wrong word count 0x10", "03");
  RTU05(1, 0x10, 0x1020, 0, 12, words, TestRTU, LNO(__LINE__) "illegal word count(0) 0x10", "E7");
  RTU05(1, 0x10, 0x1020, 124, 12, words, TestRTU, LNO(__LINE__) "illegal word count(124) 0x10", "E7");
  RTU05(1, 0x10, 0x1020, 1, 2, words, TestRTU, LNO(__LINE__) "correct call 0x10", 
        "01 10 10 20 00 01 02 00 00 B0 F1");

  // #### RTU, generateRequest(serverID, functionCode, p1, p2, count, arrayOfBytes) #06
  RTU06(0, 0x01, 0x1122, 0x0002,  1, bytes, TestRTU, LNO(__LINE__) "invalid server id",        "E1");
  RTU06(1, 0x07, 0x1122, 0x0002,  1, bytes, TestRTU, LNO(__LINE__) "invalid FC for RTU06",     "E6");
  RTU06(1, 0xA2, 0x1122, 0x0002,  1, bytes, TestRTU, LNO(__LINE__) "invalid FC>127",           "01");

  RTU06(1, 0x0F, 0x1020, 31, 4, bytes, TestRTU, LNO(__LINE__) "correct call 0x0F", 
        "01 0F 10 20 00 1F 04 00 11 22 33 06 EF");
  RTU06(1, 0x0F, 0x1020, 5, 12, bytes, TestRTU, LNO(__LINE__) "wrong word count 0x0F", "03");
  RTU06(1, 0x0F, 0x1020, 0, 1, bytes, TestRTU, LNO(__LINE__) "illegal word count(0) 0x0F", "E7");
  RTU06(1, 0x0F, 0x1020, 2001, 251, bytes, TestRTU, LNO(__LINE__) "illegal word count(2001) 0x0F", "E7");
  RTU06(1, 0x0F, 0x1020, 1, 1, bytes, TestRTU, LNO(__LINE__) "correct call 0x10", 
        "01 0F 10 20 00 01 01 00 AD C0");

  // #### RTU, generateRequest(serverID, functionCode, count, arrayOfBytes) #07
  RTU07(0, 0x20, 0x0002, bytes, TestRTU, LNO(__LINE__) "invalid server id",        "E1");
  RTU07(1, 0xA2, 0x0002, bytes, TestRTU, LNO(__LINE__) "invalid FC>127",           "01");

  RTU07(1, 0x42, 0x0007, bytes, TestRTU, LNO(__LINE__) "correct call",
       "01 42 00 11 22 33 44 55 66 89 E4");
  RTU07(1, 0x42, 0x0000, bytes, TestRTU, LNO(__LINE__) "correct call 0 bytes", "01 42 80 11");

  // #### RTU, generateErrorResponse(serverID, functionCode, errorCode) #08
  RTU08(0, 0x03, (Error)0x02, TestRTU, LNO(__LINE__) "invalid server id",      "E1");
  RTU08(1, 0x9F, (Error)0x02, TestRTU, LNO(__LINE__) "invalid FC>127",         "01");

  RTU08(1, 0x05, (Error)0xE1, TestRTU, LNO(__LINE__) "correct call", "01 85 E1 82 D8");
  RTU08(1, 0x05, (Error)0x73, TestRTU, LNO(__LINE__) "correct call (unk.err)", "01 85 73 03 75");

  // ******************************************************************************
  // Write test cases above this line!
  // ******************************************************************************

  // Print summary.
  Serial.printf("Generate messages tests: %4d, passed: %4d\n", testsExecuted, testsPassed);


  // ******************************************************************************
  // Tests using the complete turnaround next. TCP is simulated by TCPstub stub!
  // ******************************************************************************

  printPassed = true;

  // Some prerequisites 
  IPAddress testHost = IPAddress(192, 166, 1, 1);
  IPAddress testHost2 = IPAddress(26, 183, 4, 22);

  // Register onData and onError handlers
  TestTCP.onDataHandler(&handleData);
  TestTCP.onErrorHandler(&handleError);

  // Start ModbusTCP background task
  TestTCP.begin();

  // Start TCP stub with initial identity testhost:502
  // testCasesByTID is the map to find the matching test case in the worker task
  stub.begin(&testCasesByTID, testHost, 502);

  // ****************************************************************************************
  // Example test case.
  // Setting a different identity is optional, target should be set to avoid confusion
  TestTCP.setTarget(testHost, 502, 2000, 200);
  stub.setIdentity(testHost, 502);

  // Define new TestCase object. This is holding all data the TCPstub worker and the
  // onData and onError handlers will need to reagrd the correct test case.
  TestCase *tc = new TestCase { 
    // getMessageCount will be used internally to generate the transaction ID, so get a copy
    .transactionID = (uint16_t)(TestTCP.getMessageCount() & 0xFFFF),
    // The token value _must_ be different for each test case!!!
    .token = 0xDEADBEEF,
    // The worker can be made to delay its response by the time given here
    .delayTime = 0,
    // function and test case name to be printed with the result.
    .name = LNO(__LINE__),
    .testname = "Example test case",
    // A vector of the response the worker shall return
    .response = makeVector("01 04 08 00 00 11 11 22 22 33 33"),
    // A vector of the expected data arriving in onData/onError
    .expected = makeVector("01 03 08 00 00 11 11 22 22 33 33")
  };

  // Now create an entry in both reference maps (by TID and by token) for the test case
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;

  // Finally execute the test call
  Error e = TestTCP.addRequest(1, 0x03, 1, 4, tc->token);
  // Did the call immediately return an error?
  if (e != SUCCESS) {
    // Yes, give it to the test result examiner
    testOutput(tc->testname, tc->name, tc->expected, { e });
  }
}

void loop() {
  // Nothing done here!
  delay(1000);
}
