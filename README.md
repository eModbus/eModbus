# eModbus

![eModbus](https://github.com/eModbus/eModbus/workflows/Building/badge.svg)

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
- [ModbusMessage](#modbusmessage)
  - [Constructor calls](#constructor-calls)
  - [Filling a message](#filling-a-message)
  - [Reading data from a message](#reading-data-from-a-message)
  - [Miscellaneous functions](#miscellaneous-functions)
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
  - ``std::placeholder``

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
  ModbusClient=https://github.com/eModbus/eModbus.git
```

If you are using the Arduino IDE, you will want to copy the library folder into your library directory. In Windows this will be  ``C:\Users\<user_name>\Documents\Arduino\libraries`` normally. For Arduino IDE, you'll have to install the dependencies manually. These are:
- AsyncTCP (https://github.com/me-no-dev/AsyncTCP)
- Ethernet-compatible library (example: https://github.com/maxgerhardt/Ethernet)

[Return to top](#eModbus)

## ModbusMessage
Basically all data going back and forth inside the library has the form of a ``ModbusMessage`` object.
This object will contain all the bytes of a Modbus standard message - request or response - but without everything that is depending on the interface used (RTU or TCP).
A ``ModbusMessage`` internally is a ``std::vector<uint8_t>``, and in multiple aspects can be used as one.

### Constructor calls
``ModbusMessage`` is created with one of several constructors. 
The very first is the basic one:

#### ``ModbusMessage()`` or ``ModbusMessage(uint16_t dataLen)``
These will create an empty ``ModbusMessage`` instance. 
The second form is taking the ``dataLen`` parameter to allocate the given number of bytes.
This can speed up the use of the instance, since no re-allocations are done internally as long as the pre-allocated memory is sufficient.

All further constructors will fill the ``ModbusMessage`` with a defined Modbus message:

#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode)``
This is usable to set up Modbus standard messages for the Modbus function codes requiring no additional parameter, as 0x07, 0x0B, 0x0C and 0x11.

If you will specify a serverID or function code outside the Modbus specifications, an error message is sent to ``Serial`` and the message is not generated.

This applies to all constructors in this group.
Parameters to known standard Modbus messages are checked for conformity.

As a ``functionCode`` you may specify a numeric value or one of the predefined constant names:

```
enum FunctionCode : uint8_t {
  ANY_FUNCTION_CODE       = 0x00,  // only valid to be used by ModbusServer/ModbusBridge!
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

#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1)``
There is one standard Modbus message requiring exactly one ``uint16_t`` parameter: 0x18, READ_FIFO_QUEUE.
This constructor will help set it up correctly, but of course may be used for any other proprietary function code with the same signature.
Please note that in this case the ``p1`` parameter will not be range-checked.

#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2)``
This constructor will be one of the most used throughout, as the most relevant Modbus messages have this signature (taking two 16bit parameters).
These are the FCs 0x01, 0x02, 0x03, 0x04, 0x05 and 0x06.
  
#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3)``
Modbus standard function code 0x16 (MASK_WRITE_REGISTER) does take three 16bit arguments and will fit into this constructor.
  
#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords)``
Another frequently used Modbus function code is WRITE_MULT_REGISTERS, 0x10. 
It requires a 16bit address, a numer of words to write (16bit again), a length byte and a uint8_t* pointer to an array of words to be written.
That is the purpose of this constructor here.
  
#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes)``
A very similar constructor, only that it takes an array of bytes instead of 16bit words. Modbus standard FC 0x0F is using this.

#### ``ModbusMessage(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes)``
This is a 'generic' constructor to pre-fill a freshly created ``ModbusMessage`` with arbitrary data.
The byte array of length ``count`` is taken as is and put into the message behind server ID and function code. 
Please note that there is no additional check on validity - you will have to maintain the correctness of the data!

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

### Filling a message
If you are not able to use one of the constructors as are described above, you can use a number of calls to add or modify the contents of a ``ModbusMessage``

First, there are ``setMessage()`` calls that have the same functionality as the constructors, but with one exception:
these calls will return an ``Error`` value describing what may have happened when creating the message.
``Error`` is one of the error codes <a name="errorcodes"></a>defined in ``ModbusTypeDefs.h``:
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
  If parameter checks will find a deviation from the standard, the returned ``Error`` will tell you what it was.

For completeness, here are the ``setMessage()`` variants:

1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode);``

2. one uint16_t parameter (FC 0x18)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1);``

3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);``

4. three uint16_t parameters (FC 0x16)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);``

5. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of words (FC 0x10)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);``

6. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of bytes (FC 0x0f)<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);``

7. generic method for preformatted data ==> count is counting bytes!<br>
``Error setMessage(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);``

There is in fact an eighth function, that is not available as a constructor.
It will set the ``ModbusMessage`` to be an error response, signalling the given ``Error``:

8. Error response method<br>
``Error setError(uint8_t serverID, uint8_t functionCode, Error errorCode);``

**Note**: a call to one of the ``set...()`` methods will erase the previous contents of the ``ModbusMessage`` before filling it!

### Adding data to a message
If a message has been set up - for instance with server ID and function code only, more data can be added to it.

#### ``uint16_t add(T value);``
``add()`` takes the integral ``value`` of type ``T`` and appends it MSB-first to the existing message.
``uint8_t b = 9; msg.add(b);`` for instance will append one single byte ``09`` to the message, whereas ``uint32_t i = 122316; msg.add(i);`` will append four bytes: ``00 01 DD CC`` (the hexadecimal representation of 122316, MSB-first!).

The ``add()`` method will return the number of bytes in the message after the addition.

**Warning**: expressions like ``a ? 1 : 2`` are converted to ``int`` by the compiler. So please be sure to cast expressions if you would like to have a shorter representation.
``msg.add(a ? 1 : 2);`` most likely will add ``00 00 00 01`` or ``00 00 00 02`` to your message, while ``msg.add((uint8_t)(a ? 1 : 2));`` will append ``01`` or ``02`` - which may be what you wanted instead.

#### ``uint16_t add(T v, ...);``
``add()`` may also be used with more than one integral value and will add the given values one by one. 
The returned size is that after adding all of the values in a row.

#### ``uint16_t add(uint8_t *data, uint16_t dataLen);``
This version of ``add()`` will append the given buffer of ``dataLen`` bytes pointed to by ``data``.
It also does return the final size of the message.

#### ``void append(ModbusMessage& m);`` and<br> ``void append(std::vector<uint8_t>& m):``
With ``append()`` you may add in another ``ModbusMessage``'s content or that of a ``std::vector`` of bytes.

#### ``void push_back(const uint8_t& value);``
Like the ``std::vector`` method of the same name, ``push_back()`` will append the given byte to the end of the existing message.

### Reading data from a message
There are a few methods to get values out of a ``ModbusMessage``, to read Modbus standard values as well as free-form data.

#### ``uint8_t getServerID();``
If the message has at least 2 bytes (a Modbus message needs to have a server ID and function code as a minimum), this method will return the value of the very first, the server ID.
If the message is shorter, you will be returned a ``0`` value.

#### ``uint8_t getFunctionCode();``
This method will return the second byte of a Modbus message - the function code, according to the standard.
This method will return a ``0`` as well, if the function code could not be read.

#### ``Error getError();``
``getError()`` will return the error code in a Modbus error response message. If the message is no error response, you will get a ``SUCCESS`` code instead.

#### The ``[]`` operator
A.You can use the well-known bracket operator with a ``ModbusMessage`` to get the *n*th byte of a message: ``uint8_t byte = msg[7];``
If the message is shorter than the requested byte number, you will get a ``0`` instead.
Opposite to its ``std::vector`` sibling, the ``[]`` operator does *not* extend the message length!

#### All contents by data/size
Again like the ``std::vector``, a ``ModbusMessage`` provides the ``uint8_t *data()`` and ``uint16_t size()`` methods to get a ``const`` access to the internal data buffer.

#### Iterators
``ModbusMessage`` has both the ``begin()`` and ``end()`` iterator functions to allow iterator looping and range ``for`` loops:
```
for (auto& byte : msg) {
  Serial.printf("%02X ", byte);
}
Serial.println();
```

#### ``uint16_t get(uint16_t index, T& value);``
As the ``add()`` function is able to write integral data values into a message, ``get()`` is used to read values back.
The ``index`` parameter gives the starting position for the extraction of a ``value`` of the integral type ``T``.
The method returns the index value after the extraction has taken place.
Example:
```
// Get address and word count for a READ_INPUT_REGISTER request message
uint16_t addr, words;
msg.get(2, addr);
msg.get(4, words);
```

### Miscellaneous functions
There are a few additional methods not fitting in the previous categories. 

#### ``void setServerID(uint8_t serverID);`` and<br> ``void setFunctionCode(uint8_t functionCode);``
With these two functions you can change the two Modbus standard values in-place, without changing the message length or other contents.
**Note**: if the message is shorter than 2 bytes, the two methods will do nothing.

#### ``uint16_t resize(uint16_t newSize);``
``resize`` will change the ``size()`` of the underlying ``std::vector`` to the given number of bytes. 

#### ``void clear();``
To completely erase the message content, use the ``clear()`` call. The message will be empty afterwards.

#### Assignment
``ModbusMessage``s may be assigned safely to another. The complete content will be copied into the recieving ``ModbusMessage``.
As a ``ModbusMessage`` also implements the ``std::move`` pattern, a returned ``ModbusMessage`` may be moved instead of copied, if the compiler decides it.

#### Comparison
``ModbusMessage``s may be compared for equality (``==``) and inequality (``!=``).
If used in a ``bool`` context, a ``ModbusMessage`` will be evaluated to ``true``, if it has 2 or more bytes as content.

[Return to top](#eModbus)

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

Next we will define a callback function for data responses coming in. This example will print out a hexadecimal dump of the response data, using the range iterator:

```
void handleData(ModbusMessage msg, uint32_t token) 
{
  Serial.printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", msg.getServerID(), msg.getFunctionCode(), token, msg.size());
  for (auto& byte : msg) {
    Serial.printf("%02X ", byte);
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
  Serial.printf("Error response: %02X - %s\n", error, (const char *)me);
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
Error err = RS485.addRequest(0x12345678, 1, READ_HOLD_REGISTER, 10, 1);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", err, (const char *)e);
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
void func(ModbusMessage msg, uint32_t token);
```

- ``msg``: the response message received
- ``token``: this is a general concept throughout the library, see below.

**Note**: a handler can only be claimed once. If that already has been done before, subsequent calls are returning a ``false`` value.

#### The ``token`` concept
Each request must be given a user-defined ``token`` value. This token is saved with each request and is returned in the callback. No processing whatsoever is done on the token, you will get what you gave. This enables a user to keep track of the sent requests when receiving a response.

Imagine an application that has several ModbusClients working; some for dedicated TCP Modbus servers, another for a RS485 RTU Modbus and another again to request different TCP servers.

If you only want to have a single onData and onError function handling all responses regardless of the server that sent them, you will need a means to tell one response from the other. 

Your token you gave at request time will tell you that, as the response will return exactly that token.

##### `bool onErrorHandler(MBOnError handler)`
Very similar to the ``onDataHandler`` call, this allows to catch all error responses with a callback function.

**Note:** it is not strictly *required* to have an error callback registered. But if there is none, errors will go unnoticed!

The ``onErrorhandler`` call accepts a function pointer with the following signature only:

```
void func(ModbusClient::Error, uint32_t token);
```

The parameters are 
- the ``token`` value as described in the ``onDataHandler`` section above.

The Library is providing a separate wrapper class ``ModbusError`` that can be assigned or initialized with any ``ModbusClient::Error`` code and will produce a human-readable error text message if used in ``const char *`` context. See above in [**Basic use**](#basic-use) an example of how to apply it.
  
##### ``uint32_t getMessageCount()``
Each request that got successfully enqueued is counted. By calling ``getMessageCount()`` you will be able to read the number accumulated so far.

Please note that this count is instance-specific, so any ModbusClient instance you created will have its own count.

##### ``void begin()`` and<br> ``void begin(int coreID)``
This is the most important call to get a ModbusClient instance to work. It will open the request queue and start the background worker task to process the queued requests.

The second form of ``begin()`` allows you to choose a CPU core for the worker task to run (only on multi-core systems like the ESP32). This is advisable in particular for the ``ModbusClientRTU`` client, as the handling of the RS485 Modbus is a time-critical and will profit from having its own core to run.

**Note:** The worker task is running forever or until the ModbusClient instance is killed that started it. The destructor will take care of all requests still on the queue and remove those, then will stop the running worker task.

#### Setting up requests
All interfaces provide a common set of ``addRequest()`` methods to set up and enqueue a request.
There are seven ``addRequest()`` variants that closely resemble the ``ModbusMessage.setMessage()`` calls described in the ``ModbusMessage`` chapter.
Internally exactly those ``setMessage()`` calls are used by these ``addRequest()`` functions.
The major difference to ``setMessage()`` is the additional ``Token`` parameter, given as first argument in front of the message parameters:

So we have:
1. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode);``

2. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t p1);``
  
3. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2);``
  
4. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3);``
  
5. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords);``
  
6. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes);``

7. ``Error addRequest(uint32_t token, uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes);``

Again, there is an eighth ``addRequest()``, this time taking a pre-formatted ``ModbusMessage``:

8. ``Error addRequest(ModbusMessage request, uint32_t token);``
Note the switched position of the ``token`` parameter - necessary for the internal handling of the ``addRequest()`` methods.

#### ModbusClientRTU API elements
You will have to have the following include line in your code to make the ``ModbusClientRTU`` API available:

```
#include "ModbusClientRTU.h"
```

##### ``ModbusClientRTU(HardwareSerial& serial)``,<br> ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin)`` and<br> ``ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit)``
These are the constructor variants for an instance of the ``ModbusClientRTU`` type. The parameters are:
- ``serial``: a reference to a Serial interface the Modbus is conncted to (mostly by a RS485 adaptor). This Serial interface must be configured to match the baud rate, data and stop bits and parity of the Modbus.
- ``rtsPin``: some RS485 adaptors have "DE/RE" lines to control the half duplex communication. When writing to the bus, the lines have to be set accordingly. DE and RE usually have opposite logic levels, so that they can be connected to a single GPIO that is set to HIGH for writing and LOW for reading. This will be done by the library, if a GPIO pin number is given for ``rtsPin``. Leave this parameter out or use ``-1`` as value if you do not need this GPIO (usually with RS485 adaptors doing auto half duplex themselves).
- ``queueLimit``: this specifies the number of requests that may be placed on the worker task's queue. If the queue has reached this limit, the next ``addRequest`` call will return a ``REQUEST_QUEUE_FULL`` error. The default value built in is 100. **Note:** while the queue holds pointers to the requests only, the requests need memory as well. If you choose a ``queueLimit`` too large, you may encounter "out of memory" conditions!

