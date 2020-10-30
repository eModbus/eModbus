// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_WIFI_H
#define _MODBUS_SERVER_WIFI_H
#include <WiFi.h>
#include "ModbusServerTCPtemp.h"

using ModbusServerWiFi = ModbusServerTCP<WiFiServer, WiFiClient>;

#endif
