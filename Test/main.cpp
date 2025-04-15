// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include <Arduino.h>
#include "ModbusServerRTU.h"
#include "ModbusClientRTU.h"
#include "ModbusClientTCP.h"
#include "ModbusServerWiFi.h"
#include "ModbusBridgeWiFi.h"

#undef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
#include "Logging.h"

#include "TCPstub.h"
#include "CoilData.h"

#define STRINGIFY(x) #x
#define LNO(x) "line " STRINGIFY(x) " "

// Define alternate LOGDEVICE class to test rerouting of the log output
class AltLog : public Print {
  size_t write(uint8_t c) override {
    Serial.write(c);
    return 1;
  }
  size_t write(const uint8_t *buffer, size_t size) override {
    Serial.write(buffer, size);
    return size;
  }
};
AltLog a;

// RTS callback test function
uint32_t ExpectedToggles = 0;
uint32_t cntRTShigh = 0;
uint32_t cntRTSlow = 0;
void RTStest(bool level) {
  if (level) cntRTShigh++;
  else       cntRTSlow++;
}

// Test prerequisites
TCPstub stub;
ModbusClientTCP TestTCP(stub, 2);               // ModbusClientTCP test instance for stub use.
WiFiClient wc;
ModbusClientTCP TestClientWiFi(wc, 25);         // ModbusClientTCP test instance for WiFi loopback use.
ModbusClientRTU RTUclient(GPIO_NUM_4);          // ModbusClientRTU test instance. Connect a LED to GPIO pin 4 to see the RTS toggle.
ModbusServerRTU RTUserver(20000, RTStest);      // ModbusServerRTU instance
ModbusServerWiFi MBserver;                      // ModbusServerWiFi instance
ModbusBridgeWiFi Bridge;                        // Modbus bridge instance
IPAddress ip = {127,   0,   0,   1};            // IP address of ModbusServerWiFi (loopback IF)
uint16_t port = 502;                            // port of modbus server
uint16_t testsExecuted = 0;                     // test cases counter. Incremented in testOutput().
uint16_t testsPassed = 0;                       // passed test cases counter. Incremented in testOutput().
uint16_t testsExecutedGlobal = 0;               // Global test cases counter. Incremented in testOutput().
uint16_t testsPassedGlobal = 0;                 // Global passed test cases counter. Incremented in testOutput().
bool printPassed = false;                       // If true, testOutput will print passed tests as well.
TidMap testCasesByTID;
TokenMap testCasesByToken;
uint32_t highestTokenProcessed = 0;
uint16_t broadcastCnt = 0;

#define WAIT_FOR_FINISH(x) while ((highestTokenProcessed < (Token - 1)) && (x.pendingRequests() != 0)) { delay(100); }

uint16_t memo[32];                     // Test server memory: 32 words

ModbusMessage empty;                   // Empty message for initializers

// Worker function for function code 0x03
ModbusMessage FC03(ModbusMessage request) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read
  ModbusMessage response;

  // Get addr and words from data array. Values are MSB-first, get() will convert to binary
  request.get(2, addr, wrds);

  // address valid?
  if (!addr || addr > 32) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Number of words valid?
  if (!wrds || (addr + wrds) > 32) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(wrds * 2));

  // Loop over all words to be sent
  for (uint16_t i = 0; i < wrds; i++) {
    // Add word MSB-first to response buffer
    // serverID 1 gets the real values, all others the inverted values
    response.add((uint16_t)((request.getServerID() == 1) ? memo[addr + i] : ~memo[addr + i]));
  }

  // Return the data response
  return response;
}

// Worker function function code 0x06
ModbusMessage FC06(ModbusMessage request) {
  uint16_t addr = 0;        // Start address to read
  uint16_t value = 0;       // New value for register
  ModbusMessage response;

  // Get addr and value from data array. Values are MSB-first, getValue() will convert to binary
  request.get(2, addr);
  request.get(4, value);

  // address valid?
  if (!addr || addr > 32) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Fake data error - 0x0000 or 0xFFFF will not be accepted
  if (!value || value == 0xFFFF) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_VALUE);
    return response;
  }

  // Fill in new value.
  memo[addr] = value;

  // Return the ECHO response
  return ECHO_RESPONSE;
}

// Worker function function code 0x41 (user defined)
ModbusMessage FC41(const ModbusMessage& request) {
  // return nothing to test timeout
  return NIL_RESPONSE;
}

// Worker function for broadcast requests
void BroadcastWorker(const ModbusMessage& request) {
  // Count broadcasts
  broadcastCnt++;
}

// Worker for sniffing the messages
void Sniffer(const ModbusMessage& m) {
  Serial.printf("Sniff: ");
  for (auto b : m) {
    Serial.printf("%02X ", b);
  }
  Serial.println();
}

// Worker function for any function code
ModbusMessage FCany(ModbusMessage request) {
  // return recognizable text
  ModbusMessage response;
  uint8_t resp[] = "ANY FC";

  response.add(request.getServerID(), request.getFunctionCode());
  response.add(resp, 6);
  return response;
}

// Worker function for any server ID
ModbusMessage SVany(ModbusMessage request) {
  // return recognizable text
  ModbusMessage response;
  uint8_t resp[] = "ANY ID";

  response.add(request.getServerID(), request.getFunctionCode());
  response.add(resp, 6);
  return response;
}

// Worker function for any server ID and any function code
ModbusMessage SVFCany(ModbusMessage request) {
  // return recognizable text
  ModbusMessage response;
  uint8_t resp[] = "ANY ID/FC";

  response.add(request.getServerID(), request.getFunctionCode());
  response.add(resp, 9);
  return response;
}

// Worker function for large message tests
ModbusMessage FC44(ModbusMessage request) {
  ModbusMessage response;
  uint16_t offs = 2;
  uint16_t value = 0;
  uint16_t correctValues = 0;

  for (uint16_t i = 0; i < (request.size() - 2) / 2; ++i) {
    offs = request.get(offs, value);
    if (value == i + 1) correctValues++;
  }
  response.add(request.getServerID(), request.getFunctionCode(), correctValues);
  return response;
}

// 2nd Worker function for large message tests
ModbusMessage FC45(ModbusMessage request) {
  ModbusMessage response;

  response.add(request.getServerID(), request.getFunctionCode(), request.size());
  return response;
}

// testOutput:  takes the test function name called, the test case name and expected and recieved messages,
// compares both and prints out the result.
// If the test passed, true is returned - else false.
bool testOutput(const char *testname, const char *name, ModbusMessage expected, ModbusMessage received) {
  testsExecuted++;

  if (expected == received) {
    testsPassed++;
    if (printPassed) Serial.printf("%s, %s - passed.\n", testname, name);
    return true;
  } 

  Serial.printf("%s, %s - failed:\n", testname, name);
  Serial.print("   Expected:");
  for (const auto& b : expected) {
    Serial.printf(" %02X", b);
  }
  if (expected.size() == 1) {
    ModbusError me((Error)expected[0]);
    Serial.printf(" %s", static_cast<const char *>(me));
  }
  Serial.println();
  Serial.print("   Received:");
  for (const auto& b : received) {
    Serial.printf(" %02X", b);
  }
  if (received.size() == 1) {
    ModbusError me((Error)received[0]);
    Serial.printf(" %s", static_cast<const char *>(me));
  }
  Serial.println();
  
  return false;
}