##### ``void setTimeout(uint32_t TOV)``
This call lets you define the time for stating a response timeout. ``TOV`` is defined in **milliseconds**. When the worker task is waiting for a response from a server, and the specified number of milliseconds has passed without data arriving, it will return a ``TIMEOUT`` error response.

**Note:** This timeout is blocking the worker task. No other requests will be made while waiting for a response. Furthermore, the worker will retry the failing request two more times. So the ``TOV`` value in effect can block the worker up to three times the time you specified, if a server is dead. 
Too short timeout values on the other hand may miss slow servers' responses, so choose your value with care...

##### ``uint16_t RTUutils::calcCRC16(uint8_t *data, uint16_t len)`` and<br> ``uint16_t RTUutils::calcCRC16(ModbusMessage msg)``
This is a convenient method to calculate a CRC16 value for a given block of bytes. ``*data`` points to this block, ``len`` gives the number of bytes to consider.
The call will return the 16-bit CRC16 value.

##### ``bool RTUutils::validCRC(const uint8_t *data, uint16_t len)`` and<br> ``bool RTUutils::validCRC(ModbusMessage msg)``
If you got a buffer holding a Modbus message including a CRC, ``validCRC()`` will tell you if it is correct - by calculating the CRC16 again and comparing it to the last two bytes of the message buffer.

