// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_LOGGING
#define _MODBUS_LOGGING

#if defined ARDUINO_ARCH_ESP32
#include <esp32-hal-log.h>
#else
#define log_v(...)
#define log_d(...)
#define log_i(...)
#define log_w(...)
#define log_e(...)
#define log_n(...)
#endif

#endif