// Helper function to convert hexadecimal ([0-9A-F]) digits in a char array into a vector of bytes
ModbusMessage makeVector(const char *text) {
  ModbusMessage rv;            // The vector to be returned
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

// The test functions are named as follows: "MSG" and a 2-digit number denoting the 
// underlying setMessage() function. The "MSG08()" functions will call setError() instead
//
// The last two calling arguments always are:
// name: pointer to a char array holding the test case description
// expected: the ModbusMessage, that normally should result from the call. There are two variants for each
//           of the test functions, one accepting the ModbusMessage, the other will expect a char array
//           with hexadecimal digits like ("12 34 56 78 9A BC DE F0"). All characters except [0-9A-F] are ignored!
//
// The leading [n] calling arguments are the same that will be used for the respective setMessage() calls.
bool MSG01(uint8_t serverID, uint8_t functionCode, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG01(uint8_t serverID, uint8_t functionCode, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG02(uint8_t serverID, uint8_t functionCode, uint16_t p1, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG02(uint8_t serverID, uint8_t functionCode, uint16_t p1, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG03(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG03(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG04(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, p3);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG04(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, p3);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG05(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, count, arrayOfWords);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG05(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, count, arrayOfWords);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG06(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, count, arrayOfBytes);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG06(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, p1, p2, count, arrayOfBytes);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG07(uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, count, arrayOfBytes);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, expected, msg);
}

bool MSG07(uint8_t serverID, uint8_t functionCode, uint8_t count, uint8_t *arrayOfBytes, const char *name, const char *expected) {
  ModbusMessage msg;
  Error e = msg.setMessage(serverID, functionCode, count, arrayOfBytes);
  if (e != SUCCESS) {
    msg.setError(serverID, functionCode, e);
  }
  return testOutput(__func__, name, makeVector(expected), msg);
}

bool MSG08(uint8_t serverID, uint8_t functionCode, Error error, const char *name, const ModbusMessage& expected) {
  ModbusMessage msg;
  msg.setError(serverID, functionCode, error);
  return testOutput(__func__, name, expected, msg);
}

bool MSG08(uint8_t serverID, uint8_t functionCode, Error error, const char *name, const char *expected) {
  ModbusMessage msg;
  msg.setError(serverID, functionCode, error);
  return testOutput(__func__, name, makeVector(expected), msg);
}

void handleData(ModbusMessage response, uint32_t token) 
{
  // catch highest token processed
  if (highestTokenProcessed < token) highestTokenProcessed = token;
  // Look for the token in the TestCase map
  auto tc = testCasesByToken.find(token);
  if (tc != testCasesByToken.end()) {
    // Get a handier pointer for the TestCase found
    const TestCase *myTest(tc->second);
    testOutput(myTest->testname, myTest->name, myTest->expected, response);
  } else {
    Serial.printf("Could not find test case for token %08X\n", token);
  }
}

void handleError(Error err, uint32_t token)
{
  // catch highest token processed
  if (highestTokenProcessed < token) highestTokenProcessed = token;
  // Look for the token in the TestCase map
  auto tc = testCasesByToken.find(token);
  if (tc != testCasesByToken.end()) {
    // Get a handier pointer for the TestCase found
    const TestCase *myTest(tc->second);
    ModbusMessage response;
    response.add(err);
    testOutput(myTest->testname, myTest->name, myTest->expected, response);
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

  LOGDEVICE = &a;

  // Init WiFi with fake ssid/pass (we will use loopback only)
  WiFi.begin("foo", "bar");

  // Wait 10s for output monitor to initialize
  delay(10000);

  Serial.println("-----> Some timeout tests may take a while. Wait patiently for 'All finished.'");

  // ******************************************************************************
  // Write test cases below this line!
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  // #### MSG, setMessage(serverID, functionCode) #01
  MSG01(0, 0x07, LNO(__LINE__) "invalid server id",    "00 87 E1");
  MSG01(1, 0x01, LNO(__LINE__) "invalid FC for MSG01", "01 81 E6");
  MSG01(1, 0xA2, LNO(__LINE__) "invalid FC>127",       "01 A2 01");

  MSG01(1, 0x07, LNO(__LINE__) "correct call 0x07",    "01 07");
  MSG01(1, 0x0B, LNO(__LINE__) "correct call 0x0B",    "01 0B");
  MSG01(1, 0x0C, LNO(__LINE__) "correct call 0x0C",    "01 0C");
  MSG01(1, 0x11, LNO(__LINE__) "correct call 0x11",    "01 11");

  // #### MSG, setMessage(serverID, functionCode, p1) #02
  MSG02(0, 0x18, 0x1122, LNO(__LINE__) "invalid server id",    "00 98 E1");
  MSG02(1, 0x01, 0x1122, LNO(__LINE__) "invalid FC for MSG02", "01 81 E6");
  MSG02(1, 0xA2, 0x1122, LNO(__LINE__) "invalid FC>127",       "01 A2 01");

  MSG02(1, 0x18, 0x9A20, LNO(__LINE__) "correct call",         "01 18 9A 20");

  // #### MSG, setMessage(serverID, functionCode, p1, p2) #03
  MSG03(0, 0x01, 0x1122, 0x0002, LNO(__LINE__) "invalid server id",        "00 81 E1");
  MSG03(1, 0x07, 0x1122, 0x0002, LNO(__LINE__) "invalid FC for MSG03",     "01 87 E6");
  MSG03(1, 0xA2, 0x1122, 0x0002, LNO(__LINE__) "invalid FC>127",           "01 A2 01");

  MSG03(1, 0x01, 0x1020, 2000,   LNO(__LINE__) "correct call 0x01 (2000)", "01 01 10 20 07 D0");
  MSG03(1, 0x01, 0x0300, 2001,   LNO(__LINE__) "illegal # coils 0x01",     "01 81 E7");
  MSG03(1, 0x01, 0x0300, 0x0000, LNO(__LINE__) "illegal coils=0 0x01",     "01 81 E7");
  MSG03(1, 0x01, 0x1020, 1,      LNO(__LINE__) "correct call 0x01 (1)",    "01 01 10 20 00 01");

  MSG03(1, 0x02, 0x1020, 2000,   LNO(__LINE__) "correct call 0x02 (2000)", "01 02 10 20 07 D0");
  MSG03(1, 0x02, 0x0300, 2001,   LNO(__LINE__) "illegal # inputs 0x02",    "01 82 E7");
  MSG03(1, 0x02, 0x0300, 0x0000, LNO(__LINE__) "illegal inputs=0 0x02",    "01 82 E7");
  MSG03(1, 0x02, 0x1020, 1,      LNO(__LINE__) "correct call 0x02 (1)",    "01 02 10 20 00 01");

  MSG03(1, 0x03, 0x1020, 125,    LNO(__LINE__) "correct call 0x03 (125)",  "01 03 10 20 00 7D");
  MSG03(1, 0x03, 0x0300, 126,    LNO(__LINE__) "illegal # registers 0x03", "01 83 E7");
  MSG03(1, 0x03, 0x0300, 0x0000, LNO(__LINE__) "illegal registers=0 0x03", "01 83 E7");
  MSG03(1, 0x03, 0x1020, 1,      LNO(__LINE__) "correct call 0x03 (1)",    "01 03 10 20 00 01");

  MSG03(1, 0x04, 0x1020, 125,    LNO(__LINE__) "correct call 0x04 (125)",  "01 04 10 20 00 7D");
  MSG03(1, 0x04, 0x0300, 126,    LNO(__LINE__) "illegal # registers 0x04", "01 84 E7");
  MSG03(1, 0x04, 0x0300, 0x0000, LNO(__LINE__) "illegal registers=0 0x04", "01 84 E7");
  MSG03(1, 0x04, 0x1020, 1,      LNO(__LINE__) "correct call 0x04 (1)",    "01 04 10 20 00 01");

  MSG03(1, 0x05, 0x1020, 0x0000, LNO(__LINE__) "correct call 0x05 (0)",    "01 05 10 20 00 00");
  MSG03(1, 0x05, 0x0300, 0x00FF, LNO(__LINE__) "illegal coil value 0x05",  "01 85 E7");
  MSG03(1, 0x05, 0x0300, 0x0FF0, LNO(__LINE__) "illegal coil value 0x05",  "01 85 E7");
  MSG03(1, 0x05, 0x1020, 0xFF00, LNO(__LINE__) "correct call 0x05 (0xFF00)", "01 05 10 20 FF 00");

  MSG03(1, 0x06, 0x0000, 0xFFFF, LNO(__LINE__) "correct call 0x06 (0xFFFF)", "01 06 00 00 FF FF");

  // #### MSG, setMessage(serverID, functionCode, p1, p2, p3) #04
  MSG04(0, 0x01, 0x1122, 0x0002, 0xBEAD, LNO(__LINE__) "invalid server id",        "00 81 E1");
  MSG04(1, 0x07, 0x1122, 0x0002, 0xBEAD, LNO(__LINE__) "invalid FC for MSG04",     "01 87 E6");
  MSG04(1, 0xA2, 0x1122, 0x0002, 0xBEAD, LNO(__LINE__) "invalid FC>127",           "01 A2 01");

  MSG04(1, 0x16, 0x0000, 0xFAFF, 0xDEEB, LNO(__LINE__) "correct call 0x16", "01 16 00 00 FA FF DE EB");

  // Prepare arrays for setMessage() #05, #06 and #07
  uint16_t words[] = { 0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE, 0xFFFF };
  uint8_t bytes[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

  // #### MSG, setMessage(serverID, functionCode, p1, p2, count, arrayOfWords) #05
  MSG05(0, 0x01, 0x1122, 0x0002,  4, words, LNO(__LINE__) "invalid server id",        "00 81 E1");
  MSG05(1, 0x07, 0x1122, 0x0002,  4, words, LNO(__LINE__) "invalid FC for MSG05",     "01 87 E6");
  MSG05(1, 0xA2, 0x1122, 0x0002,  4, words, LNO(__LINE__) "invalid FC>127",           "01 A2 01");

  MSG05(1, 0x10, 0x1020, 6, 12, words, LNO(__LINE__) "correct call 0x10", 
        "01 10 10 20 00 06 0C 00 00 11 11 22 22 33 33 44 44 55 55");
  MSG05(1, 0x10, 0x1020, 5, 12, words, LNO(__LINE__) "wrong word count 0x10", "01 90 03");
  MSG05(1, 0x10, 0x1020, 0, 12, words, LNO(__LINE__) "illegal word count(0) 0x10", "01 90 E7");
  MSG05(1, 0x10, 0x1020, 124, 12, words, LNO(__LINE__) "illegal word count(124) 0x10", "01 90 E7");
  MSG05(1, 0x10, 0x1020, 1, 2, words, LNO(__LINE__) "correct call 0x10", 
        "01 10 10 20 00 01 02 00 00");

  // #### MSG, setMessage(serverID, functionCode, p1, p2, count, arrayOfBytes) #06
  MSG06(0, 0x01, 0x1122, 0x0002,  1, bytes, LNO(__LINE__) "invalid server id",        "00 81 E1");
  MSG06(1, 0x07, 0x1122, 0x0002,  1, bytes, LNO(__LINE__) "invalid FC for MSG06",     "01 87 E6");
  MSG06(1, 0xA2, 0x1122, 0x0002,  1, bytes, LNO(__LINE__) "invalid FC>127",           "01 A2 01");

  MSG06(1, 0x0F, 0x1020, 31, 4, bytes, LNO(__LINE__) "correct call 0x0F", 
        "01 0F 10 20 00 1F 04 00 11 22 33");
  MSG06(1, 0x0F, 0x1020, 5, 12, bytes, LNO(__LINE__) "wrong word count 0x0F", "01 8F 03");
  MSG06(1, 0x0F, 0x1020, 0, 1, bytes, LNO(__LINE__) "illegal word count(0) 0x0F", "01 8F E7");
  MSG06(1, 0x0F, 0x1020, 2001, 251, bytes, LNO(__LINE__) "illegal word count(2001) 0x0F", "01 8F E7");
  MSG06(1, 0x0F, 0x1020, 1, 1, bytes, LNO(__LINE__) "correct call 0x10", 
        "01 0F 10 20 00 01 01 00");

  // #### MSG, setMessage(serverID, functionCode, count, arrayOfBytes) #07
  MSG07(0, 0x20, 0x0002, bytes, LNO(__LINE__) "invalid server id",        "00 A0 E1");
  MSG07(1, 0xA2, 0x0002, bytes, LNO(__LINE__) "invalid FC>127",           "01 A2 01");

  MSG07(1, 0x42, 0x0007, bytes, LNO(__LINE__) "correct call",
       "01 42 00 11 22 33 44 55 66");
  MSG07(1, 0x42, 0x0000, bytes, LNO(__LINE__) "correct call 0 bytes", "01 42");

  // #### TCP, setMessage(serverID, functionCode, errorCode) #08
  // These will have serverID and function code unchecked, as theose may be the error!
  MSG08(0, 0x03, (Error)0x02, LNO(__LINE__) "invalid server id",      "00 83 02");
  MSG08(1, 0x9F, (Error)0x02, LNO(__LINE__) "invalid FC>127",         "01 9F 02");

  MSG08(1, 0x05, (Error)0xE1, LNO(__LINE__) "correct call", "01 85 E1");
  MSG08(1, 0x05, (Error)0x73, LNO(__LINE__) "correct call (unk.err)", "01 85 73");

  // Testing add()
  ModbusMessage adder;

  uint8_t b = 0x11;
  uint16_t r = 0x2233;
  uint32_t w = 0x44556677;
  float f = 1.2345678;
  double d = -9.87654321;

  // add uint8_t, uint16_t and uint32_t
  adder.clear();
  adder.add(b, r, w);
  testOutput(__func__, LNO(__LINE__) "add uint types", makeVector("11 22 33 44 55 66 77"), adder);

  // add float in normalized form
  adder.clear();
  adder.add(b);
  adder.add(f);
  adder.add(b);
  testOutput(__func__, LNO(__LINE__) "add float normalized", makeVector("11 3F 9E 06 51 11"), adder);

  // add float in register- and nibble-swapped form
  adder.clear();
  adder.add(b);
  adder.add(f, SWAP_REGISTERS|SWAP_NIBBLES);
  adder.add(b);
  testOutput(__func__, LNO(__LINE__) "add float swapped", makeVector("11 60 15 F3 E9 11"), adder);

  // add double in normalized form
  adder.clear();
  adder.add(b);
  adder.add(d);
  adder.add(b);
  testOutput(__func__, LNO(__LINE__) "add double normalized", makeVector("11 C0 23 C0 CA 45 88 F6 33 11"), adder);

  // add double in word- and byte-swapped form
  adder.clear();
  adder.add(b);
  adder.add(d, SWAP_WORDS|SWAP_BYTES);
  adder.add(b);
  testOutput(__func__, LNO(__LINE__) "add double swapped", makeVector("11 88 45 33 F6 23 C0 CA C0 11"), adder);

  // Print summary.
  Serial.printf("----->    Generate messages tests: %4d, passed: %4d\n", testsExecuted, testsPassed);


  // ******************************************************************************
  // Tests using the complete turnaround next. TCP is simulated by TCPstub stub!
  //
  // ATTENTION: the request queue limit has been set to >>> 2 <<< entries only for
  //            test reasons. Better have a 
  //              WAIT_FOR_FINISH(<client>)
  //            after longer test cases to not flood it!
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  printPassed = false;

  // Some prerequisites 
  IPAddress testHost = IPAddress(192, 166, 1, 1);
  IPAddress testHost2 = IPAddress(26, 183, 4, 22);
  uint32_t Token = 1;

  // Register onResponse handler
  TestTCP.onResponseHandler(&handleData);

  // Start ModbusClientTCP background task
  TestTCP.begin();

  // Start TCP stub
  // testCasesByTID is the map to find the matching test case in the worker task
  stub.begin(&testCasesByTID);

  // ****************************************************************************************
  // Example test case.
  
  // Set the host/port the stub shall simulate
  stub.setIdentity(testHost, 502);

  // Set the target host/port the request shall be directed to
  TestTCP.setTarget(testHost, 502, 2000, 200);

  // Define new TestCase object. This is holding all data the TCPstub worker and the
  // onData and onError handlers will need to regard the correct test case.

  TestCase *tc = new TestCase { 

    // function and/or source line number and test case name to be printed with the result.
    .name = LNO(__LINE__),
    .testname = "Simple 0x03 request",
    
    // getMessageCount will be used internally to generate the transaction ID, so get a copy
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    
    // The token value _must_ be different for each test case!!!
    .token = Token++,
    
    // A vector of the response the worker shall return
    .response = makeVector("01 03 08 00 00 11 11 22 22 33 33"),
    
    // A vector of the expected data arriving in onData/onError
    .expected = makeVector("01 03 08 00 00 11 11 22 22 33 33"),
    
    // The worker can be made to delay its response by the time given here
    .delayTime = 0,
    
    // flag to order stub to disconnect after responding
    .stopAfterResponding = false,

    // flag to order stub to respond with a wrong TID
    .fakeTransactionID = false
  };

  // Now create an entry in both reference maps (by TID and by token) for the test case
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;

  // Finally execute the test call
  Error e = TestTCP.addRequest(tc->token, 1, 0x03, 1, 4);
  // Did the call immediately return an error?
  if (e != SUCCESS) {
    // Yes, give it to the test result examiner
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  // Delay a bit to get the request queue accepting again (see ATTENTION! above)
  WAIT_FOR_FINISH(TestTCP)

  // Template to copy & paste
  /* ------------- 21 lines below -------------------------------------------------
  stub.setIdentity(testHost, 502);
  TestTCP.setTarget(testHost, 502, 2000, 200);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "EDIT THIS!",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("EDIT THIS!"),
    .expected = makeVector("EDIT THIS!"),
    .delayTime = 0,                             // <=== change, if needed!
    .stopAfterResponding = false,               // <=== change, if needed!
    .fakeTransactionID = false                  // <=== change, if needed!
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  //          vvvvvvv EDIT THIS! vvvvvvvvvvvvvvvvv
  e = TestTCP.addRequest(tc->token, 1, 0x03, 1, 4);
  if (e != SUCCESS) {
    ModbusMessage r;
    r.add(e);
    testOutput(tc->testname, tc->name, tc->expected, r);
    highestTokenProcessed = tc->token;
  }
  delay(1000);
    ------------------------------------------------------------------------------- */

  // ******************************************************************************
  // Write test cases below this line!
  // ******************************************************************************

// Case to test timeout handling. The stub is asked to delay the response by 3 seconds
  stub.setIdentity(testHost, 502);
  TestTCP.setTarget(testHost, 502, 500, 200);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Forced timeout!",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07"),
    .expected = makeVector("01 87 E0"),
    .delayTime = 3000,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  // Wait for secure timeout end
  WAIT_FOR_FINISH(TestTCP)
  delay(5000);
  stub.flush();

  // Send response with wrong transaction ID
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Wrong transaction ID in response",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07"),
    .expected = makeVector("01 87 EB"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = true
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  // Send response with wrong server ID
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Wrong server ID in response",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("2F 03 06 11 22 33 44 55 66"),
    .expected = makeVector("01 83 E4"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x03, 1, 3);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  // Send response with wrong function code
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Wrong FC in response",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 04 06 11 22 33 44 55 66"),
    .expected = makeVector("01 83 E3"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x03, 1, 3);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  // Stub will not respond at all - another timeout constellation
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "No answer from server",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("01 83 E0"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x03, 1, 3);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  // Wait for secure timeout end
  WAIT_FOR_FINISH(TestTCP)
  delay(5000);
  stub.clear();

  // Provoke full request queue by sending 3 requests without delay.
  // The third shall get the REQUEST_QUEUE_FULL error
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Request queue full - pre 1",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07 2B"),
    .expected = makeVector("01 07 2B"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // Second call immediately following
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Request queue full - pre 2",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07 2B"),
    .expected = makeVector("01 07 2B"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // Third and final. This should catch the error
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Request queue full",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07 2B"),
    .expected = makeVector("E8"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)
  delay(5000);
  stub.clear();

  // Simulate Server not responding (host/port different from stub's identity)
  TestTCP.setTarget(testHost2, 502);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Server not responding",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 07 2B"),
    .expected = makeVector("01 87 EA"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  // Server returns undefined error code
  TestTCP.setTarget(testHost, 502);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Unknown error code",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 87 46"),
    .expected = makeVector("01 87 46"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  // Host switch sequence (requires re-connect())
  // testHost2, testHost2, testHost, testHost2
  // testHost2 is simulated to stop-after-response, so a re-connect has to be done each time
  TestTCP.setTarget(testHost2, 502);
  stub.setIdentity(testHost2, 502);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "testHost2 stop after response(1)",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 87 01"),
    .expected = makeVector("01 87 01"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "testHost2 stop after response(2)",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 87 02"),
    .expected = makeVector("01 87 02"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  TestTCP.setTarget(testHost, 502, 2000, 200);
  stub.setIdentity(testHost, 502);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "testHost interlude",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 87 11"),
    .expected = makeVector("01 87 11"),
    .delayTime = 0,
    .stopAfterResponding = false,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestTCP)

  TestTCP.setTarget(testHost2, 502);
  stub.setIdentity(testHost2, 502);
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "testHost2 stop after response(3)",
    .transactionID = static_cast<uint16_t>(TestTCP.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = makeVector("01 87 03"),
    .expected = makeVector("01 87 03"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByTID[tc->transactionID] = tc;
  testCasesByToken[tc->token] = tc;
  e = TestTCP.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // Print summary. We will have to wait a bit to get all test cases executed!
  WAIT_FOR_FINISH(TestTCP)

  Serial.printf("----->    TCP loop stub tests: %4d, passed: %4d\n", testsExecuted, testsPassed);


  // ******************************************************************************
  // Tests using RTU client and server looped together next.
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  printPassed = false;

  // Baud rate to be used for RTU components
  const uint32_t BaudRate(5000000);

  // Set up Serial1 and Serial2
  RTUutils::prepareHardwareSerial(Serial1);
  RTUutils::prepareHardwareSerial(Serial2);
  Serial1.begin(BaudRate, SERIAL_8N1, GPIO_NUM_32, GPIO_NUM_33);
  Serial2.begin(BaudRate, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_16);

  LOG_I("Serial1 at %d baud\n", Serial1.baudRate());
  LOG_I("Serial2 at %d baud\n", Serial2.baudRate());

// CHeck if connections are made
  char chkSerial[64];
  snprintf(chkSerial, 64, "This is Serial1\n");
  uint16_t chkLen = strlen(chkSerial);
  uint16_t chkCnt = 0;
  bool chkFailed = false;

  Serial1.write(reinterpret_cast<uint8_t *>(chkSerial), chkLen);
  Serial1.flush();
  delay(50);

  while (Serial2.available()) {
    if (Serial2.read() == chkSerial[chkCnt]) {
      chkCnt++;
    }
  }
  if (chkCnt != chkLen) {
    Serial.printf("Serial1 failed: %d != %d\n", chkLen, chkCnt);
    chkFailed = true;
  }

  snprintf(chkSerial, 64, "And this is Serial2\n");
  chkLen = strlen(chkSerial);
  chkCnt = 0;

  Serial2.write(reinterpret_cast<uint8_t *>(chkSerial), chkLen);
  Serial2.flush();
  delay(50);

  while (Serial1.available()) {
    if (Serial1.read() == chkSerial[chkCnt]) {
      chkCnt++;
    }
  }
  if (chkCnt != chkLen) {
    Serial.printf("Serial2 failed: %d != %d\n", chkLen, chkCnt);
    chkFailed = true;
  }
  if (chkFailed) {
    Serial.println("--> Please be sure to connect GPIO_16<->GPIO_32 and GPIO_17<->GPIO_33!");
    Serial.println("    RTU loop tests skipped.");
  } else {
    // Connections are as needed. Set up RTU client
    RTUclient.onResponseHandler(&handleData);
    // RTUclient.onDataHandler(&handleData);
    // RTUclient.onErrorHandler(&handleError);
    RTUclient.setTimeout(2000);

    // Start RTU client. 
    RTUclient.begin(Serial1);

    // Define and start RTU server
    RTUserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=1
    RTUserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);     // FC=04 for serverID=1
    RTUserver.registerWorker(1, WRITE_HOLD_REGISTER, &FC06);     // FC=06 for serverID=1
    RTUserver.registerWorker(1, USER_DEFINED_44, &FC44);         // FC=44 for serverID=1
    RTUserver.registerWorker(1, USER_DEFINED_45, &FC45);         // FC=45 for serverID=1
    RTUserver.registerWorker(2, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=2
    RTUserver.registerWorker(2, USER_DEFINED_41, &FC41);         // FC=41 for serverID=2
    RTUserver.registerWorker(2, ANY_FUNCTION_CODE, &FCany);      // FC=any for serverID=2

    // Have the RTU server run on core 1.
    RTUserver.begin(Serial2, 1);

    ExpectedToggles = 0;

    // Set up test memory
    for (uint16_t i = 0; i < 32; ++i) {
      memo[i] = (i * 2) << 8 | ((i * 2) + 1);
    }
    // ******************************************************************************
    // Write test cases below this line!
    // ******************************************************************************

    // #1: read a word of data
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Read one word of data",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 03 02 1E 1F"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 1, 0x03, 16, 1);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #2: write a word of data
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Write one word of data",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 06 00 10 BE EF"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 1, 0x06, 16, 0xBEEF);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #3: read several words
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Read back 4 words of data",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 03 08 1A 1B 1C 1D BE EF 20 21"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 1, 0x03, 14, 4);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #4: use explicit worker
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Explicit worker FC03",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("02 03 08 C9 C8 C7 C6 C5 C4 C3 C2"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 2, READ_HOLD_REGISTER, 28, 4);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // Snap in the Sniffer
    // RTUserver.registerSniffer(Sniffer);

    // #5: use default worker
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Default worker FC07",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("02 07 41 4E 59 20 46 43"),  // "ANY FC"
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 2, 0x07);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #6: invalid FC
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Invalid function code",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 87 01"), 
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 1, 0x07);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #7: invalid server id
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Invalid server ID",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("03 87 E0"), 
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    e = RTUclient.addRequest(tc->token, 3, 0x07);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }
  WAIT_FOR_FINISH(RTUclient)

    // #8: NIL_RESPONSE (aka timeout)
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "NIL_RESPONSE",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("02 C1 E0"), 
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    e = RTUclient.addRequest(tc->token, 2, USER_DEFINED_41);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }
    WAIT_FOR_FINISH(RTUclient)

    // Test-wise, switch handlers
    RTUclient.onResponseHandler(nullptr);
    RTUclient.onDataHandler(handleData);
    RTUclient.onErrorHandler(handleError);

    // #9: Error response
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Error response",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("02"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    e = RTUclient.addRequest(tc->token, 1, 0x03, 45, 1);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #10: Large message
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Large message",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 44 00 7D"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    ModbusMessage large;
    large.add((uint8_t)1, USER_DEFINED_44);
    for (uint16_t i = 1; i < 126; ++i) {
      large.add(i);
    }
    e = RTUclient.addRequest(large, tc->token);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #11: Oversize message
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Oversize message",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 44 00 C7"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    ExpectedToggles++;
    for (uint16_t i = 126; i < 200; ++i) {
      large.add(i);
    }
    e = RTUclient.addRequest(large, tc->token);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // #12: Queue and loop stress
    for (uint8_t i = 0; i < 100; ++i) {
      char tn[32];
      snprintf(tn, 32, "RTU stress loop #%d", i);
      tc = new TestCase { 
        .name = LNO(__LINE__),
        .testname = tn,
        .transactionID = 0,
        .token = Token++,
        .response = empty,
        .expected = makeVector("02 03 08 C9 C8 C7 C6 C5 C4 C3 C2"),
        .delayTime = 0,
        .stopAfterResponding = true,
        .fakeTransactionID = false
      };
      testCasesByToken[tc->token] = tc;
      ExpectedToggles++;
      e = RTUclient.addRequest(tc->token, 2, READ_HOLD_REGISTER, 28, 4);
      if (e != SUCCESS) {
        ModbusMessage ri;
        ri.add(e);
        testOutput(tc->testname, tc->name, tc->expected, ri);
        highestTokenProcessed = tc->token;
      }
      delay(10);
    }

    WAIT_FOR_FINISH(RTUclient)

    // Check RTS toggle
    testsExecuted++;
    // We expect one more LOW callback (initialization)
    if (cntRTShigh != ExpectedToggles || cntRTSlow != ExpectedToggles + 1) {
      Serial.printf("RTS toggle counts do not match. Expected=%d, HIGH=%d, LOW=%d\n", ExpectedToggles, cntRTShigh, cntRTSlow);
    } else {
      testsPassed++;
    }

    // Now to something completely different...
    // Use Modbus ASCII now.
    // First set client to ASCII only to check for error returns
    RTUclient.useModbusASCII();

    Serial.printf("Expect an E2 CRC error from ModbusServerRTU - is intended!\n");

    // #12: read a word of data with no ASCII server present
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Read one word of data (ASCII)",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("E0"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    e = RTUclient.addRequest(tc->token, 1, 0x03, 16, 1);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    WAIT_FOR_FINISH(RTUclient)

    // Now switch server to ASCII as well
    RTUserver.useModbusASCII();

    // #13: read a word of data 
    tc = new TestCase { 
      .name = LNO(__LINE__),
      .testname = "Read one word of data (ASCII)",
      .transactionID = 0,
      .token = Token++,
      .response = empty,
      .expected = makeVector("01 03 02 BE EF"),
      .delayTime = 0,
      .stopAfterResponding = true,
      .fakeTransactionID = false
    };
    testCasesByToken[tc->token] = tc;
    e = RTUclient.addRequest(tc->token, 1, 0x03, 16, 1);
    if (e != SUCCESS) {
      ModbusMessage ri;
      ri.add(e);
      testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
    }

    // Switch back to RTU mode for the rest of tests
    RTUclient.useModbusRTU();
    RTUserver.useModbusRTU();
    // We will have to wait a bit to get all test cases executed!
    WAIT_FOR_FINISH(RTUclient)

    // Test Broadcasts
    // Set up some BC data
    uint8_t bcdata[] = "Broadcast data #1";
    uint8_t bclen = 18;

    // Send message
    e = RTUclient.addBroadcastMessage(bcdata, bclen);
    if (e != SUCCESS) {
      ModbusError me(e);
      LOG_N("%s failed: %d - %s\n", reinterpret_cast<const char *>(bcdata), static_cast<int>(me), static_cast<const char *>(me));
    }
    // Wait for the server worker task to pass timeout
    delay(5000);
    testsExecuted++;
    // We have no worker registered yet, so the message shall be discarded
    if (broadcastCnt == 0) {
      testsPassed++;
    } else {
      LOG_N("Broadcast was caught???\n");
    }

    // Kick off the Sniffer
    // RTUserver.registerSniffer(nullptr);

    // Modify BC data
    bcdata[16] = '2';

    // Now register a worker for Broadcasts
    RTUserver.registerBroadcastWorker(BroadcastWorker);

    // Send BC again
    e = RTUclient.addBroadcastMessage(bcdata, bclen);
    if (e != SUCCESS) {
      ModbusError me(e);
      LOG_N("%s failed: %d - %s\n", reinterpret_cast<const char *>(bcdata), static_cast<int>(me), static_cast<const char *>(me));
    }
    delay(5000);
    testsExecuted++;
    // The BC must have been caught
    if (broadcastCnt == 1) {
      testsPassed++;
    } else {
      LOG_N("Broadcast not caught\n");
    }

    // Check worker function matching patterns
    RTUserver.registerWorker(ANY_SERVER, READ_HOLD_REGISTER, &SVany); // FC=03 for any server ID
    RTUserver.registerWorker(ANY_SERVER, ANY_FUNCTION_CODE, &SVFCany); // FC=any for any server ID

    // We have an explicit worker for 01/03: FC03 must be used
    testsExecuted++;
    auto wrk = RTUserver.getWorker(1, READ_HOLD_REGISTER).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk && *wrk == FC03) {
      testsPassed++;
    } else {
      LOG_N("worker(01/03) != FC03\n");
    }

    // same for 02/03: FC03 must be used
    testsExecuted++;
    wrk = RTUserver.getWorker(2, READ_HOLD_REGISTER).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk && *wrk == FC03) {
      testsPassed++;
    } else {
      LOG_N("worker(02/03) != FC03\n");
    }

    // 08/03 has never been defined, but we have SVany as a generic 03 worker
    testsExecuted++;
    wrk = RTUserver.getWorker(8, READ_HOLD_REGISTER).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk && *wrk == SVany) {
      testsPassed++;
    } else {
      LOG_N("worker(08/03) != SVany\n");
    }

    // 02/66 shall be processed by FCany
    testsExecuted++;
    wrk = RTUserver.getWorker(2, USER_DEFINED_66).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk && *wrk == FCany) {
      testsPassed++;
    } else {
      LOG_N("worker(02/66) != FCany\n");
    }

    // Finally 54/16 is to be caught by SVFCany, the "catch-all" worker
    testsExecuted++;
    wrk = RTUserver.getWorker(54, WRITE_MULT_REGISTERS).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk && *wrk == SVFCany) {
      testsPassed++;
    } else {
      LOG_N("worker(54/16) != SVFCany\n");
    }

    // Unregister ANY/ANY worker again
    RTUserver.unregisterWorker(ANY_SERVER, ANY_FUNCTION_CODE);

    // Now 54/16 has no worker any more and shall return a nullptr
    testsExecuted++;
    wrk = RTUserver.getWorker(54, WRITE_MULT_REGISTERS).target<ModbusMessage(*)(ModbusMessage)>();
    if (wrk) {
      LOG_N("worker(54/16) != nullptr\n");
    } else {
      testsPassed++;
    }

    // Check unregistering workers
    bool didit = RTUserver.unregisterWorker(1, USER_DEFINED_48);
    testsExecuted++;
    if (!didit && !RTUserver.getWorker(1, USER_DEFINED_48)) {
      testsPassed++;
    } else {
      LOG_N("unregisterWorker 01/48 failed (didit=%d)\n", didit ? 1 : 0);
    }

    didit = RTUserver.unregisterWorker(1, USER_DEFINED_44);
    testsExecuted++;
    if (didit && !RTUserver.getWorker(1, USER_DEFINED_44)) {
      testsPassed++;
    } else {
      LOG_N("unregisterWorker 01/44 failed (didit=%d)\n", didit ? 1 : 0);
    }

    didit = RTUserver.unregisterWorker(4);
    testsExecuted++;
    if (!didit) {
      testsPassed++;
    } else {
      LOG_N("unregisterWorker 04 failed (didit=%d)\n", didit ? 1 : 0);
    }

    didit = RTUserver.unregisterWorker(2);
    testsExecuted++;
    if (didit && !RTUserver.getWorker(2, READ_HOLD_REGISTER)) {
      testsPassed++;
    } else {
      LOG_N("unregisterWorker 02 failed (didit=%d)\n", didit ? 1 : 0);
    }
    WAIT_FOR_FINISH(RTUclient)

    // Try larger packets with increasing baud rates
    uint32_t myBaud = 1200;
    // Prepare request long enough to exceed UART FIFO buffer
    ModbusMessage myReq;
    myReq.add((uint8_t)1, USER_DEFINED_45);
    for (uint16_t cntr = 0; cntr < 160; cntr++) {
      myReq.add((uint8_t)('A' + cntr % 26));
    }
    // Loop while doubling the baud rate each turn
    MBUlogLvl = LOG_LEVEL_VERBOSE;
    while (myBaud < 5000000) {
      delay(1000);
      Serial1.updateBaudRate(myBaud);
      Serial2.updateBaudRate(myBaud);
      uint32_t myInterval = 3500;
      RTUclient.begin(Serial1, -1, myInterval);
      RTUserver.begin(Serial2, -1, myInterval);
      LOG_I("testing %d baud.\n", myBaud);
      testsExecuted++;
      ModbusMessage ret = RTUclient.syncRequest(myReq, Token++);
      Error ei = ret.getError();
      // If not successful, report it
      if (ei != SUCCESS) {
        ModbusError me(ei);
        LOG_N("Baud test failed at %u (%02X - %s)\n", myBaud, ei, static_cast<const char *>(me));
      } else {
        // No error, but is the responded value correct?
        uint16_t mySize = 0;
        ret.get(2, mySize);
        if (mySize != 162) {
          // No, report it.
          LOG_N("Baud test failed at %u (size %u != 162)\n", myBaud, mySize);
        } else {
          testsPassed++;
        }
      }
      myBaud *= 2;
    }
    MBUlogLvl = LOG_LEVEL_ERROR;

    // Print summary. We will have to wait a bit to get all test cases executed!
    WAIT_FOR_FINISH(RTUclient)

    Serial.printf("----->    RTU tests: %4d, passed: %4d\n", testsExecuted, testsPassed);
  
  }


  // ******************************************************************************
  // Tests using WiFi client and server looped together next.
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  printPassed = false;

  // Register onData and onError handlers
  TestClientWiFi.onDataHandler(&handleData);
  TestClientWiFi.onErrorHandler(&handleError);
  
  // Activate connection cutting after timeouts
  TestClientWiFi.closeConnectionOnTimeouts();

  // Start ModbusClientTCP background task
  TestClientWiFi.begin();
  TestClientWiFi.setTarget(ip, port, 500, 0);

  // Define and start TCP server
  MBserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=1
  MBserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);     // FC=04 for serverID=1
  MBserver.registerWorker(1, WRITE_HOLD_REGISTER, &FC06);     // FC=06 for serverID=1
  MBserver.registerWorker(2, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=2
  MBserver.registerWorker(2, USER_DEFINED_41, &FC41);         // FC=41 for serverID=2
  MBserver.registerWorker(2, ANY_FUNCTION_CODE, &FCany);      // FC=41 for serverID=2

  // Set up test memory
  for (uint16_t i = 0; i < 32; ++i) {
    memo[i] = (i * 2) << 8 | ((i * 2) + 1);
  }

  MBserver.start(port, 2, 10000);

  // ******************************************************************************
  // Write test cases below this line!
  // ******************************************************************************

  // #1: read a word of data
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Read one word of data",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("01 03 02 1E 1F"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 1, 0x03, 16, 1);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #2: write a word of data
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Write one word of data",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("01 06 00 10 BE EF"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 1, 0x06, 16, 0xBEEF);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #3: read several words
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Read back 4 words of data",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("01 03 08 1A 1B 1C 1D BE EF 20 21"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 1, 0x03, 14, 4);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #4: use explicit worker
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Explicit worker FC03",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("02 03 08 C9 C8 C7 C6 C5 C4 C3 C2"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 2, READ_HOLD_REGISTER, 28, 4);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #5: use default worker
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Default worker FC07",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("02 07 41 4E 59 20 46 43"),  // "ANY FC"
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 2, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #6: invalid FC
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Invalid function code",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("01"), 
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 1, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #7: invalid server id
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Invalid server ID",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("E1"), 
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 3, 0x07);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // #8: NIL_RESPONSE (aka timeout)
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "NIL_RESPONSE",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("E0"), 
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 2, USER_DEFINED_41);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }
  WAIT_FOR_FINISH(TestClientWiFi)

  // #9: Error response
  tc = new TestCase { 
    .name = LNO(__LINE__),
    .testname = "Error response",
    .transactionID = static_cast<uint16_t>(TestClientWiFi.getMessageCount() & 0xFFFF),
    .token = Token++,
    .response = empty,
    .expected = makeVector("02"),
    .delayTime = 0,
    .stopAfterResponding = true,
    .fakeTransactionID = false
  };
  testCasesByToken[tc->token] = tc;
  e = TestClientWiFi.addRequest(tc->token, 1, 0x03, 45, 1);
  if (e != SUCCESS) {
    ModbusMessage ri;
    ri.add(e);
    testOutput(tc->testname, tc->name, tc->expected, ri);
    highestTokenProcessed = tc->token;
  }

  // Print summary. We will have to wait a bit to get all test cases executed!
  WAIT_FOR_FINISH(RTUclient)
  Serial.printf("----->    TCP WiFi loopback tests: %4d, passed: %4d\n", testsExecuted, testsPassed);

  // ******************************************************************************
  // Tests for synchronous requests
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  printPassed = false;
  ModbusMessage n, m;

  if (chkFailed) {
    Serial.printf("Synchronous RTU tests skipped\n");
  } else {
    m.setMessage(1, READ_HOLD_REGISTER, 1, 4);
    n = RTUclient.syncRequest(m, Token++);
    testOutput("Regular sync response (RTU)", LNO(__LINE__), makeVector("01 03 08 00 01 02 03 04 05 06 07"), n);

    n = RTUclient.syncRequest(Token++, 1, 251);
    testOutput("Sync FC error (RTU)", LNO(__LINE__), makeVector("01 FB 01"), n);

    n = RTUclient.syncRequest(Token++, 8, READ_HOLD_REGISTER, 8, 4);
    testOutput("Sync request wrong serverID (RTU)", LNO(__LINE__), makeVector("08 83 E0"), n);
  }

  n = TestClientWiFi.syncRequest(Token, 1, READ_HOLD_REGISTER, 4, 4);
  testOutput("Sync regular request (WiFi)", LNO(__LINE__), makeVector("01 03 08 06 07 08 09 0A 0B 0C 0D"), n);

  // Same Token!
  n = TestClientWiFi.syncRequest(Token++, 1, READ_HOLD_REGISTER, 8, 4);
  testOutput("Sync request same token (WiFi)", LNO(__LINE__), makeVector("01 03 08 0E 0F 10 11 12 13 14 15"), n);

  n = TestClientWiFi.syncRequest(Token++, 8, READ_HOLD_REGISTER, 8, 4);
  testOutput("Sync request wrong serverID (WiFi)", LNO(__LINE__), makeVector("08 83 E1"), n);

  n = TestClientWiFi.syncRequest(Token++, 2, READ_HOLD_REGISTER, 32, 160);
  testOutput("Sync request address/words invalid (WiFi)", LNO(__LINE__), makeVector("02 83 E7"), n);

  // Print summary.
  Serial.printf("----->    Synchronous request tests: %4d, passed: %4d\n", testsExecuted, testsPassed);

  // ******************************************************************************
  // Bridge tests
  // ******************************************************************************

  // Restart test case and tests passed counter
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;

  printPassed = false;

  // Attaching the servers will include an "unfriendly takeover" of the onError and onData handlers,
  // so prevent the warning to be printed
  MBUlogLvl = LOG_LEVEL_ERROR;
  Bridge.attachServer(3, 1, ANY_FUNCTION_CODE, &TestClientWiFi, ip, port);
  Bridge.attachServer(4, 2, ANY_FUNCTION_CODE, &TestClientWiFi, ip, port);
  Bridge.denyFunctionCode(4, READ_INPUT_REGISTER);
  // Re-enable warnings
  MBUlogLvl = LOG_LEVEL_WARNING;

  m.setMessage(3, READ_HOLD_REGISTER, 3, 2);
  n = Bridge.localRequest(m);
  testOutput("Regular request", LNO(__LINE__), makeVector("03 03 04 04 05 06 07"), n);

  m.setMessage(5, READ_HOLD_REGISTER, 3, 2);
  n = Bridge.localRequest(m);
  testOutput("Invalid server ID", LNO(__LINE__), makeVector("05 83 E1"), n);

  m.setMessage(3, MASK_WRITE_REGISTER, 3, 2, 1);
  n = Bridge.localRequest(m);
  testOutput("FC not supported by server", LNO(__LINE__), makeVector("03 96 01"), n);

  m.setMessage(4, READ_INPUT_REGISTER, 3, 2);
  n = Bridge.localRequest(m);
  testOutput("FC not supported by bridge", LNO(__LINE__), makeVector("04 84 01"), n);

  if (chkFailed) {
    Serial.printf("Bridge RTU tests skipped\n");
  } else {
    MBUlogLvl = LOG_LEVEL_ERROR;
    Bridge.attachServer(2, 1, READ_HOLD_REGISTER, &RTUclient);
    Bridge.attachServer(6, 9, ANY_FUNCTION_CODE, &RTUclient);
    Bridge.addFunctionCode(2, READ_HOLD_REGISTER);
    MBUlogLvl = LOG_LEVEL_WARNING;

    m.setMessage(2, READ_HOLD_REGISTER, 3, 2);
    n = Bridge.localRequest(m);
    testOutput("Regular request (RTU)", LNO(__LINE__), makeVector("02 03 04 04 05 06 07"), n);

    m.setMessage(6, READ_HOLD_REGISTER, 3, 2);
    n = Bridge.localRequest(m);
    testOutput("Invalid server (RTU)", LNO(__LINE__), makeVector("06 83 E0"), n);
  }

  // Print summary.
  Serial.printf("----->    Bridge tests: %4d, passed: %4d\n", testsExecuted, testsPassed);

