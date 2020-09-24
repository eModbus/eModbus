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

Modbus TCP will require either an Ethernet module like the WizNet W5xxx series connected to the SPI interface or an (internal) WiFi adaptor. The library is by the way using the functions defined in ``Client.h`` only.
