## Linux Modbus client
There is some support for a Modbus client running on Linux in the library.
I developed it for my own use and it serves my purposes, but is far from being the same quality the ESP library has.
So here more than ever the motto

**Use at your own risk!**

applies!

### Prerequisites
The files in this folder are Linux-only. 
- ``Client.cpp`` and ``Client.h`` are implementing the same ``Client`` class the Arduino/ESP32/ESP8266 core does provide, whereas ``IPAddress.cpp`` and ``IPAddress.h`` are supplying the class holding IP addresses the way the eModbus library likes it.
- *Note*: ``Client`` is providing a public static function ``IPAddress hostname_to_ip(const char *hostname);`` that does a DNS conversion for the hostname given. If no IP could be found, a NIL_ADDR is returned!
- *Note*: In addition to the known types, ``IPAddress`` does support initialization, assignment and comparison with a ``const char *ip``also. It is perfectly valid to conveniently write ``IPAddress i = "192.168.178.1";``.
- ``main.cpp`` is a basic example of a command line Modbus client that can be used to issue ``READ_HOLD_REGISTER`` request to any Modbus server your network may provide. 
- ``parseTarget.h`` and ``parseTarget.cpp`` are providing an ``int parseTarget(const char *source, IPAddress &IP, uint16_t &port, uint8_t &serverID)`` call to analyze and extract a Modbus server target description to a combination of IP, port and server ID. The descriptor has the form ``IP[:port[:serverID]]`` or ``hostname[:port[:serverID]]``.
- the ``Makefile`` is set up to build this example client.

Additionally, the ``libexplain`` lib was installed to get better error descriptions. It is used in ``Client.cpp``.
If you do not want or need it, you will have to change the source there.

You will need to copy some more files from the main eModbus ``src`` folder here to complete the required sources:
- ``Logging.cpp`` and ``Logging.h``
- ``options.h``
- ``ModbusClient.cpp`` and ``ModbusClient.h``
- ``ModbusClientTCP.cpp`` and ``ModbusClientTCP.h``
- ``ModbusMessage.cpp`` and ``ModbusMessage.h``
- ``ModbusError.h``
- ``ModbusTypeDefs.h``

### Building the example
The example was developed on **Lubuntu 20.04**, but should run on any major Linux variety.

If you have collected all the files mentioned in a folder, go into this folder and run ``make``.
In case all runs okay, you will have a ``testMB`` executable after ``make`` has finished.
The ``Makefile`` has some more targets:
- ``make clean`` will remove all object and reference files that may have been created during the build.
- ``make dist`` will pack all necessary files into a zipped file ``MB.zip``.

### Trying the example client
The client is called with 3 arguments:
```
./testMB <target> <addr> <words>
```

A typical run may look like:
```
miq@LinuxBox:~/MB/work$ ./testMB 192.168.178.77:6502:20 1 100
Response: serverID=20, FC=3, Token=04FE49CA, length=203:
[N] Data: @7FD2DC000DC0/203:
  | 0000: 14 03 64 48 21 00 90 51  32 00 00 01 39 00 09 00  |..dH!..Q2...9...|
  | 0010: 03 C0 A8 C7 28 01 F6 FF  FF FF 00 C0 A8 C7 01 C0  |....(...........|
  | 0020: A8 C7 01 00 31 5A 67 00  01 C0 A8 C7 28 FF FF FF  |....1Zg.....(...|
  | 0030: 00 C0 A8 C7 01 C0 A8 C7  01 00 14 01 02 0B DB 00  |................|
  | 0040: 02 00 03 01 02 0C 25 00  02 00 03 01 02 0B DB 00  |......%.........|
  | 0050: 02 00 03 14 07 00 14 00  02 00 07 01 02 0C 25 00  |..............%.|
  | 0060: 02 00 03 00 00 00 00 00  00 00 00 04 02 00 02 00  |................|
  | 0070: 02 00 03 04 02 00 04 00  02 00 03 04 02 00 06 00  |................|
  | 0080: 02 00 03 04 03 00 10 00  02 00 08 04 01 00 12 00  |................|
  | 0090: 04 00 08 04 01 00 19 00  04 00 03 00 00 00 00 00  |................|
  | 00A0: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
  | 00B0: 00 00 00 14 06 00 60 00  02 00 03 00 01 5F C4 AD  |......`......_..|
  | 00C0: 07 60 58 C6 D0 00 00 53  65 70 20                 |.`X....Sep      |
```
