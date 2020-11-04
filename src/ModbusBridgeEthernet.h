// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_BRIDGE_ETHERNET_H
#define _MODBUS_BRIDGE_ETHERNET_H
#include <Ethernet.h>
#include "ModbusBridgeTemp.h"

using ModbusBridgeEthernet = ModbusBridge<ModbusServerTCP<EthernetServer, EthernetClient>>;

#endif
