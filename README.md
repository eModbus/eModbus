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
```
The only thing missing are the requests now. In ``loop()`` you will trigger these like seen below - here a "read holding register" request is sent to server #1, requesting one data word at address 10. Please disregard the ``token`` value of 0x12345678 for now, it will be explained below:
```
Error err = RS485.addRequest(1, READ_HOLD_REGISTER, 10, 1, 0x12345678);
  if (err!=SUCCESS) {
    ModbusError e(err);
    Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  }
```
If anything was wrong with your parameters the request will not be created. You will be given an error return value to find out what went wrong.
In case everything was okay, the request is created and put into the background queue to be processed. As soon as that has happened, your ``onData`` or ``onError`` callbacks will be called with the response.