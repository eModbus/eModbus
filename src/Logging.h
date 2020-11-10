// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_LOGGING
#define _MODBUS_LOGGING

#ifndef LOGDEVICE
#include <Arduino.h> // for Serial
#define LOGDEVICE Serial
#endif

#define LOG_LEVEL_NONE (0)
#define LOG_LEVEL_CRITICAL (1)
#define LOG_LEVEL_ERROR (2)
#define LOG_LEVEL_WARNING (3)
#define LOG_LEVEL_INFO (4)
#define LOG_LEVEL_DEBUG (5)
#define LOG_LEVEL_VERBOSE (6)

constexpr const char* str_end(const char *str) {
    return *str ? str_end(str + 1) : str;
}

constexpr bool str_slant(const char *str) {
    return *str == '/' ? true : (*str ? str_slant(str + 1) : false);
}

constexpr const char* r_slant(const char* str) {
    return *str == '/' ? (str + 1) : r_slant(str - 1);
}
constexpr const char* file_name(const char* str) {
    return str_slant(str) ? r_slant(str_end(str)) : str;
}

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL
#endif

#define LOGLINE_T(x, format, ...) LOGDEVICE.printf("[" #x "] %s:%d:%s " format, file_name(__FILE__), __LINE__, __func__, ##__VA_ARGS__)

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_NONE
#define LOG_N(format, ...) LOGLINE_T(N, format, ##__VA_ARGS__)
#else
#define LOG_N(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_CRITICAL
#define LOG_C(format, ...) LOGLINE_T(C, format, ##__VA_ARGS__)
#else
#define LOG_c(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(format, ...) LOGLINE_T(E, format, ##__VA_ARGS__)
#else
#define LOG_E(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_WARNING
#define LOG_W(format, ...) LOGLINE_T(W, format, ##__VA_ARGS__)
#else
#define LOG_W(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(format, ...) LOGLINE_T(I, format, ##__VA_ARGS__)
#else
#define LOG_I(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(format, ...) LOGLINE_T(D, format, ##__VA_ARGS__)
#else
#define LOG_D(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_VERBOSE
#define LOG_V(format, ...) LOGLINE_T(V, format, ##__VA_ARGS__)
#else
#define LOG_V(format, ...)
#endif

#endif
