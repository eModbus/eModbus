# ModbusUnified

![ModbusUnified](https://github.com/ESP32ModbusUnified/ModbusUnified/workflows/Building/badge.svg)

This is a library to provide Modbus client (formerly known as master), server (formerly slave) and bridge/gateway functionalities for both Modbus RTU and TCP protocols.

Modbus communication is done in separate tasks, so Modbus requests and responses are non-blocking. Callbacks are provided to prepare or receive the responses asynchronously.

Key features:
- for use in the Arduino framework
- non blocking / asynchronous API
- server, client and bridge modes
- TCP (Ethernet, WiFi and Async) and RTU interfaces
- all common and user-defined Modbus standard function codes 

This has been developed by enthusiasts. While we do our utmost best to make robust software, do not expect any bullet-proof, industry deployable, guaranteed software. **See the license** to learn about liabilities etc.

We do welcome any ideas, suggestions, bug reports or questions. Please use the "Issues" tab to report these!

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
      - [ModbusServerTCPasync](#modbusservertcpasync)
    - [ModbusServerRTU](#modbusserverrtu)
- [ModbusBridge](#modbusbridge)
  - [Basic use](#basic-use-2)
  - [API description](#api-description-2)
  - [Filtering](#filtering)
- [Logging](#logging)

## Requirements
The library was developed for and on ESP32 MCUs in the Arduino core development environment. In principle it should run in any environment providing these resources:
- Arduino core functions - Serial interfaces, ``millis()``, ``delay()`` etc.
- FreeRTOS' ``xTask`` task functions. Note that there are numerous MCUs running FreeRTOS!
- C++ standard library components
  - ``std::queue``
  - ``std::vector``
  - ``std::list``
  - ``std::map``
  - ``std::mutex``
  - ``std::lock_guard``
  - ``std::forward``
  - ``std::function``
  - ``std::bind``

For Modbus RTU you will need a RS485-to-Serial adaptor. Commonly used are the **RS485MAX** adaptors or the **XY-017** adaptors with automatic half duplex control. Be sure to check compatibility of voltage levels.

Modbus TCP will require either an Ethernet module like the WizNet W5xxx series connected to the SPI interface or an (internal or external) WiFi adaptor. The client library is by the way using the functions defined in ``Client.h`` only, whereas the server and bridge library is requiring either ``Ethernet.h`` or ``Wifi.h`` internally. Async versions rely on the [AsyncTCP library](https://github.com/me-no-dev/AsyncTCP).

## Installation
We recommend to use the [PlatformIO IDE](https://platformio.org/). There, in your project's ``platformio.ini`` file, add the Github URL of this library to your ``lib_deps =`` entry in the ``[env:...]`` section of that file:
```
[platformio]

# some settings

[env:your_project_target]

# some settings

lib_deps = 
  ModbusClient=https://github.com/ESP32ModbusUnified/ModbusUnified.git
```

If you are using the Arduino IDE, you will want to copy the library folder into your library directory. In Windows this will be  ``C:\Users\<user_name>\Documents\Arduino\libraries`` normally. For Arduino IDE, you'll have to install the dependencies manually. These are:
- AsyncTCP (https://github.com/me-no-dev/AsyncTCP)
- Ethernet-compatible library (example: https://github.com/maxgerhardt/Ethernet)

[Return to top](#modbusunified)

## ModbusClient

### Basic use
To get up and running you will need to put only a few lines in your code. *We will be using the RTU variant here* to show an example.
First you have to include the matching header file:

```
#include "ModbusClientRTU.h"
```

For ModbusClientRTU a Serial interface is required, that connects to your RS485 adaptor. This is given as parameter to the ``ModbusClientRTU`` constructor. An optional pin can be given to use half-duplex.

```
ModbusClientRTU RS485(Serial2);          // for auto half-duplex
ModbusClientRTU RS485(Serial2, rtsPin);  // use rtsPin to toggle DE/RE in half-duplex
```

Next we will define a callback function for data responses coming in. This example will print out a hexadecimal dump of the response data:

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

Optionally we can define another callback to be called when an error response was received. The callback will give you an ``Error`` which can be converted to a human-readable ``ModbusError``:

```
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  Serial.printf("Error response: %02X - %s\n", (int)me, (const char *)me);
}
```

Now we will bring everything together in the ``setup()`` function:

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

You can start making requests now. In the code below a "read holding register" request is sent to server id ``1``, requesting one data word at address 10. Please disregard the ``token`` value of 0x12345678 for now, it will be explained further below:

```
Error err = RS485.addRequest(1, READ_HOLD_REGISTER, 10, 1, 0x12345678);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }
```

This method enqueues your request and returns immediately. If anything was wrong with your parameters or the queue was full, the request will not be created. You will be given an error return value to find out what went wrong.
In case everything was okay, the request is created and put into the background queue to be processed. Upon response, your ``onData`` or ``onError`` callbacks will be called.

### API description
Core concepts are the same for all interfaces. We will first describe the API elements independent from the interface used and further down the specifics of the RTU and TCP interfaces.

#### Common concepts
Internally the Modbus message contents for requests and responses are held separately from the elements the protocols will require. Hence several API elements are common to both protocols.

##### ``bool onDataHandler(MBOnData handler)``
This is the interface to register a callback function to be called when a response was received. The only parameter is a pointer to a function with this signature:

```
void func(uint8_t serverID, uint8_t functionCode, const uint8_t *data, uint16_t data_length, uint32_t token);
```

- ``serverID``: the Modbus server number (range 1..247)
- ``functionCode``: the function from the request that led to this response
- ``*data`` and ``data_length``: data points to a buffer containing the response message bytes, ``data_length`` gives the number of bytes in the message.
- ``token``: this is a general concept throughout the library, see below.

**Note**: a handler can only be claimed once. If that already has been done before, subsequent calls are returning a ``false`` value.

#### The ``token`` concept
Each request must be given a user-defined ``token`` value. This token is saved with each request and is returned in the callback. No processing whatsoever is done on the token, you will get what you gave. This enables a user to keep track of the sent requests when receiving a response.

Imagine an application that has several ModbusClients working; some for dedicated TCP Modbus servers, another for a RS485 RTU Modbus and another again to request different TCP servers.

If you only want to have a single onData and onError function handling all responses regardless of the server that sent them, you will need a means to tell one response from the other. 

Your token you gave at request time will tell you that, as the response will return exactly that token.

##### `bool onErrorHandler(MBOnError handler)`
Very similar to the ``onDataHandler`` call, this allows to catch all error responses with a callback function.

**Note:** it is not *required* to have an error callback registered. But if there is none, errors will go unnoticed!

The ``onErrorhandler`` call accepts a function pointer with the following signature only:

```
void func(ModbusClient::Error, uint32_t token);
```

The parameters are 
- ``ModbusClient::Error``, which is one of the error codes <a name="errorcodes"></a>defined in ``ModbusTypeDefs.h``:
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
  ```
- the ``token`` value as described in the ``onDataHandler`` section above.

The Library is providing a separate wrapper class ``ModbusError`` that can be assigned or initialized with any ``ModbusClient::Error`` code and will produce a human-readable error text message if used in ``const char *`` context. See above in [**Basic use**](#basic-use) an example of how to apply it.
  
##### ``uint32_t getMessageCount()``
Each request that got successfully enqueued is counted. By calling ``getMessageCount()`` you will be able to read the number accumulated so far.

Please note that this count is instance-specific, so any ModbusClient instance you created will have its own count.

##### ``void begin()`` and ``void begin(int coreID)``
This is the most important call to get a ModbusClient instance to work. It will open the request queue and start the background worker task to process the queued requests.

The second form of ``begin()`` allows you to choose a CPU core for the worker task to run (only on multi-core systems like the ESP32). This is advisable in particular for the ``ModbusClientRTU`` client, as the handling of the RS485 Modbus is a time-critical and will profit from having its own core to run.

**Note:** The worker task is running forever or until the ModbusClient instance is killed that started it. The destructor will take care of all requests still on the queue and remove those, then will stop the running worker task.

#### Setting up requests
The function calls to create requests to be sent to servers will take care of the function codes defined by the Modbus standard, in terms of parameter and parameter limits checking, but will also accept other function codes from the user-defined or reserved range of codes. For these non-standard codes no parameter checks can be made, so it is up to you to have them correct!

Any ``addRequest`` call will take the same ``serverID`` and ``functionCode`` parameters as first and second, plus the ``token`` value as last parameter.
``serverID`` must not be 0 and ``functionCode`` has to be non-zero and below 128, according to the Modbus standard.

There are 7 different ``addRequest`` calls, covering most of the Modbus standard function codes with their parameter lists.

The function codes <a name="functioncodes"></a>may be given numerically or by one of the constant names defined in ``ModbusTypeDefs.h``:
```
enum FunctionCode : uint8_t {
  ANY_FUNCTION_CODE       = 0x00,  // only valid to be used by ModbusServer!
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
This is for function codes that will require no additional parameter (FCs 0x07, 0x0B, 0x0C, 0x11)
  
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
uint8_t u = 4;      // 0x04
uint16_t w = 1276;  // 0x04FC 
uint32_t l = 0xDEADBEEF;
uint8_t buffer[24];
uint16_t remaining = 24;

remaining -= addValue(buffer + 24 - remaining, remaining, u);
remaining -= addValue(buffer + 24 - remaining, remaining, w);
remaining -= addValue(buffer + 24 - remaining, remaining, l);
```

After the above code is run ``buffer`` will contain ``04 04 FC DE AD BE EF`` and ``remaining`` will be 17.

#### ModbusClientRTU API elements
You will have to have the following include line in your code to make the ``ModbusClientRTU`` API available:

```
#include "ModbusClientRTU.h"
```

##### ``ModbusClientRTU(HardwareSerial& serial)``, ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin)`` and ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit)``
These are the constructor variants for an instance of the ``ModbusClientRTU`` type. The parameters are:
- ``serial``: a reference to a Serial interface the Modbus is conncted to (mostly by a RS485 adaptor). This Serial interface must be configured to match the baud rate, data and stop bits and parity of the Modbus.
- ``rtsPin``: some RS485 adaptors have "DE/RE" lines to control the half duplex communication. When writing to the bus, the lines have to be set accordingly. DE and RE usually have opposite logic levels, so that they can be connected to a single GPIO that is set to HIGH for writing and LOW for reading. This will be done by the library, if a GPIO pin number is given for ``rtsPin``. Leave this parameter out or use ``-1`` as value if you do not need this GPIO (usually with RS485 adaptors doing auto half duplex themselves).
- ``queueLimit``: this specifies the number of requests that may be placed on the worker task's queue. If the queue has reached this limit, the next ``addRequest`` call will return a ``REQUEST_QUEUE_FULL`` error. The default value built in is 100. **Note:** while the queue holds pointers to the requests only, the requests need memory as well. If you choose a ``queueLimit`` too large, you may encounter "out of memory" conditions!

##### ``void setTimeout(uint32_t TOV)``
This call lets you define the time for stating a response timeout. ``TOV`` is defined in **milliseconds**. When the worker task is waiting for a response from a server, and the specified number of milliseconds has passed without data arriving, it will return a ``TIMEOUT`` error response.

**Note:** This timeout is blocking the worker task. No other requests will be made while waiting for a response. Furthermore, the worker will retry the failing request two more times. So the ``TOV`` value in effect can block the worker up to three times the time you specified, if a server is dead. 
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

##### ``uint16_t RTUutils::calcCRC16(uint8_t *data, uint16_t len)``
This is a convenient method to calculate a CRC16 value for a given block of bytes. ``*data`` points to this block, ``len`` gives the number of bytes to consider.
The call will return the 16-bit CRC16 value.

##### ``bool RTUutils::validCRC(const uint8_t *data, uint16_t len)``
If you got a buffer holding a Modbus message including a CRC, ``validCRC()`` will tell you if it is correct - by calculating the CRC16 again and comparing it to the last two bytes of the message buffer.

##### ``bool RTUutils::validCRC(const uint8_t *data, uint16_t len, uint16_t CRC)``
``validCRC()`` has a second variant, where you can provide a pre-calculated (or otherwise obtained) CRC16 to be compared to that of the given message buffer.

##### ``void RTUutils::addCRC(RTUMessage& raw)``
If you have a ``RTUMessage`` (see above), you can calculate a valid CRC16 tnd have it pushed o its end with ``addCRC()``.

#### ModbusClientTCP API elements
You will have to include one of these lines in your code to make the ``ModbusClientTCP`` API available:

```
#include "ModbusClientTCP.h"
#include "ModbusclientTCPasync.h"
```

##### ``ModbusClientTCP(Client& client)`` and ``ModbusClientTCP(Client& client, uint16_t queueLimit)``
The first set of constructors does take a ``client`` reference parameter, that may be any interface instance supporting the methods defined in ``Client.h``, f.i. an ``EthernetClient`` or a ``WiFiClient`` instance.
This interface will be used to send the Modbus TCP requests and receive the respective TCP responses.

The optional ``queueLimit`` parameter lets you define the maximum number of requests the worker task's queue will accept. The default is 100; please see the remarks to this parameter in the ModbusClientRTU section.

##### ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port)`` and ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit)``
Alternatively you may give the initial target host IP address and port number to be used for communications. This can be sensible if you have to set up a ModbusClientTCP client dedicated to one single target host.

##### _**AsyncTCP only**_ ``ModbusClientTCPasync(IPAddress host, uint16_t port)`` and ``ModbusClientTCPasync(IPAddress host, uint16_t port, uint16_t queueLimit)``
The asynchronous version takes 3 arguments: the target host IP address and port number of the modbus server and an optionally queue size limit (defaults to 100). The async version connects to one modbus server.

##### ``void setTimeout(uint32_t timeout)`` and ``void setTimeout(uint32_t timeout, uint32_t interval)`` 
Similar to the ModbusClientRTU timeout, you may specify a time in milliseconds that will determine if a ``TIMEOUT`` error occurred. The worker task will wait the specified time without data arriving to then state a timeout and return the error response for it. The default value is 2000 - 2 seconds. Setting the interval is not possible on the ``async`` version.

**Note:** the caveat for the ModbusClientRTU timeout applies here as well. The timeout will block the worker task up to three times its value, as two retries are attempted by the worker by sending the request again and waiting for a response. 

The optional ``interval`` parameter also is given in milliseconds and specifies the time to wait for the worker between two consecutive requests to the same target host. Some servers will need some milliseconds to recover from a previous request; this interval prevents sending another request prematurely. 

**Note:** the interval is also applied for each attempt to send a request, it will add to the timeout! To give an example: ``timeout=2000`` and ``interval=200`` will result in 6600ms inactivity, if the target host notoriously does not answer.

**_AsyncTCP only_:** The timeout mechanism on the async version works slightly different. 
Every 500msec, only the oldest request is checked for a possible timeout. 
A newer request will only time out if a previous has been processed (answer is received or has hit timeout). 
For example: 3 requests are made at the same time. 
The first times out after 2000msec, the second after 2500msec and the last at 3000msec.

##### _**AsyncTCP only**_ ``void setIdleTimeout(uint32_t timeout)``
Sets the time after which the client closes the TCP connection to the server.
The async version tries to keep the connection to the server open. Upon the first request, a connection is made to the server and is kept open. 
If no data has been received from the server after the idle timeout, the client will close the connection.

Mind that the client will only reset the idle timeout timer upon data reception.

##### ``bool setTarget(IPAddress host, uint16_t port [, uint32_t timeout [, uint32_t interval]]``
_**This method is not available in the async client.**_

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

[Return to top](#modbusunified)

## ModbusServer
ModbusServer (aka slave) allows you to concentrate on the server functionality - data provision, manipulation etc. -, while the library will take care of the communication part.

You write callback functions to handle requests for accepted function codes and register those with the ModbusServer. All requests matching one of the registered callbacks will be forwarded to these callbacks. In the callback, you generate the response that will be returned to the requester.

Any request for a function code without one of your callbacks registered for it will be answered by an ``ILLEGAL_FUNCTION_CODE`` or ``INVALID_SERVER`` response automatically.

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
    return ModbusServer::ErrorResponse(ILLEGAL_DATA_ADDRESS);
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Number of words valid?
  if (!wrds || (addr + wrds) > 127) {
    // No. Return error response
    return ModbusServer::ErrorResponse(ILLEGAL_DATA_ADDRESS);
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
  return ModbusServer::DataResponse(wrds * 2 + 1, rdata);
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
  while (WiFi.status(); != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
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

**Note**: there is a special function code ``ANY_FUNCTION_CODE``, that will register your callback to any function code except those you have explicitly registered otherwise.
This is meant for applications where the standard function code and response handling does not fit the needs of the user.
There will be no automatic ``ILLEGAL_FUNCTION_CODE`` response anymore for this server!

_Hint_: You may combine this with the ``NIL_RESPONSE`` response variant described below to have a server that will gobble all incoming requests.
With RTU, this will give you a copy of all messages directed to another server on the bus, but without interfering with it.

A ``MBSworker`` callback must return a data object of ``ResponseType``. This is in fact a ``std::vector<uint8_t>`` and may be used likewise. There are basically five different ``ResponseType`` return variants:
- ``NIL_RESPONSE``: no response at all will be sent back to the requester
- ``ECHO_RESPONSE``: the request will be sent back without any modification as a response. This is common for the writing function code requests the Modbus standard defines.
- ``ErrorResponse(Error errorCode)``: the error code should be one of the defined error codes in the lib (see [Error codes](#errorcodes)). It will be completed to a standard Modbus error response.
- ``DataResponse(uint16_t dataLen, uint8_t *data)``: this is the regular response for data requests etc. You will have to fill the ``data`` buffer with the response data starting at the first byte after the function code. Please note that the data has to be in MSB-first format. The ``addValue()`` service method will help you putting your data into that required order.
- free form: you may of course return any ``std::vector<uint8_t>`` you like as well. It will be sent as is, no addition of serverID or function code etc. will be done by the lib in this case.

  **Note**: do not use the first byte as ``0xFF``, followed by one of ``0xF0``, ``0xF1``, ``0xF2`` or ``0xF3``, as these are used internally for NIL_RESPONSE, ECHO_RESPONSE, ErrorResponse and DataResponse, respectively!

##### ``uint16_t getValue(uint8_t *source, uint16_t sourceLength, T &v)``
As a complement to the ``addValue()`` service function described in the ModbusClient section, ``getValue()`` will help you reading MSB-first data from a request data buffer.
This buffer is given as ``uint8_t *source``, its length as ``uint16_t sourceLength``.
Depending on the data type ``T`` of the variable given as reference ``&v`` to the ``getValue()`` call, the right number of bytes is taken from ``source``, converted into type ``T`` and stored in the variable ``v``.
The return value of ``getValue()`` is the number of bytes consumed from ``source`` to enable you updating the ``source`` pointer for the next call. **Note**: please take care to reduce the ``sourceLength`` parameter accordingly in subsequent calls to ``getValue()``!

##### ``MBSworker getWorker(uint8_t serverID, uint8_t functionCode)``
You may check if a given serverID/functionCode combination has been covered by a callback with ``getWorker()``. This method will return the callback function pointer, if there is any, and a ``nullptr`` else.

##### ``bool isServerFor(uint8_t serverID)``
``isServerFor()`` will return ``true``, if at least one callback has been registered for the given ``serverID``, and ``false`` else.

##### ``uint32_t getMessageCount()``
Each request received will be counted. The ``getMessageCount()`` method will return the current state of the counter.

##### ``ResponseType localRequest(uint8_t serverID, uint8_t functionCode, uint16_t dataLen, uint8_t* data)``
This function is a simple local interface to issue requests to the server running. Responses are returned immediately - there is no request queueing involved. This call is *blocking* for that reason, so be prepared to have to wait until the response is ready!

A bridge (see below) will respond to this call for all known serverID/function code combinations, so the delegated request to a remote server may be involved as well.

##### ``void listServer()``
Mostly intended to be used in debug situations, ``listServer()`` will output all servers and their function codes served by the ModbusServer to the Serial Monitor.

#### ModbusServerTCP
Inconsistencies between the WiFi and Ethernet implementations in the core libraries required a split of ModbusServerTCP into the ``ModbusServerEthernet`` and ``ModbusServerWiFi`` variants.

Both are sharing the majority of methods etc., but need to be included with different file names and have different object names as well.

All relevant calls after initialization of the respective ModbusServer object are identical.

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

#### ModbusServerTCPasync
To use the WiFi version, you will have to use
```
#include "ModbusServerTCPasync.h"
...
ModbusServerTCPasync myServer;
...
```
in your source file. You will have to add ``#include <WiFi.h>``, yourself and have `AsyncTCP.h` as dependency installed on your system.

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

The asynchronous version does not rely on client tasks but on events from a single async TCP task which are forwarded to this library.

##### ``bool stop()``
The ``stop()`` call will close all open connections, free allocated memory and finally stop the server task. At this time your Modbus server will not respond to any connection request any more. 

It can be restarted, though, with another ``start()`` call, even with different ``port``, ``maxClient`` or ``timeout`` setting.

#### ModbusServerRTU

There are a few differences to the TCP-based ModbusServers: 
- ModbusServerRTU only has a single background task to listen for responses on the Modbus.
- as timing is critical on the Modbus, the background task has a higher priority than others
- a request received for another server ID not covered by this server will be ignored (rather than answered with an ``ILLEGAL_SERVER_ID`` error as with TCP)

##### ``ModbusServerRTU(HardwareSerial& serial, uint32_t timeout)`` and ``ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, int rtsPin)``
The first parameter, ``serial`` is mandatory, as it gives the serial interface used to connect to the RTU Modbus to the server.
``timeout`` is less important as it is for TCP. It defines after what time of inactivity the server should loop around and re-initialize some working data.
A value of ``20000`` (20 seconds) is reasonable.
The third (optional) parameter ``rtsPin`` is the same as for the RTU Modbus client described above - if you are using a RS485 adaptor requiring a DE/RE line to be maintained, ``rtsPin`` should be the GPIO number of the wire to that DE/RE line. The linbrary will take care of toggling the pin.

  // start: create task with RTU server to accept requests
##### ``bool start()`` and ``bool start(int coreID)``
With ``start()`` the server will create its background task and start listening to the Modbus. 
The optional parameter ``coreID`` may be used to have that background task run on the named core for multi-core MCUs.

##### ``bool stop()``
The server background process can be stopped and the task be deleted with the ``stop()`` call. You may start it again with another ``start()`` afterwards.
In fact a ``start()`` to an already running server will stop and then restart it.

[Return to top](#modbusunified)

## ModbusBridge
A Modbus "bridge" (or gateway) is basically a Modbus server forwarding requests to other servers, that are in a different Modbus. 
The bridge will control and translate requests from a TCP Modbus into a RTU Modbus and vice versa. 

Some typical applications:
- TCP bridge gives access to all servers from a connected RTU Modbus
- RTU bridge brings a TCP server into a RS485 Modbus
- bridge collects data from several (TCP and RTU) Modbuses under a single interface

As the bridge is based on a ``ModbusServer``, it also can serve locally in addition to the external servers connected.

### Basic Use
First you will have to include the matching header file for the type of bridge you are going to set up (talking to Ethernet in this case):
```
#include "ModbusBridgeEthernet.h"
```
Next the bridge needs to be defined:
```
ModbusBridgeEthernet MBbridge(5000);
```
We will need at least one ``ModbusClient`` for the bridge to contact external servers:
```
#include "ModbusClientRTU.h"
```
... and set that up:
```
ModbusClientRTU MB(Serial1);
```
Then, most probably in ``setup()``, the client is started and the bridge needs to be configured:
```
MB.setTimeout(2000);
MB.begin();
...
// ServerID 4: RTU Server with remote serverID 1, accessed through RTU client MB - FC 03 accepted only
MBbridge.attachServer(4, 1, READ_HOLD_REGISTER, &MB);
// Add FC 04 to it
MBbridge.addFunctionCode(4, READ_INPUT_REGISTER);

// ServerID 5: RTU Server with remote serverID 4, accessed through RTU client MB - all FCs accepted
MBbridge.attachServer(5, 4, ANY_FUNCTION_CODE, &MB);
// Remove FC 04 from it
MBbridge.denyFunctionCode(5, READ_INPUT_REGISTER);
```
And finally the bridge is started:
```
MBbridge.start(port, 4, 600);
```
That's it!

From now on all requests for serverID 4, function codes 3 and 4 are forwarded to the RTU-based server 1, its responses are given back. Same applies to server ID 5 and all possible function codes - with the exception of FC 4.

All else requests will be answered by the bridge with either ILLEGAL_FUNCTION or INVALID_SERVER.

### API description
**Note**: all API calls for ``ModbusServer`` are valid for the bridge as well (except the constructors of course) - see the [ModbusServer API description](#api-description-1) for these.

You will have to include the matching header file in your code for the bridge type you want to use:
- ``ModbusBridgeEthernet.h`` for the Ethernet-based bridge
- ``ModbusBridgeWiFi.h`` for the WiFi sibling
- ``ModbusBridgeRTU`` for a bridge listening on a RTU Modbus

#### ``ModbusBridge()`` and ``ModbusBridge(uint32_t TOV)``
These are the constructors for a TCP-based bridge. The optional ``TOV`` (="timeout value") parameter sets the maximum time the bridge will wait for the responses from external servers. The default value for this is 10000 - 10 seconds.

**Note**: this is a value common for all servers connected, so choose the value sensibly to cover all normal response times of the servers. 
Else you will be losing responses from those servers taking longer to respond. Their responses will be dropped unread if arriving after the bridge's timeout had struck.

#### ``ModbusBridge(HardwareSerial& serial, uint32_t timeout)``, ``ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV)`` and ``ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV, int rtsPin)``
The corresponding constructors for the RTU bridge variant. With the exception of ``TOV`` all parameters are those of the underlying RTU server.
The duplicity of ``timeout`` and ``TOV`` may look irritating at the first moment, but while ``timeout`` denotes the timeout the bridge will use when waiting for messages as a _server_, the ``TOV`` value is the _bridge_ timeout as described above.

#### ``bool attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client)`` and ``bool attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client, IPAddress host, uint16_t port)``
These calls are used to bind ``ModbusClient``s to the bridge. 
The former call is for RTU clients, the latter for their TCP pendants.
The call parameters are:
- ``aliasID``: the server ID, that will be visible externally for the attached server. As connected servers in different Modbus environments may have the same server IDs, these need to be unified for requests to the bridge.
The ``aliasID`` must be unique for the bridge. Attempts to attach the same ``aliasID`` to another server/client will be denied by the bridge.
The call will return ``false`` in these cases.
- ``serverID``: the server ID of the remote server. This is the ID the server natively is using on the Modbus the client is connected to.
- ``functionCode``: the function code that shall be accessed on the remote server.
This may be any FC allowed by the Modbus standard (see [Function Codes](#functioncodes) above), including the special ``ALL_FUNCTION_CODES`` non-standard value. The latter will open the bridge for any function code sent without further checking. See [Filtering](#filtering) for some refined recipes.
- ``*client``: this must be a pointer to any ``ModbusClient`` type, that is connecting to the external Modbus the remote server is living in.

The TCP ``attachServer`` call has two more parameters needed to address the TCP host where the server is located:
- ``host``: the IP address of the target host
- ``port``: the port number to connect to the TCP Modbus on that host.

At least one ``attachServer()`` call has to be made before any of the following make any sense!

#### ``bool addFunctionCode(uint8_t aliasID, uint8_t functionCode)``
With ``addFunctionCode()`` you may add another function code for the server known behind the ``aliasID``, that will be forwarded by the bridge. One by one you can add those function codes you would like to have served by the bridge.
The ``aliasID`` has to be attached already before you may use this call successfully. If it is not, the call will return ``false``.

#### ``bool denyFunctionCode(uint8_t aliasID, uint8_t functionCode)``
This call is the opposite to ``addFunctionCode()`` - the function code you give here as a parameter will **not** be served by the bridge, but responded to with an ILLEGAL_FUNCTION error response.
Like its sibling, this call will return ``false`` if the ``aliasID`` was not yet ``attached`` to the bridge.

### Filtering
Given the means of both the underlying ``ModbusServer`` and the ``ModbusBridge`` itself, the access to the remote servers can be controlled in multiple ways.

As a principle, the bridge will only react on server IDs made known to it - either by the ``attachServer()`` bridge call or by the server's ``registerWorker()`` call for a locally served function code. So a server ID filter is intrinsically always active. Server IDs not known will not be served.

A finer level of control is possible on the function code level. We have two different ways to restrict function code uses:
- allow most, deny some
- deny most, allow some

You will want to choose the method that will require the least effort for your application.

#### Allow most, deny some
This is achieved by first attaching a server for the special ``ANY_FUNCTION_CODE`` value. It will open the server to all possible function codes regardless.

To now restrict the use of a few function codes, you will call ``denyFunctionCode()`` for each of these. Following that the bridge will return the ``ILLEGAL_FUNCTION`` error response if a request for that code is sent to the bridge.

#### Deny most, allow some
You have to refrain from using the ``ANY_FUNCTION_CODE`` value here and only use those function codes in ``attachServer()`` and ``addFunctionCode()`` that you will allow to be served.

#### More subtle methods
There is one basic server rule, that also applies to the bridge: "specialized beats generic". That means, if the server/bridge has a detailed order for a server ID/function code combination, it will use that, even if there is another for the same server ID and ANY_FUNCTION_CODE.
Second rule is "last register counts": whatever was known to the server/bridge on how to serve a server ID/function code combination, will be replaced by a later ``registerWorker()`` call for the same combination without a trace.

You can make use of that in interesting ways:
- add a local function to serve a certain server ID/function code combination that normally would be served on a remote server. Thus you can mask the external server for that function code at will.
- add your own local function to respond with a different error than the default ILLEGAL_FUNCTION - or not at all.
- after attaching some explicit function codes for an external server add a local worker for that server ID and ``ALL_FUNCTION_CODES`` to cover all other codes not explicitly named with a default response.

So besides plain filtering you may modify parts or all of the responses the external server would give.

**Warning**: registering local worker functions for the serverID of a remote server may completely block the external server! There is no way to go back once you did that!

[Return to top](#modbusunified)

## Logging
The library brings its own logging and tracing mechanism, encoded in the ``Logging.h`` header file.
It allows for global and file-local verbosity settings.
Log statements above the defined log level will not be compiled in the binary file. 

### Installation
- Put ``#include "Logging.h"`` in your source file.
- The compile-time definition ``-DLOG_LEVEL=...`` as a compiler flag or with ``#define LOG_LEVEL LOG_LEVEL_...`` before including ``Logging.h`` will set the global log level for the compiler. 
All log statements with log levels higher than that of ``LOG_LEVEL`` will not be included in the binary.
- In the line before the ``#include "Logging.h"``, either write ``#undef LOCAL_LOG_LEVEL`` to accept the global ``LOG_LEVEL`` for the current file, or ``#define LOCAL_LOG_LEVEL LOG_LEVEL_XXX`` - with ``XXX`` as whatever level you like - to set a different level for the current file.

### Runtime log level 
- the ``MBUlogLvl`` ``extern`` variable may be used to set a different log level at runtime. It will be set to ``LOG_LEVEL`` initially. 
**NOTE**: setting the runtime log level above that of ``LOG_LEVEL`` will have only an effect for those source files where there is a different ``LOCAL_LOG_LEVEL`` defined, that is above ``LOG_LEVEL``!

### Output channel
All output will be sent to the output defined in ``LOGDEVICE``. The default is ``Serial``, but any ``Print``-derived object will do.

### Log levels
Every log level includes all lower level output as well!
- ``LOG_LEVEL_NONE`` (0), denoted by ``N``, is completely unrestricted, the respective statements will be executed regardless of ``LOG_LEVEL`` set.
- ``LOG_LEVEL_CRITICAL`` (1), ``C``, is meant for the most important messages in cases of an instable system etc. 
- ``LOG_LEVEL_ERROR`` (2), ``E``, should be used for error messages. This is the default ``LOG_LEVEL`` value, if none was set at compile time.
- ``LOG_LEVEL_WARNING`` (3), ``W``, is for less relevant problems.
- ``LOG_LEVEL_INFO`` (4), ``I``, for informative output.
- ``LOG_LEVEL_DEBUG`` (5), ``D``, intended to print out detailed information to help debugging.
- ``LOG_LEVEL_VERBOSE`` (6), ``V`` - output everything and all minor details.

### Logging statements
In all statements, ``X`` has to be one of the log level letters as specified above.
- ``LOG_X(format, ...);`` is a printf-style statement that will print a standard line header like 
```
 +----------------------------------------------- log level
 |     +----------------------------------------- millis()
 |     |    +------------------------------------ source file name
 |     |    |                       +------------ line number
 |     |    |                       |    +------- function name
 |     |    |                       |    |    +-- user output
 v     v    v                       v    v    v 
[N] 101973| main.cpp             [ 258] loop: 05/03 @8/13? (heap=343524)
```
  followed by the specified formatted data.
- ``LOGRAW_X(format, ...);`` does the same, but without the line header. This is handy to collect several log output on one line
- ``HEXDUMP_X(label, address, length)`` will print out a hexadecimal and ASCII dump of ``length`` bytes, starting at ``address``:
  Dump header:
```
 +-------------------------------- log level
 |  +----------------------------- user-defined label
 |  |                 +----------- starting address
 |  |                 |        +-- number of bytes
 v  v                 v        v
[N] Bridge response: @3FFB49FC/29:
```
  Dump body:
```
    +----------------------------------------------------------- offset (hex)
    |     +----------------------------------------------------- hexadecimal dump
    |     |                                                  +-- ASCII dump
    v     v                                                  v
  | 0000: 05 03 0D 49 38 52 72 49  32 30 8A 00 00 00 00 00  |...I8RrI20......|
  | 0010: 00 00 00 45 4D 48 00 09  01 45 4D 48 00           |...EMH...EMH.   |
```

[Return to top](#modbusunified)