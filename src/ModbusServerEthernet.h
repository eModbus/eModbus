// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_SERVER_ETHERNET_H
#define _MODBUS_SERVER_ETHERNET_H
#include <Ethernet.h>
#include <SPI.h>
#include "ModbusServerTCPtemp.h"

using ModbusServerEthernet = ModbusServerTCP<EthernetServer, EthernetClient>;

#endif
