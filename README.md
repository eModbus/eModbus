# ModbusUnified
This is a library to provide Modbus client (formerly known as master) and server (formerly slave) functionalities for both Modbus RTU and TCP protocols.

Modbus communication is done in separate tasks, so Modbus requests and responses are non-blocking. Callbacks are provided to prepare or receive the responses asynchronously.

This has been developed by enthusiasts, so do not expect any bullet-proof, industry deployable, guaranteed software. **See the license** to learn about liabilities etc. You are at your own!

We do welcome any ideas, suggestions, bug reports or questions, though. Please use the "Issues"" tab to report these!

Have fun!

## Table of Contents
- [Requirements](#requirements)
- [Installation](#installation)
- [ModbusClient](#modbusclient)
  - [Basic use](#basic-use)
  - [API description](#api-description)
    - [Common concepts](#common-concepts)
    - [ModbusClientRTU API elements](#modbusclientrtu-api-elements)
    - [ModbusClientTCP API elements](#modbusclienttcp-api-elements)
- [ModbusServer](#modbusserver)
  - [Basic use](#basic-use-1)
  - [API description](#api-description-1)
    - [Common concepts](#common-concepts-1)
    - [ModbusServerTCP](#modbusservertcp)
    - [ModbusServerEthernet](#modbusserverethernet)
    - [ModbusServerWiFi](#modbusserverwifi)
    - [ModbusServerRTU](#modbusserverrtu)

## Requirements
The library was developed for and on ESP32 MCUs in the Arduino core development environment. In principle it should run in any environment providing these resources:
- Arduino core functions - Serial interfaces, ``millis()``, ``delay()`` etc.
- FreeRTOS' ``xTask`` task functions. Note that there are numerous MCUs running FreeRTOS!
- C++ standard library components
  - ``std::queue``
  - ``std::vector``
  - ``std::map``
  - ``std::mutex``
  - ``std::lock_guard``

For Modbus RTU you will need a RS485-to-Serial adaptor. Commonly used are the **RS485MAX** adaptors or the **XY-017** adaptors with automatic half duplex control.

Modbus TCP will require either an Ethernet module like the WizNet W5xxx series connected to the SPI interface or an (internal or external) WiFi adaptor. The client library is by the way using the functions defined in ``Client.h`` only, whereas the server library is requiring either ``Ethernet.h`` or ``Wifi.h`` internally.

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
## ModbusClient

### Basic use
To get up and running you will need to put only a few lines in your code. *We will be using the RTU variant here* to show an example.
First you have to pull the matching header file:
```
#include "ModbusClientRTU.h"
```
For ModbusClientRTU a Serial interface is required, that connects to your RS485 adaptor. This is given as parameter to the ``ModbusClientRTU`` constructor:
```
ModbusClientRTU RS485(Serial2);
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

// Set up ModbusClientRTU client.
// - provide onData and onError handler functions
  RS485.onDataHandler(&handleData);
  RS485.onErrorHandler(&handleError);

// Start ModbusClientRTU background task
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
### API description
Following we will first describe the API elements independent from the protocol adaptor used and after that the specifics of both RTU and TCP interfaces.
#### Common concepts
Internally the Modbus message contents for requests and responses are held separately from the elements the protocols will require. Hence several API elements are common to both protocols.

##### ``void onDataHandler(MBOnData handler)``
This is the interface to register a callback function to be called when a response was received. The only parameter is a pointer to a function with this signature:
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

##### ``void onErrorHandler(MBOnError handler)``
Very similar to the ``onDataHandler`` call, this allows to catch all error responses with a callback function. 

**Note:** it is not *required* to have an error callback registered. If there is none, errors will vanish unnoticed!

The ``onErrorhandler`` call accepts a function pointer with the following signature only:
```
void func(ModbusClient::Error, uint32_t token);
```
The parameters are 
- ``ModbusClient::Error``, which is one of the <a name="errorcodes"></a>error codes defined in ``ModbusTypeDefs.h``:
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

The Library is providing a separate wrapper class ``ModbusError`` that can be assigned or initialized with any ``ModbusClient::Error`` code and will produce a readable error text message if used in ``const char *`` context. See above in [**Basic use**](#basic-use) an example of how to apply it.
  
##### ``uint32_t getMessageCount()``
Each request that got successfully enqueued is counted. By calling ``getMessageCount()`` you will be able to read the number accumulated so far.

Please note that this count is instance-specific, so any ModbusClient instance you created will have an own count.

##### ``void begin()`` and ``void begin(int coreID)``
This is the most important call to get a ModbusClient instance to work. It will open the request queue and start the background worker task to process the queued requests.

The second form of ``begin()`` allows you to choose a CPU core for the worker task to run (on multi-core systems as the ESP32 only, of course). This is advisable in particular for the ``ModbusClientRTU`` client, as the handling of the RS485 Modbus is a little time-critical and will profit from having its own core to run.

**Note:** The worker task is running forever or until the ModbusClient instance is killed that started it. The destructor will take care of all requests still on the queue and remove those, then will stop the running worker task.

#### Setting up requests
The function calls to create requests to be sent to servers will take care of the function codes defined by the Modbus standard, in terms of parameter and parameter limits checking, but will also accept other function codes from the user-defined or reserved range of codes. For these non-standard codes no parameter checks can be made, so it is up to you to have them correct!

Any ``addRequest`` call will take the same ``serverID`` and ``functionCode`` parameters as first and second, plus the ``token`` value as last parameter.
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

##### 1) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token)``
This is for function codes that will require no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
  
##### 2) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token)``
There is one function code defined in the standard that will require one uint16_t parameter (FC 0x18).

##### 3) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token)``
The vast majority of all Modbus requests are probably those with two uint16_t parameters, mostly used for address and number of registers/coils/etc. (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)

##### ``4) Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token)``
Among the standardized function codes, just one takes three uint16_t parameters (FC 0x16)

##### 5) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token)``
Again only one will require two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words, but it is the frequently used "write multiple registers" code (FC 0x10)

##### 6) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token)``
The sixth ``addRequest`` is used for function code "write multiple coils": two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)

##### 7) ``Error addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token)``
Final of the 7 ``addRequest`` calls is a generic one. This is meant to fit all other function codes that do not match the parameter constellations of the previous 6. 

Besides the standard arguments ``serverID``, ``functionCode`` and ``token``, this call takes ``*arrayOfBytes``, a pointer to a uint8_t array of bytes and the length of it in ``uint16_t count``.

The ``arrayOfBytes`` has to contain prepared data MSB first, as it is copied unchecked into the request generated. Everything is up to you here!

To help setting up such an ``arrayOfBytes``, the library provides a service function:

##### ``uint16_t addValue(uint8_t *target, uint16_t targetLength, T v)``
This service function takes the integral value of type ``T`` provided as parameter ``v`` and will write it, MSB first, to the ``uint8_t`` array pointed to by ``target``. If ``targetLength`` does not indicate enough space left, the copy is not made.

In any case the function will return the number of bytes written. A typical application may look like:
```
uint8_t u = 4;
uint16_t w = 1276;
uint32_t l = 0xDEADBEEF;
uint8_t buffer[24];
uint16_t remaining = 24;

remaining -= addValue(buffer + 24 - remaining, remaining, u);
remaining -= addValue(buffer + 24 - remaining, remaining, w);
remaining -= addValue(buffer + 24 - remaining, remaining, l);
```
After the code is run ``buffer`` will contain ``04 04 FC DE AD BE EF`` and ``remaining`` will be 17.

#### ModbusClientRTU API elements
You will have to have the following include line in your code to make the ``ModbusClientRTU`` API available:
```
#include "ModbusClientRTU.h"
```
##### ``ModbusClientRTU(HardwareSerial& serial)``, ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin)`` and ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit)``
These are the constructor variants for an instance of the ``ModbusClientRTU`` type. The parameters are:
- ``serial``: a reference to a Serial interface the Modbus is conncted to (mostly by a RS485 adaptor). This Serial interface must be configured to match the baud rate, data and stop bits and parity of the Modbus.
- ``rtsPin``: some RS485 adaptors have "DE/RE" lines to control the half duplex communication. When writing to the bus, the lines have to be set accordingly. DE and RE usually have opposite logic levels, so that they can be connected to a single GPIO that is set to HIGH for writing and LOW for reading. This will be done by the library, if a GPIO pin number is given for ``rtsPin``. Use ``-1`` as value if you do not need this GPIO (usually with RS485 adaptors doing auto half duplex themselves).
- ``queueLimit``: this specifies the number of requests that may be placed on the worker task's queue. If you exceed this number by issueing another request while the queue is full, the ``addRequest`` call will return with a ``REQUEST_QUEUE_FULL`` error. The default value built in is 100. **Note:** while the queue holds pointers to the requests only, the requests need memory as well. If you choose a ``queueLimit`` too large, you may encounter "out of memory" conditions!

##### ``void setTimeout(uint32_t TOV)``
This call lets you define the time for stating a response timeout. ``TOV`` is defined in milliseconds. when the worker task is waiting for a response from a server, and the specified number of milliseconds has passed without data arriving, the worker task gives up on that and will return a ``TIMEOUT`` error response instead.

**Note:** this timeout is blocking the worker task. Nothing else will be done until the timeout has occured. Worse, the worker will retry the failing request two more times! So the ``TOV`` value in effect can block the worker up to three times the time you specified, if a server is dead. 
Too short timeout values on the other hand may miss slow servers' responses, so choose your value with care...

##### Message generating calls (no communication)
If you need Modbus messages properly formatted for other reasons (interfaces not covered by this lib, documentation purposes etc.), there is a set of calls that will take the same parameters as the ``addRequest`` calls, do the parameter checks all the same, but will not put the requests on the worker task's queue to be sent to the bus, but return the message to your call.

The messages will also contain the trailing CRC16 bytes Modbus RTU is requiring. 
The message data is returned as a ``RTUMessage`` data object, that internally is a ``std::vector<uint8_t>`` and may be used as a ``vector``, using iterators, ``size()``, ``data()`` etc.

The first seven ``generateRequest`` calls are siblings of the respective seven ``addRequest`` calls, leaving out the ``token`` parameter not needed here:
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes)``
- ``RTUMessage generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes)``

There is one more ``generate`` call to create a properly formed error response message:

``RTUMessage generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode)``

The ``errorCode`` parameter takes an ``Error`` value as defined above. The resulting message returned also will have the correct CRC16 data added to it.

##### ``RTUCRC::calcCRC16(uint8_t *data, uint16_t len)``
This is a convenient method to calculate a CRC16 value for a given block of bytes. ``*data`` points to this block, ``len`` gives the number of bytes to consider.
The call will return the 16-bit CRC16 value.

#### ModbusClientTCP API elements
You will have to have the following include line in your code to make the ``ModbusClientTCP`` API available:
```
#include "ModbusClientTCP.h"
```
##### ``ModbusClientTCP(Client& client)`` and ``ModbusClientTCP(Client& client, uint16_t queueLimit)``
The first set of constructors does take a ``client`` reference parameter, that may be any interface instance supporting the methods defined in ``Client.h``, f.i. an ``EthernetClient`` or a ``WiFiClient`` instance.
This interface will be used to send the Modbus TCP requests and receive the respective TCP responses.

The optional ``queueLimit`` parameter lets you define the maximum number of requests the worker task's queue will accept. The default is 100; please see the remarks to this parameter in the ModbusClientRTU section.

##### ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port)`` and ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit)``
Alternatively you may give the initial target host IP address and port number to be used for communications. This can be sensible if you have to set up a ModbusClientTCP client dedicated to one single target host.

The ``queueLimit`` parameter is the same as explained above.

##### ``void setTimeout(uint32_t timeout)`` and ``void setTimeout(uint32_t timeout, uint32_t interval)`` 
Similar to the ModbusClientRTU timeout, you may specify a time in milliseconds that will determine if a ``TIMEOUT`` error occurred. The worker task will wait the specified time without data arriving to then state a timeout and return the error response for it. The default value is 2000 - 2 seconds.

**Note:** the caveat for the ModbusClientRTU timeout applies here as well. The timeout will block the worker task up to three times its value, as two retries are attempted by the worker by sending the request again and waiting for a response. 

The optional ``interval`` parameter also is given in milliseconds and specifies the time to wait for the worker between two consecutive requests to the same target host. Some servers will need some milliseconds to recover from a previous request; this interval prevents sending another request prematurely. 

**Note:** the interval is also applied for each attempt to send a request, it will add to the timeout! To give an example: ``timeout=2000`` and ``interval=200`` will result in 6600ms inactivity, if the target host notoriously does not answer.

##### ``bool setTarget(IPAddress host, uint16_t port [, uint32_t timeout [, uint32_t interval]]``
This function is necessary at least once to set the target host IP address and port number (unless that has been done with the constructor already). All requests will be directed to that host/port, until another ``setTarget()`` call is issued.

The optional ``timeout`` and ``interval`` parameters will let you override the standards set with the ``setTimeout()`` method **for just those requests sent from now on to the targeted host/port**. The next ``setTarget()`` will return to the standard values, if not specified differently again.

##### Message generating calls (no communication)
The ModbusClientTCP ``generate`` calls are identical in function to those described in the ModbusClientRTU section, with the following differences:

- as first parameter, the ``generate`` calls are requiring a 16-bit ``transactionID`` transaction identification. This will be used as part of the TCP header block prefixed to the message proper, as Modbus TCP mandates.
If sent as a request, the TCP response shall return the same transaction identifier in its TCP header.
- the messages generated will be returned as a ``TCPMessage`` object instance. While this as well is a ``std::vector<uint8_t>`` internally, it has the 6-byte TCP header additionally (and no CRC16, of course, as that is for RTU only).

The calls are:
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes)``
- ``TCPMessage generateRequest(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes)``
- ``TCPMessage generateErrorResponse(uint16_t transactionID, uint8_t serverID, uint8_t functionCode, Error errorCode)``
For a description please see the related calls above in the ModbusClientRTU section and the ``addRequest`` call descriptions in the common section.

## ModbusServer
ModbusServer (aka slave) allows you to concentrate on the server functionality - data provision, manipulation etc. -, while the library will do the communication part.

You will write callback functions to handle requests for accepted function codes and register those with the ModbusServer. All requests matching one of the registered callbacks will lead to these callbacks being called to get a response, that in turn is sent back to the requester.

Any request for a function code without one of your callbacks registered for it will be answered by an ILLEGAL_FUNCTION_CODE response automatically.

### Basic use
Here is an example of a Modbus server running on WiFi, that reacts on function codes 0x03:``READ_HOLD_REGISTER`` and 0x04:``READ_INPUT_REGISTER`` with the same callback function.
```
#include <Arduino.h>
#include "ModbusServerWiFi.h"

char ssid[] = "YOURNETWORKSSID";
char pass[] = "YOURNETWORKPASS";
// Set up a Modbus server
ModbusServerWiFi MBserver;
IPAddress lIP;                      // assigned local IP

const uint8_t MY_SERVER(1);
uint16_t memo[128];                 // Test server memory

// Worker function for serverID=1, function code 0x03 or 0x04
ResponseType FC03(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read

  // Get addr and words from data array. Values are MSB-first, getValue() will convert to binary
  // Returned: number of bytes eaten up 
  uint16_t offs = getValue(data, dataLen, addr);
  offs += getValue(data + offs, dataLen - offs, wrds);
  
  // address valid?
  if (!addr || addr > 128) {
    // No. Return error response
    return MBserver.ErrorResponse(ILLEGAL_DATA_ADDRESS);
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Number of words valid?
  if (!wrds || (addr + wrds) > 127) {
    // No. Return error response
    return MBserver.ErrorResponse(ILLEGAL_DATA_ADDRESS);
  }

  // Data buffer for returned values. +1 for the leading length byte!
  uint8_t rdata[wrds * 2 + 1];

  // Set length byte
  rdata[0] = wrds * 2;
  offs = 1;

  // Loop over all words to be sent
  for (uint16_t i = 0; i < wrds; i++) {
    // Add word MSB-first to response buffer
    offs += addValue(rdata + offs, wrds * 2 - offs + 1, memo[addr + i]);
  }

  // Return the data response
  return MBserver.DataResponse(wrds * 2 + 1, rdata);
}

void setup() {
// Init Serial
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

  // Register the worker function with the Modbus server
  MBserver.registerWorker(MY_SERVER, READ_HOLD_REGISTER, &FC03);
  
  // Register the worker function again for another FC
  MBserver.registerWorker(MY_SERVER, READ_INPUT_REGISTER, &FC03);
  
  // Connect to WiFi 
  WiFi.begin(ssid, pass);
  delay(200);
  int status = WiFi.status();
  while (status != WL_CONNECTED) {
    Serial.print('.');
    WiFi.disconnect(true);
    WiFi.begin(ssid, pass);
    delay(1000);
    status = WiFi.status();
  }

  // print local IP address:
  lIP = WiFi.localIP();
  Serial.printf("My IP address: %u.%u.%u.%u\n", lIP[0], lIP[1], lIP[2], lIP[3]);

  // Initialize server memory with consecutive values
  for (uint16_t i = 0; i < 128; ++i) {
    memo[i] = (i * 2) << 8 | ((i * 2) + 1);
  }

  // Start the Modbus TCP server:
  // Port number 502, maximum of 4 clients in parallel, 10 seconds timeout
  MBserver.start(502, 4, 10000);
}

void loop() {
  static uint32_t statusTime = millis();
  const uint32_t statusInterval(10000);

  // We will be idling around here - all is done in subtasks :D
  if (millis() - statusTime > statusInterval) {
    Serial.printf("%d clients running.\n", MBserver.activeClients());
    statusTime = millis();
  }
  delay(100);
}
```

### API description

#### Common concepts
All ModbusServer variants have some calls in common, regardless of the protocol or interface they will use.

##### ``void registerWorker(uint8_t serverID, uint8_t functionCode, MBSworker worker)``
The ``registerWorker`` method is used to announce callback functions to the Modbus server that will serve certain function codes.

These callback functions must have a function signature of:
``ResponseType (*MBSworker) (uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t *data)``,
where the parameters given to the callback are 
- ``serverID``: the Modbus server identification (1 - 247) from the request
- ``functionCode``: the function code requested
- ``dataLen``: the number of bytes in ``data``
- ``data``: a buffer with the request data received

**Note**: you may specify different server identifications for registered callbacks. This way the server you are providing can cover more than one Modbus server at a time. You will have to check the requested serverID in the callback to learn which of the registered servers was meant in the request.

There is no limit in registered callbacks, but a second register for a certain serverID/functionCode combination will overwrite the first without warning.

A ``MBSworker`` callback must return a data object of ``ResponseType``. This is in fact a ``std::vector<uint8_t>`` and may be used likewise. There are basically five different ``ResponseType`` return variants:
- ``NIL_RESPONSE``: no response at all will be sent back to the requester
- ``ECHO_RESPONSE``: the request will be sent back without any modification as a response. This is common for the writing function code requests the Modbus standard defines.
- ``ErrorResponse(Error errorCode)``: the error code should be one of the defined error codes in the lib (see [Error codes](#errorcodes)). It will be completed to a standard Modbus error response.
- ``DataResponse(uint16_t dataLen, uint8_t *data)``: this is the regular response for data requests etc. You will have to fill the ``data`` buffer with the response data starting at the first byte after the function code. Please note that the data has to be in MSB-first format. The ``addValue()`` service method will help you putting your data into that required order.
- free form: you may of course return any ``std::vector<uint8_t>`` you like as well. It will be sent as is, no addition of serverID or function code etc. will be done by the lib in this case.

  **Note**: do not use the first byte as ``0xFF``, followed by one of ``0xF0``, ``0xF1``, ``0xF2`` or ``0xF3``, as these are used internally for NIL_RESPONSE, ECHO_RESPONSE, ErrorResponse and DataResponse, respectively!

##### ``MBSworker getWorker(uint8_t serverID, uint8_t functionCode)``
You may check if a given serverID/functionCode combination has been covered by a callback with ``getWorker()``. This method will return the callback function pointer, if there is any, and a ``nullptr`` else.

##### ``bool isServerFor(uint8_t serverID)``
``isServerFor()`` will return ``true``, if at least one callback has been registered for the given ``serverID``, and ``false`` else.

##### ``uint32_t getMessageCount()``
Each request received will be counted. The ``getMessageCount()`` method will return the current state of the counter.

#### ModbusServerTCP
Inconsistencies between the WiFi and Ethernet implementations in the core libraries required a split of ModbusServerTCP into the ``ModbusServerEthernet`` and ``ModbusServerWiFi`` variants.

Both are sharing the majority of methods etc., but need to be included with different file names and have different object names as well.

All relevant calls after initialization of the respective ModbusServer object are identical.

You can use the ``TCPMODE`` macro to tell in your code, which variant you are using - ``#if TCPMODE == ETHERNET`` will be true in the Ethernet case, ``#if TCPMODE == WIFI`` in the other.

##### ModbusServerEthernet
To use the Ethernet version, you will have to use
```
#include "ModbusServerEthernet.h"
...
ModbusServerEthernet myServer;
...
```
in your source file. You will not have to add ``#include <Ethernet.h>``, as this is done in the library. It does not do any harm if you do, though.

##### ModbusServerWiFi
To use the WiFi version, you will have to use
```
#include "ModbusServerWiFi.h"
...
ModbusServerWiFi myServer;
...
```
in your source file. You will not have to add ``#include <WiFi.h>``, as this is done in the library. It does not do any harm if you do, though.

#### Common calls
##### ``uint16_t activeClients()``
This call will give you the number of clients that currently are connected to your ModbusServer. 

##### ``bool start(uint16_t port, uint8_t maxClients, uint32_t timeout)``
After callback functions have been registered, the Modbus server is started with this method. The parameters are:

- ``port``: the port number the server will liten to. Usually for Modbus this is port 502, but there are situations where another port number is adequate.
- ``maxClients``: the maximum number of concurrent client connections the server will accept. This is mainly important for Ethernet with the WizNet modules, that can only provide 4 or 8 sockets simultaneously. In this case it can be sensible to restrict the connections of the Modbus server to have connections left for other tasks.
- ``timeout``: this is given in milliseconds and defines the timespan of inactivity, after which the connection to a client is cut.

  The connection may be cut on the far side by the client at any time, the respective Modbus server connection will be freed immediately, independently of the timeout.

  A ``timeout`` value of ``0`` is possible - this will prevent any timeout handling! If the timeout is set to 0, the server will rely on the far side closing the connection!

- There is in fact a fourth, optional parameter ``int coreID``: if you will add a number here, the server task will be started on the named core# on multi-core MCUs.

The ``start()`` call will create a TCP server task in the background that is listening on the said port. If a connection request comes in, another task with a client handling that connection is started.

A client task is stopped again, if the timeout hits or if the far side has closed the connection.

##### ``bool stop()``
The ``stop()`` call will close all open connections, free allocated memory and finally stop the server task. At this time your Modbus server will not respond to any connection request any more. 

It can be restarted, though, with another ``start()`` call, even with different ``port``, ``maxClient`` or ``timeout`` setting.

#### ModbusServerRTU


