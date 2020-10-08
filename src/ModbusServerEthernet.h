// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_ETHERNET_H
#define _MODBUS_SERVER_ETHERNET_H
#include <Ethernet.h>

#define WIFIMODE 1
#define ETHERNETMODE 2
#define TCPMODE ETHERNETMODE
#define CLIENTTYPE EthernetClient
#define CLASSNAME ModbusServerEthernet

#include "ModbusServerTCP.h"

#endif