##### ``bool RTUutils::validCRC(const uint8_t *data, uint16_t len, uint16_t CRC)`` and<br> ``bool RTUutils::validCRC(ModbusMessage msg, uint16_t CRC)``
``validCRC()`` has a second variant, where you can provide a pre-calculated (or otherwise obtained) CRC16 to be compared to that of the given message buffer.

##### ``void RTUutils::addCRC(ModbusMessage& raw)``
If you have a ``ModbusMessage`` (see above), you can calculate a valid CRC16 and have it pushed o its end with ``addCRC()``.

#### ModbusClientTCP API elements
You will have to include one of these lines in your code to make the ``ModbusClientTCP`` API available:

```
#include "ModbusClientTCP.h"
#include "ModbusclientTCPasync.h"
```

##### ``ModbusClientTCP(Client& client)`` and<br> ``ModbusClientTCP(Client& client, uint16_t queueLimit)``
The first set of constructors does take a ``client`` reference parameter, that may be any interface instance supporting the methods defined in ``Client.h``, f.i. an ``EthernetClient`` or a ``WiFiClient`` instance.
This interface will be used to send the Modbus TCP requests and receive the respective TCP responses.

The optional ``queueLimit`` parameter lets you define the maximum number of requests the worker task's queue will accept. The default is 100; please see the remarks to this parameter in the ModbusClientRTU section.

##### ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port)`` and<br> ``ModbusClientTCP(Client& client, IPAddress host, uint16_t port, uint16_t queueLimit)``
Alternatively you may give the initial target host IP address and port number to be used for communications. This can be sensible if you have to set up a ModbusClientTCP client dedicated to one single target host.

##### _**AsyncTCP only**_ ``ModbusClientTCPasync(IPAddress host, uint16_t port)`` and<br> ``ModbusClientTCPasync(IPAddress host, uint16_t port, uint16_t queueLimit)``
The asynchronous version takes 3 arguments: the target host IP address and port number of the modbus server and an optionally queue size limit (defaults to 100). The async version connects to one modbus server.

##### ``void setTimeout(uint32_t timeout)`` and<br> ``void setTimeout(uint32_t timeout, uint32_t interval)`` 
Similar to the ModbusClientRTU timeout, you may specify a time in milliseconds that will determine if a ``TIMEOUT`` error occurred. The worker task will wait the specified time without data arriving to then state a timeout and return the error response for it. The default value is 2000 - 2 seconds. Setting the interval is not possible on the ``async`` version.

**Note:** the caveat for the ModbusClientRTU timeout applies here as well. The timeout will block the worker task up to three times its value, as two retries are attempted by the worker by sending the request again and waiting for a response. 

The optional ``interval`` parameter also is given in milliseconds and specifies the time to wait for the worker between two consecutive requests to the same target host. Some servers will need some milliseconds to recover from a previous request; this interval prevents sending another request prematurely. 

**Note:** the interval is also applied for each attempt to send a request, it will add to the timeout! To give an example: ``timeout=2000`` and ``interval=200`` will result in 6600ms inactivity, if the target host notoriously does not answer.

**_AsyncTCP only_:** The timeout mechanism on the async version works slightly different. 
Every 500msec, only the oldest request is checked for a possible timeout. 
A newer request will only time out if a previous has been processed (answer is received or has hit timeout). 
For example: 3 requests are made at the same time. 
The first times out after 2000msec, the second after 2500msec and the last at 3000msec.

##### ``bool setTarget(IPAddress host, uint16_t port [, uint32_t timeout [, uint32_t interval]]``
_**This method is not available in the async client.**_

This function is necessary at least once to set the target host IP address and port number (unless that has been done with the constructor already). All requests will be directed to that host/port, until another ``setTarget()`` call is issued.

**Note**: without a ``setTarget()`` or using a client constructor with a target host no ``addRequest()`` will make any sense for TCP!

The optional ``timeout`` and ``interval`` parameters will let you override the standards set with the ``setTimeout()`` method **for just those requests sent from now on to the targeted host/port**. The next ``setTarget()`` will return to the standard values, if not specified differently again.

##### _**AsyncTCP only**_ ``void connect()`` and ``void disconnect()``
The library connects automatically upon making the first request. However, you can also connect manually.
When making requests, the requests are put in a queue and the queue is processed once connected. There is however a delay of 500msec between the moment the connection is established and the first request is sent to the server. All the following requests are send immediately.

You can avoid the 500msec delay by connecting manually.

Disconnecting is also automatic (see ``void setIdleTimeout(uint32_t timeout)``). Likewise, you can disconnect manually.

