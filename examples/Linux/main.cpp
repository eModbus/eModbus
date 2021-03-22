#include "Logging.h"
#include "ModbusClientTCP.h"

// Define a TCP client
Client cl;

// Define a Modbus client using the TCP client
ModbusClientTCP MBclient(cl);

// Flag to hold the main thread until the client has answered
bool gotIt = false;

// Define an onData handler function to receive the regular responses
// Arguments are Modbus server ID, the function code requested, the message data and length of it, 
// plus a user-supplied token to identify the causing request
void handleData(ModbusMessage response, uint32_t token) 
{
  printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
  HEXDUMP_N("Data", response.data(), response.size());
  gotIt = true;
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  printf("Error response: %02X - %s\n", (int)me, (const char *)me);
  gotIt = true;
}

// ============= main =============
int main(int argc, char **argv) {
// Set default target 
  IPAddress targetIP("192.168.178.25");
  uint16_t targetPort = 502;
  uint8_t targetSID = 1;
  uint16_t addr = 1;
  uint16_t words = 8;

// Do we have caller's parameters?
  if (argc > 1) {
    // Yes. parameter #1 shall be a host IP
    targetIP = argv[1];
    // even more? #2 is the port number
    if (argc > 2) {
      // Yes. Get it
      targetPort = atoi(argv[2]) & 0xFFFF;
      // Next, if existing, is the serverID
      if (argc > 3) {
        // Get it.
        targetSID = atoi(argv[3]) & 0xFF;
        // Next is the register address
        if (argc > 4) {
          // Get it.
          addr = atoi(argv[4]) & 0xFFFF;
          // Final one is the number of registers
          if (argc > 5) {
            // Get it.
            words = atoi(argv[5]) & 0xFFFF;
          }
        }
      }
    }
  }

// Disable Nagle algorithm
  cl.setNoDelay(true);

// Set up ModbusTCP client.
// - provide onData handler function
  MBclient.onDataHandler(&handleData);
// - provide onError handler function
  MBclient.onErrorHandler(&handleError);
// Set message timeout to 2000ms and interval between requests to the same host to 200ms
  MBclient.setTimeout(2000, 200);
// Start ModbusTCP background task
  MBclient.begin();

// Issue a request
// Set Modbus TCP server address and port number
// (Fill in your data here!)
  MBclient.setTarget(targetIP, targetPort);

// Create request for
// (Fill in your data here!)
// - token to match the response with the request. We take the current millis() value for it.
// - server ID = 1
// - function code = 0x03 (read holding register)
// - start address to read = word 1
// - number of words to read = 8
//
// If something is missing or wrong with the call parameters, we will immediately get an error code 
// and the request will not be issued
  gotIt = false; // force sync
  Error err = MBclient.addRequest((uint32_t)millis(), targetSID, READ_HOLD_REGISTER, addr, words);
  if (err!=SUCCESS) {
    ModbusError e(err);
    printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
    gotIt = true; // sync
  }

// Loop to wait for the result
  while (!gotIt) {
    printf(".");
    delay(500);
  }
  printf("\n");

  return 0;
}