// ******************************************************************************
// CoilData type tests
// ******************************************************************************

  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;
  MBUlogLvl = LOG_LEVEL_WARNING;

// Prepare test coil data with all coils set to 1
  CoilData coils(37, true);

// Switch off some coils
  coils.set(2, false);
  coils.set(11, false);
  coils.set(13, false);
  coils.set(17, false);
  coils.set(22, false);
  coils.set(26, false);
  coils.set(27, false);
  coils.set(19, false);
  coils.set(32, false);
  coils.set(35, false);

// Take a slice out of the middle
// Note: we have an intermediate vector<uint8_t> here, as the 2-step conversion from CoilData to ModbusMessage is ambiguous!
  vector<uint8_t> coilset;
  coilset = coils.slice(6, 22);
  testOutput("Plain vanilla slice", LNO(__LINE__), makeVector("5F D7 0E"), (ModbusMessage)coilset);

// Take a slice from the lowest coil on
  coilset = coils.slice(0, 4);
  testOutput("Leftmost slice", LNO(__LINE__), makeVector("0B"), (ModbusMessage)coilset);

// Take a 1-coil slice
  coilset = coils.slice(1, 1);
  testOutput("Single coil slice", LNO(__LINE__), makeVector("01"), (ModbusMessage)coilset);