##### _**AsyncTCP only**_ ``void setIdleTimeout(uint32_t timeout)``
Sets the time after which the client closes the TCP connection to the server.
The async version tries to keep the connection to the server open. Upon the first request, a connection is made to the server and is kept open. 
If no data has been received from the server after the idle timeout, the client will close the connection.

Mind that the client will only reset the idle timeout timer upon data reception and not when sending.

##### _**AsyncTCP only**_ ``void setMaxInflightRequests(uint32_t maxInflightRequests)``
Sets the maximum number of messages that are sent to the server at once. The async modbus client sends all the requests to the server without waiting for an earlier request to receive a response. This can result in message loss because the server could has a limited queue. Setting this number to 1 mimics a sync client.

[Return to top](#eModbus)

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
ModbusMessage FC03(ModbusMessage request) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read
  ModbusMessage response;

  // Get addr and words from data array. Values are MSB-first, get() will convert to binary
  request.get(2, addr);
  request.get(4, wrds);
  
  // address valid?
  if (!addr || addr > 128) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Modbus address is 1..n, memory address 0..n-1
  addr--;

  // Number of words valid?
  if (!wrds || (addr + wrds) > 127) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Prepare response
  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(wrds * 2));

  // Loop over all words to be sent
  for (uint16_t i = 0; i < wrds; i++) {
    // Add word MSB-first to response buffer
    response.add(memo[addr + i]);
  }

  // Return the data response
  return response;
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
``ModbusMessage (*MBSworker) (ModbusMessage request)``,
where the parameter given to the callback is 
- ``request``: the ModbusMessage containing the request

**Note**: you may specify different server identifications for registered callbacks. This way the server you are providing can cover more than one Modbus server at a time. You will have to check the requested serverID in the callback to learn which of the registered servers was meant in the request.

There is no limit in registered callbacks, but a second register for a certain serverID/functionCode combination will overwrite the first without warning.

**Note**: there is a special function code ``ANY_FUNCTION_CODE``, that will register your callback to any function code except those you have explicitly registered otherwise.
This is meant for applications where the standard function code and response handling does not fit the needs of the user.
There will be no automatic ``ILLEGAL_FUNCTION_CODE`` response anymore for this server!

_Hint_: You may combine this with the ``NIL_RESPONSE`` response variant described below to have a server that will gobble all incoming requests.
With RTU, this will give you a copy of all messages directed to another server on the bus, but without interfering with it.

A ``MBSworker`` callback must return a data object of ``ModbusMessage``. 
There are two special response messages predefined:
- ``NIL_RESPONSE``: no response at all will be sent back to the requester
- ``ECHO_RESPONSE``: the request will be sent back without any modification as a response. 
This is common for the writing function code requestsi that the Modbus standard defines.

**Note**: do not use the first byte of your ``ModbusMessage`` as ``0xFF``, followed by one of ``0xF0`` or ``0xF1``, as these are used internally for ``NIL_RESPONSE`` and ``ECHO_RESPONSE``!

##### ``uint16_t getValue(uint8_t *source, uint16_t sourceLength, T &v)``
Although you will be using ``ModbusMessage``'s ``get()`` function most of the time, there is a complement to the ``addValue()`` service function described in the ModbusClient section.
``getValue()`` will help you reading MSB-first data from an arbitrary data buffer.
This buffer is given as ``uint8_t *source``, its length as ``uint16_t sourceLength``.
Depending on the data type ``T`` of the variable given as reference ``&v`` to the ``getValue()`` call, the right number of bytes is taken from ``source``, converted into type ``T`` and stored in the variable ``v``.
The return value of ``getValue()`` is the number of bytes consumed from ``source`` to enable you updating the ``source`` pointer for the next call.

**Note**: please take care to reduce the ``sourceLength`` parameter accordingly in subsequent calls to ``getValue()``!

##### ``MBSworker getWorker(uint8_t serverID, uint8_t functionCode)``
You may check if a given serverID/functionCode combination has been covered by a callback with ``getWorker()``. This method will return the callback function pointer, if there is any, and a ``nullptr`` else.

##### ``bool isServerFor(uint8_t serverID)``
``isServerFor()`` will return ``true``, if at least one callback has been registered for the given ``serverID``, and ``false`` else.

##### ``uint32_t getMessageCount()``
Each request received will be counted. The ``getMessageCount()`` method will return the current state of the counter.

