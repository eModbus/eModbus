# ModbusClient
This is a library to provide Modbus client (formely known as master) functionalities for both Modbus RTU and TCP protocols.

Modbus communication is done in separate tasks, so Modbus requests are non-blocking. Callbacks are provided to receive the responses asynchronously.

## Requirements
The library was developed for and on ESP32 MCUs in the Arduino core development environment. In principle it should run in any environment providing these resources:
- Arduino core functions - Serial interfaces, ``millis()``, ``delay()`` etc.
- FreeRTOS' ``xTask`` task functions. Note that there are numerous MCUs running FreeRTOS!
- C++ standard library components
  - ``std::queue``
  - ``std::vector``
  - ``std::mutex``
  - ``std::lock_guard``

For Modbus RTU you will need a RS485-to-Serial adaptor. Commonly used are the **RS485MAX** adaptors or the **XY-017** adaptors with automatic half duplex control.

Modbus TCP will require either an Ethernet module like the WizNet W5xxx series connected to the SPI interface or an (internal or external) WiFi adaptor. The library is by the way using the functions defined in ``Client.h`` only.

## Installation
We recommend to use the [PlatformIO IDE](https://platformio.org/). There, in your project's ``platformio.ini`` file, add the Github URL of this library to your ``lib_deps =`` entry in the ``[env:...]`` section of that file:
```
[platformio]

# some settings

[env:your_project_target]

# some settings

lib_deps = 
  ModbusClient=https://github.com/...
```

If you are using the Arduino IDE, you will want to copy the library folder into your library directory. In Windows this will be  ``C:\Users\<user_name>\Documents\Arduino\libraries`` normally.
## Basic use
To get up and running you will need to put only a few lines in your code. *We will be using the RTU variant here* to show an example.
First you have to pull the matching header file:
```
#include "ModbusRTU.h"
```
For ModbusRTU a Serial interface is required, that connects to your RS485 adaptor. This is given as parameter to the ``ModbusRTU`` constructor:
```
ModbusRTU RS485(Serial2);
```
Next we will define a callback function for data responses coming in. This will print out a hexadecimal dump line with the response data:
```
void handleData(uint8_t serverAddress, uint8_t fc, const uint8_t* data, uint16_t length, uint32_t token) 
{
  Serial.printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", (unsigned int)serverAddress, (unsigned int)fc, token, length);
  const uint8_t *cp = data;
  uint16_t      i = length;
  while (i--) {
    Serial.printf("%02X ", *cp++);
  }
  Serial.println("");
}
```
Optionally we can define another callback to be called when an error response was received. We will use a convenient addition provided by the library here - ``ModbusError``:
```
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  Serial.printf("Error response: %02X - %s\n", (int)me, (const char *)me);
}
```

Now we will wire things together in the ``setup()`` function:
```
void setup() {
// Set up Serial2 connected to Modbus RTU
  Serial2.begin(19200, SERIAL_8N1);

// Set up ModbusRTU client.
// - provide onData and onError handler functions
  RS485.onDataHandler(&handleData);
  RS485.onErrorHandler(&handleError);

// Start ModbusRTU background task
  RS485.begin();
}
```
The only thing missing are the requests now. In ``loop()`` you will trigger these like seen below - here a "read holding register" request is sent to server #1, requesting one data word at address 10. Please disregard the ``token`` value of 0x12345678 for now, it will be explained further below:
```
Error err = RS485.addRequest(1, READ_HOLD_REGISTER, 10, 1, 0x12345678);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }
```
If anything was wrong with your parameters the request will not be created. You will be given an error return value to find out what went wrong.
In case everything was okay, the request is created and put into the background queue to be processed. As soon as that has happened, your ``onData`` or ``onError`` callbacks will be called with the response.
## API description
Following we will first describe the API elements independent from the protocol adaptor used and after that the specifics of both RTU and TCP interfaces.
### Common concepts
Internally the Modbus message contents for requests and responses are held separately from the elements the protocols will require. Hence several API elements are common to both protocols.

#### ``void onDataHandler(MBOnData handler)``
This is the interface to register a callback function to be called when a responce was received. The only parameter is a pointer to a function with this signature:
```
void func(uint8_t serverID, uint8_t functionCode, const uint8_t *data, uint16_t data_length, uint32_t token);
```
Being called, it will get the parameters
- ``serverID``: the Modbus server number (range 1..247)
- ``functionCode``: the function from the request that led to this response
- ``*data`` and ``data_length``: data points to a buffer containing the response message bytes, ``data_length`` gives the number of bytes in the message.
- ``token``: this is a general concept throughout the library, so a detour is necessary.
#### The ``token`` concept
Each request can (and should) be given a user-defined ``token`` value. This is taken with the request and delivered again with the response the request has caused. No processing whatsoever is done on the token, you will get what you sent.
This enables a user to keep track of the sent requests when receiving a response.

Imagine an application that has several ModbusClients working; some for dedicated TCP Modbus servers, another for a RS485 RTU Modbus and another again to request different TCP servers.

If you only want to have a single onData and onError function handling all responses regardless of the server that sent them, you will need a means to tell one response from the other. 

Your token you gave at request time will tell you that, as the response willl return exactly that token.

#### ``void onErrorHandler(MBOnError handler)``
Very similar to the ``onDataHandler`` call, this allows to catch all error responses with a callback function. 

**Note:** it is not *required* to have an error callback registered. If there is none, errors will vanish unnoticed!

The ``onErrorhandler`` call accepts a function pointer with the following signature only:
```
void func(ModbusClient::Error, uint32_t token);
```
The parameters are 
- ``ModbusClient::Error``, which is one of the error codes defined in ``ModbusTypeDefs.h``:
  ```
  enum Error : uint8_t {
  SUCCESS                = 0x00,
  ILLEGAL_FUNCTION       = 0x01,
  ILLEGAL_DATA_ADDRESS   = 0x02,
  ILLEGAL_DATA_VALUE     = 0x03,
  SERVER_DEVICE_FAILURE  = 0x04,
  ACKNOWLEDGE            = 0x05,
  SERVER_DEVICE_BUSY     = 0x06,
  NEGATIVE_ACKNOWLEDGE   = 0x07,
  MEMORY_PARITY_ERROR    = 0x08,
  GATEWAY_PATH_UNAVAIL   = 0x0A,
  GATEWAY_TARGET_NO_RESP = 0x0B,
  TIMEOUT                = 0xE0,
  INVALID_SERVER         = 0xE1,
  CRC_ERROR              = 0xE2, // only for Modbus-RTU
  FC_MISMATCH            = 0xE3,
  SERVER_ID_MISMATCH     = 0xE4,
  PACKET_LENGTH_ERROR    = 0xE5,
  PARAMETER_COUNT_ERROR  = 0xE6,
  PARAMETER_LIMIT_ERROR  = 0xE7,
  REQUEST_QUEUE_FULL     = 0xE8,
  ILLEGAL_IP_OR_PORT     = 0xE9,
  IP_CONNECTION_FAILED   = 0xEA,
  TCP_HEAD_MISMATCH      = 0xEB,
  UNDEFINED_ERROR        = 0xFF  // otherwise uncovered communication error
  };
- the ``token`` value as described in the ``onDataHandler`` section above.

The Library is providing a separate wrapper class ``ModbusError`` that can be assigned or initialized with any ``ModbusClient::Error`` code and will produce a readable error text message if used in ``const char *`` context. See above in **Basic use** an example of how to apply it.
  
#### ``uint32_t getMessageCount()``
Each request that got successfully enqueued is counted. By calling ``getMessageCount()`` you will be able to read the number accumulated so far.

Please note that this count is instance-specific, so any ModbusClient instance you created will have an own count.

#### ``void begin()`` and ``void begin(int coreID)``
This is the most important call to get a ModbusClient instance to work. It will open the request queue and start the background worker task to process the queued requests.

The second form of ``begin()`` allows you to choose a CPU core for the worker task to run (on multi-core systems as the ESP32 only, of course). This is advisable in particular for the ``ModbusRTU`` client, as the handling of the RS485 Modbus is a little time-critical and will profit from having its own core to run.

**Note:** The worker task is running forever or until the ModbusClient instance is killed that started it. The destructor will take care of all requests still on the queue and remove those, then will stop the running worker task.

### Setting up requests
The function calls to create requests to be sent to servers will take care of the function codes defined by the Modbus standard, in terms of parameter and parameter limits checking, but will also accept other function codes from the user-defined or reserved range of codes. For these non-standard codes no parameter checks can be made, so it is up to you to have them correct!

Any ``addRequest`` call will take the same ``serverID`` and ``functionCode`` parameters as #1 and #2, plus the ``token`` value as last parameter.
``serverID`` must not be 0 and ``functionCode`` has to be below 128, according to the Modbus standard.

There are 7 different ``addRequest`` calls, covering most of the Modbus standard function codes with their parameter lists.

The function codes may be given numerically or by one of the constant names defined in ``ModbusTypeDefs.h``:
```
enum FunctionCode : uint8_t {
  READ_COIL               = 0x01,
  READ_DISCR_INPUT        = 0x02,
  READ_HOLD_REGISTER      = 0x03,
  READ_INPUT_REGISTER     = 0x04,
  WRITE_COIL              = 0x05,
  WRITE_HOLD_REGISTER     = 0x06,
  READ_EXCEPTION_SERIAL   = 0x07,
  DIAGNOSTICS_SERIAL      = 0x08,
  READ_COMM_CNT_SERIAL    = 0x0B,
  READ_COMM_LOG_SERIAL    = 0x0C,
  WRITE_MULT_COILS        = 0x0F,
  WRITE_MULT_REGISTERS    = 0x10,
  REPORT_SERVER_ID_SERIAL = 0x11,
  READ_FILE_RECORD        = 0x14,
  WRITE_FILE_RECORD       = 0x15,
  MASK_WRITE_REGISTER     = 0x16,
  R_W_MULT_REGISTERS      = 0x17,
  READ_FIFO_QUEUE         = 0x18,
  ENCAPSULATED_INTERFACE  = 0x2B,
  USER_DEFINED_41         = 0x41,
  USER_DEFINED_42         = 0x42,
  USER_DEFINED_43         = 0x43,
  USER_DEFINED_44         = 0x44,
  USER_DEFINED_45         = 0x45,
  USER_DEFINED_46         = 0x46,
  USER_DEFINED_47         = 0x47,
  USER_DEFINED_48         = 0x48,
  USER_DEFINED_64         = 0x64,
  USER_DEFINED_65         = 0x65,
  USER_DEFINED_66         = 0x66,
  USER_DEFINED_67         = 0x67,
  USER_DEFINED_68         = 0x68,
  USER_DEFINED_69         = 0x69,
  USER_DEFINED_6A         = 0x6A,
  USER_DEFINED_6B         = 0x6B,
  USER_DEFINED_6C         = 0x6C,
  USER_DEFINED_6D         = 0x6D,
  USER_DEFINED_6E         = 0x6E,
};
```
Any call will return immediately and bring back an ``Error`` different from ``SUCCESS`` if the parameter check has found some violation of the standard definitions in the parameters.

Please pay special attention to the ``PARAMETER_COUNT_ERROR``, that will indicate you may have used a different ``addRequest`` call not suitable for the function code you provided.

#### 1) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token)``
This is for function codes that will require no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  

### ModbusRTU API elements
// Constructor takes Serial reference and optional DE/RE pin and queue limit
  explicit ModbusRTU(HardwareSerial& serial, int8_t rtsPin = -1, uint16_t queueLimit = 100);

  // Set default timeout value for interface
  void setTimeout(uint32_t TOV);

  // Methods to set up requests
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // Method to generate an error response - properly enveloped for TCP
  RTUMessage generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode);


### ModbusTCP API elements
 // Constructor takes reference to Client (EthernetClient or WiFiClient)
  explicit ModbusTCP(Client& client, uint16_t queueLimit = 100);

  // Alternative Constructor takes reference to Client (EthernetClient or WiFiClient) plus initial target host
  ModbusTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusTCP();

  // begin: start worker task
  void begin(int coreID = -1);

  // Set default timeout value (and interval)
  void setTimeout(uint32_t timeout, uint32_t interval = TARGETHOSTINTERVAL);

  // Switch target host (if necessary)
  bool setTarget(IPAddress host, uint16_t port, uint32_t timeout = 0, uint32_t interval = 0);

  // Methods to set up requests
  // 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode);
  
  // 2. one uint16_t parameter (FC 0x18)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1);
  
  // 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);
  
  // 4. three uint16_t parameters (FC 0x16)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);
  
  // 5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);
  
  // 6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);

  // 7. generic constructor for preformatted data ==> count is counting bytes!
  Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token = 0);
  TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);

  // Method to generate an error response - properly enveloped for TCP
  TCPMessage generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode);