// Take a slice up to the highest coil
  coilset = coils.slice(30, 7);
  testOutput("Rightmost slice", LNO(__LINE__), makeVector("5B"), (ModbusMessage)coilset);

// Attempt to take a slice off defined coils
  coilset = coils.slice(1, 45);
  testOutput("Invalid slice", LNO(__LINE__), makeVector(""), (ModbusMessage)coilset);

// Take a complete slice, making use of the defaults
  coilset = coils.slice();
  testOutput("Complete slice", LNO(__LINE__), makeVector("FB D7 B5 F3 16"), (ModbusMessage)coilset);

// Create a new coil set by copy constructor
  vector<uint8_t> coilset2;
  CoilData coils2(coils);
  coilset = coils;
  coilset2 = coils2;
  testOutput("Copy constructor", LNO(__LINE__), (ModbusMessage)coilset, (ModbusMessage)coilset2);

// Create a third set with smaller size
  CoilData coils3(16);

// Set a single coil
  coils3.set(12, true);
  coilset = coils3;
  testOutput("set single coil", LNO(__LINE__), makeVector("00 10"), (ModbusMessage)coilset);

// Re-init all coils to 1
  coils3.init(true);
  coilset = coils3;
  testOutput("Init coils", LNO(__LINE__), makeVector("FF FF"), (ModbusMessage)coilset);

