#include "Logging.h"
#include "ModbusClientTCP.h"
#include "parseTarget.h"

// ============= main =============
int main(int argc, char **argv) {
  // Define a TCP client
  Client cl;

  // Define a Modbus client using the TCP client
  ModbusClientTCP MBclient(cl);

  // Set default target 
  IPAddress targetIP = NIL_ADDR;
  uint16_t targetPort = 502;
  uint8_t targetSID = 1;
  uint16_t addr = 1;
  uint16_t words = 8;

  if (argc != 4) {
    printf("Usage: %s target address numRegisters\n", argv[0]);
    return -1;
  }

  if (int rc = parseTarget(argv[1], targetIP, targetPort, targetSID)) {
    printf("Invalid target descriptor. Must be IP[:port[:serverID]] or hostname[:port[:serverID]]\n");
    return -1;
  }

  addr = atoi(argv[2]) & 0xFFFF;
  words = atoi(argv[3]) & 0xFFFF;

  printf("Using %s:%u:%u @%u/%u\n", string(targetIP).c_str(), targetPort, targetSID, addr, words);

  // Disable Nagle algorithm
  cl.setNoDelay(true);

  // Set up ModbusTCP client.
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
  // - start address to read = addr
  // - number of words to read = words

  ModbusMessage response = MBclient.syncRequest((uint32_t)millis(), targetSID, READ_HOLD_REGISTER, addr, words);
  Error err = response.getError();
  if (err != SUCCESS) {
    ModbusError e(err);
    printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  } else {
    HEXDUMP_N("Response", response.data(), response.size());
  }

  return 0;
}
