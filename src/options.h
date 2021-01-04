// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _EMODBUS_OPTIONS_H
#define _EMODBUS_OPTIONS_H

#if defined(ESP32) 
#define USE_MUTEX 1
#define HAS_FREERTOS 1
#elif defined(ESP8266)
#define USE_MUTEX 0
#define HAS_FREERTOS 0
#else
#error Define target in options.h
#endif

#endif