// Define a slice for writing coils. 9 coils to be written!
  vector<uint8_t> cd = { 0xAA, 0x00 };

// Re-init coils again
  coils3.init(true);

// Do a slice set from leftmost coil on
  coils3.set(0, 9, cd);
  coilset = coils3;
  testOutput("Set from 0", LNO(__LINE__), makeVector("AA FE"), (ModbusMessage)coilset);

// Init and do another in the middle
  coils3.init(true);
  coils3.set(4, 9, cd);
  coilset = coils3;
  testOutput("Set from 4", LNO(__LINE__), makeVector("AF EA"), (ModbusMessage)coilset);

// Init and do a third set up to the end of coils
  coils3.init(true);
  coils3.set(7, 9, cd);
  coilset = coils3;
  testOutput("Set from 7", LNO(__LINE__), makeVector("7F 55"), (ModbusMessage)coilset);

// Attempt to set invalid coil addresses
  coils3.init(true);
  coils3.set(10, 9, cd);
  coilset = coils3;
  testOutput("Invalid set", LNO(__LINE__), makeVector("FF FF"), (ModbusMessage)coilset);

// Assign a larger set to the smaller. 
  coils3 = coils2;
  coilset2 = coils2;
  coilset = coils3;
  testOutput("Assignment", LNO(__LINE__), (ModbusMessage)coilset2, (ModbusMessage)coilset);