##### ``ModbusMessage localRequest(ModbusMessage request)``
This function is a simple local interface to issue requests to the server running. Responses are returned immediately - there is no request queueing involved. This call is *blocking* for that reason, so be prepared to have to wait until the response is ready!

A bridge (see below) will respond to this call for all known serverID/function code combinations, so the delegated request to a remote server may be involved as well.

**Note**: the ``localRequest()`` call will work even if the server has not been started (yet) by ``start()``, so if you need to communicate in other ways than RTU or TCP, you may make use of that!

##### ``void listServer()``
Mostly intended to be used in debug situations, ``listServer()`` will output all servers and their function codes served by the ModbusServer to the Serial Monitor.

#### ModbusServerTCP
Inconsistencies between the WiFi and Ethernet implementations in the core libraries required a split of ModbusServerTCP into the ``ModbusServerEthernet`` and ``ModbusServerWiFi`` variants.

Both are sharing the majority of methods etc., but need to be included with different file names and have different object names as well.

All relevant calls after initialization of the respective ModbusServer object are identical.

##### ModbusServerEthernet
The Ethernet variety requires the inclusion of the matching header file:
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

##### ``ModbusServerRTU(HardwareSerial& serial, uint32_t timeout)`` and<br> ``ModbusServerRTU(HardwareSerial& serial, uint32_t timeout, int rtsPin)``
The first parameter, ``serial`` is mandatory, as it gives the serial interface used to connect to the RTU Modbus to the server.
``timeout`` is less important as it is for TCP. It defines after what time of inactivity the server should loop around and re-initialize some working data.
A value of ``20000`` (20 seconds) is reasonable.
The third (optional) parameter ``rtsPin`` is the same as for the RTU Modbus client described above - if you are using a RS485 adaptor requiring a DE/RE line to be maintained, ``rtsPin`` should be the GPIO number of the wire to that DE/RE line. The linbrary will take care of toggling the pin.

  // start: create task with RTU server to accept requests
##### ``bool start()`` and<br> ``bool start(int coreID)``
With ``start()`` the server will create its background task and start listening to the Modbus. 
The optional parameter ``coreID`` may be used to have that background task run on the named core for multi-core MCUs.

##### ``bool stop()``
The server background process can be stopped and the task be deleted with the ``stop()`` call. You may start it again with another ``start()`` afterwards.
In fact a ``start()`` to an already running server will stop and then restart it.

[Return to top](#eModbus)

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

#### ``ModbusBridge()`` and<br> ``ModbusBridge(uint32_t TOV)``
These are the constructors for a TCP-based bridge. The optional ``TOV`` (="timeout value") parameter sets the maximum time the bridge will wait for the responses from external servers. The default value for this is 10000 - 10 seconds.

**Note**: this is a value common for all servers connected, so choose the value sensibly to cover all normal response times of the servers. 
Else you will be losing responses from those servers taking longer to respond. Their responses will be dropped unread if arriving after the bridge's timeout had struck.

#### ``ModbusBridge(HardwareSerial& serial, uint32_t timeout)``,<br> ``ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV)`` and<br> ``ModbusBridge(HardwareSerial& serial, uint32_t timeout, uint32_t TOV, int rtsPin)``
The corresponding constructors for the RTU bridge variant. With the exception of ``TOV`` all parameters are those of the underlying RTU server.
The duplicity of ``timeout`` and ``TOV`` may look irritating at the first moment, but while ``timeout`` denotes the timeout the bridge will use when waiting for messages as a _server_, the ``TOV`` value is the _bridge_ timeout as described above.

#### ``bool attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client)`` and<br> ``bool attachServer(uint8_t aliasID, uint8_t serverID, uint8_t functionCode, ModbusClient *client, IPAddress host, uint16_t port)``
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

[Return to top](#eModbus)

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

#### ANSI colors
If you are using a terminal to view the logging output that knows about ANSI commands, you can add some predefined color codes into your log statements:
- ``LL_RED``
- ``LL_YELLOW``
- ``LL_GREEN``
- ``LL_BLUE``
- ``LL_CYAN`` and 
- ``LL_MAGENTA``

Please make sure that you have put in a ``LL_NORM`` code after the colored section is to end to switch back to the regular output colors.

[Return to top](#eModbus)
