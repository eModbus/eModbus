// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_WIFI_H
#define _MODBUS_SERVER_WIFI_H
#include <WiFi.h>

#define WIFIMODE 1
#define ETHERNETMODE 2
#define TCPMODE WIFIMODE
#define CLIENTTYPE WiFiClient
#define SERVERTYPE WiFiServer
#define CLASSNAME ModbusServerWiFi

#include "ModbusServerTCP.h"

#endif