// Coils #4 with bit image array constructor
  CoilData coils4("1111 4 zeroes 0000 Escaped_1 4 Ones 1111 _0010101");
  coilset = coils4;
  testOutput("Image constructor", LNO(__LINE__), makeVector("0F AF 02"), (ModbusMessage)coilset);

// Assignment of bit image array
  coils4 = "111 000 1010 0101 001";
  coilset = coils4;
  testOutput("Image assignment", LNO(__LINE__), makeVector("47 29 01"), (ModbusMessage)coilset);

// Change partly with a bit image array
  coils4 = "111 000 1010 0101 001";
  coils4.set(8, "111000111");
  coilset = coils4;
  testOutput("Image set (fitting)", LNO(__LINE__), makeVector("47 C7 01"), (ModbusMessage)coilset);

// Change partly with a bit image array too long to fit
  coils4 = "111 000 1010 0101 001";
  coils4.set(8, "1111111111111111");
  coilset = coils4;
  testOutput("Image set (not fitting)", LNO(__LINE__), makeVector("47 FF 01"), (ModbusMessage)coilset);

// Changing coils by coils ;)
  coils4 = "000000";
  coils2.init(true);
  coils2.set(5, coils4.coils(), coils4);
  coilset = coils2;
  testOutput("Set with another coils set", LNO(__LINE__), makeVector("1F F8 FF FF 1F"), (ModbusMessage)coilset);

// Create a ModbusMessage with a slice in
  coils4 = "11100010100101001";
  ModbusMessage cm;

  // #1: manually set all parameters
  cm.add((uint8_t)1, WRITE_MULT_COILS, (uint16_t)0, coils4.coils(), coils4.size());
  cm.add(coils4.data(), coils4.size());
  testOutput("ModbusMessage with coils #1", LNO(__LINE__), makeVector("01 0F 00 00 00 11 03 47 29 01"), cm);

  // #2: use setMessage()
  cm.setMessage(1, WRITE_MULT_COILS, 0, coils4.coils(), coils4.size(), coils4.data());
  testOutput("ModbusMessage with coils #2", LNO(__LINE__), makeVector("01 0F 00 00 00 11 03 47 29 01"), cm);
  
  // Read back coil set from message
  vector<uint8_t> gc;
  cm.get(7, gc, coils4.size());
  coilset = coils4;
  testOutput("Read coils from message", LNO(__LINE__), (ModbusMessage)coilset, (ModbusMessage)gc);
  
  // Use read set to modify coil set
  coils.set(0, 17, gc);
  coilset = coils;
  testOutput("Set coil set from message data", LNO(__LINE__), makeVector("47 29 B5 F3 16"), (ModbusMessage)coilset);

  // Some comparison tests
  testsExecuted++;
  uint8_t okay = 0;
  coils4 = "1101010111";
  if (coils4 == "1101010111 plus some garbage trailing") {
    okay++;
  } else {
    Serial.print(LNO(__LINE__) "Compare #1 failed\n");
  }
  if (coils4 != "110101 1 0111") {
    okay++;
  } else {
    Serial.print(LNO(__LINE_) "Compare #2 failed\n");
  }
  if (coils4 == "1101010111_1") {
    okay++;
  } else {
    Serial.print(LNO(__LINE__) "Compare #3 failed\n");
  }
  if (coils4 != "11010101111") {
    okay++;
  } else {
    Serial.print(LNO(__LINE__) "Compare #4 failed\n");
  }
  if (coils4.slice(0, 3) == "110") {
    okay++;
  } else {
    Serial.print(LNO(__LINE__) "Compare #5 failed\n");
  }
  if (okay == 5) testsPassed++;

  // Print summary.
  Serial.printf("----->    CoilData tests: %4d, passed: %4d\n", testsExecuted, testsPassed);

  // ******************************************************************************
  // FC redefinition tests
  // ******************************************************************************
  testsExecutedGlobal += testsExecuted;
  testsPassedGlobal += testsPassed;
  testsExecuted = 0;
  testsPassed = 0;
  FCType ft = FCILLEGAL;

  // #1 - try redefining an existing function code
  testsExecuted++;
  ft = FCT::redefineType(READ_HOLD_REGISTER, FCGENERIC);
  if (ft == FC01_TYPE) {
    testsPassed++;
  } else {
    Serial.print(LNO(__LINE__) "FC redefinition #1 failed\n");
  }

  // #2 - try redefining an undefined function code
  testsExecuted++;
  ft = FCT::redefineType(0x55, FC01_TYPE);
  if (ft == FC01_TYPE) {
    testsPassed++;
  } else {
    Serial.print(LNO(__LINE__) "FC redefinition #2 failed\n");
  }

  // #3 - use redefined function code
  MSG03(1, 0x55, 0x1020, 125,    LNO(__LINE__) "correct call 0x55 (125)",  "01 55 10 20 00 7D");

  // #4 - use redefined function code with wrong call
  MSG04(1, 0x55, 0x1020, 125, 4711,    LNO(__LINE__) "wrong call parameter 0x55 (4711)",  "01 D5 E6");

  // Print summary.
  Serial.printf("----->    FC redefiniton: %4d, passed: %4d\n", testsExecuted, testsPassed);

  // ******************************************************************************
  // Counter tests
  // ******************************************************************************
  if (RTUserver.getMessageCount() != 125 || RTUserver.getErrorCount() != 2) {
    LOG_N("RTUserver reporting unexpected count: %d/%d instead of 125/2\n", RTUserver.getMessageCount(), RTUserver.getErrorCount());
  }
  if (RTUclient.getMessageCount() != 132 || RTUclient.getErrorCount() != 7) {
    LOG_N("RTUclient reporting unexpected count: %d/%d instead of 132/7\n", RTUclient.getMessageCount(), RTUclient.getErrorCount());
  }
  if (MBserver.getMessageCount() != 14 || MBserver.getErrorCount() != 5) {
    LOG_N("MBserver reporting unexpected count: %d/%d instead of 14/5\n", MBserver.getMessageCount(), MBserver.getErrorCount());
  }
  if (Bridge.getMessageCount() != 6 || Bridge.getErrorCount() != 4) {
    LOG_N("Bridge reporting unexpected count: %d/%d instead of 6/4\n", Bridge.getMessageCount(), Bridge.getErrorCount());
  }

/*
  // ******************************************************************************
  // Logging tests
  // ******************************************************************************

  Serial.printf("\n\n\n\nSome logging test output - please check yourself!\n");

  MBUlogLvl = LOG_LEVEL_VERBOSE;
  LOG_N("If you see this, Logging is working on a user-defined LOGDEVICE.\n");
  LOG_N("Following shall be a Test dump, then 6 pairs of output for different log levels.\n\n");
  HEXDUMP_N("Test data", (uint8_t *)&words, 10);

  Serial.println();
  LOG_C("\nCritical log message\n");
  HEXDUMP_C("Critical dump", (uint8_t *)&words, 10);

  Serial.println();
  LOG_E("\nError log message\n");
  HEXDUMP_E("Error dump", (uint8_t *)&words, 10);

  Serial.println();
  LOG_W("\nWarning log message\n");
  HEXDUMP_W("Warning dump", (uint8_t *)&words, 10);

  Serial.println();
  LOG_I("\nInformational log message\n");
  HEXDUMP_I("Info dump", (uint8_t *)&words, 10);

  Serial.println();
  LOG_D("\nDebug log message\n");
  HEXDUMP_D("Debug dump", (uint8_t *)&words, 10);

  Serial.println();
  LOG_V("\nVerbose log message\n");
  HEXDUMP_V("Verbose dump data", (uint8_t *)&words, 10);
  */

  // ======================================================================================
  // Print global summary.
  Serial.printf("\n\n *** Tests run: %5d, passed: %5d\n", testsExecutedGlobal, testsPassedGlobal);
  // Final message
  Serial.println("\n\n *** ----> All finished.");

}

void loop() {
  // Nothing done here!
  delay(1000);
}
