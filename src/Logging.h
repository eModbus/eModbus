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
#define LOG_LEVEL LOG_LEVEL_ERROR
#endif

#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL
#endif

extern int MBUlogLvl;
void logHexDump(Print& output, const char *letter, const char *label, const uint8_t *data, const size_t length);

#define LOG_LINE_T(level, x, format, ...) if (MBUlogLvl >= level) LOGDEVICE.printf("[" #x "] %s:%d:%s: " format, file_name(__FILE__), __LINE__, __func__, ##__VA_ARGS__)
#define LOG_RAW_T(level, x, format, ...) if (MBUlogLvl >= level) LOGDEVICE.printf(format, ##__VA_ARGS__)
#define HEX_DUMP_T(x, level, label, address, length) if (MBUlogLvl >= level) logHexDump(LOGDEVICE, #x, label, address, length)

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_NONE
#define LOG_N(format, ...) LOG_LINE_T(LOG_LEVEL_NONE, N, format, ##__VA_ARGS__)
#define LOGRAW_N(format, ...) LOG_RAW_T(LOG_LEVEL_NONE, N, format, ##__VA_ARGS__)
#define HEXDUMP_N(label, address, length) HEX_DUMP_T(N, LOG_LEVEL_NONE, label, address, length)
#else
#define LOG_N(format, ...)
#define LOGRAW_N(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_CRITICAL
#define LOG_C(format, ...) LOG_LINE_T(LOG_LEVEL_CRITICAL, C, format, ##__VA_ARGS__)
#define LOGRAW_C(format, ...) LOG_RAW_T(LOG_LEVEL_CRITICAL, C, format, ##__VA_ARGS__)
#define HEXDUMP_C(label, address, length) HEX_DUMP_T(C, LOG_LEVEL_CRITICAL, label, address, length)
#else
#define LOG_c(format, ...)
#define LOGRAW_C(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(format, ...) LOG_LINE_T(LOG_LEVEL_ERROR, E, format, ##__VA_ARGS__)
#define LOGRAW_E(format, ...) LOG_RAW_T(LOG_LEVEL_ERROR, E, format, ##__VA_ARGS__)
#define HEXDUMP_E(label, address, length) HEX_DUMP_T(E, LOG_LEVEL_ERROR, label, address, length)
#else
#define LOG_E(format, ...)
#define LOGRAW_E(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_WARNING
#define LOG_W(format, ...) LOG_LINE_T(LOG_LEVEL_WARNING, W, format, ##__VA_ARGS__)
#define LOGRAW_W(format, ...) LOG_RAW_T(LOG_LEVEL_WARNING, W, format, ##__VA_ARGS__)
#define HEXDUMP_W(label, address, length) HEX_DUMP_T(W, LOG_LEVEL_WARNING, label, address, length)
#else
#define LOG_W(format, ...)
#define LOGRAW_W(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(format, ...) LOG_LINE_T(LOG_LEVEL_INFO, I, format, ##__VA_ARGS__)
#define LOGRAW_I(format, ...) LOG_RAW_T(LOG_LEVEL_INFO, I, format, ##__VA_ARGS__)
#define HEXDUMP_I(label, address, length) HEX_DUMP_T(I, LOG_LEVEL_INFO, label, address, length)
#else
#define LOG_I(format, ...)
#define LOGRAW_I(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(format, ...) LOG_LINE_T(LOG_LEVEL_DEBUG, D, format, ##__VA_ARGS__)
#define LOGRAW_D(format, ...) LOG_RAW_T(LOG_LEVEL_DEBUG, D, format, ##__VA_ARGS__)
#define HEXDUMP_D(label, address, length) HEX_DUMP_T(D, LOG_LEVEL_DEBUG, label, address, length)
#else
#define LOG_D(format, ...)
#define LOGRAW_D(format, ...)
#endif

#if LOCAL_LOG_LEVEL >= LOG_LEVEL_VERBOSE
#define LOG_V(format, ...) LOG_LINE_T(LOG_LEVEL_VERBOSE, V, format, ##__VA_ARGS__)
#define LOGRAW_V(format, ...) LOG_RAW_T(LOG_LEVEL_VERBOSE, V, format, ##__VA_ARGS__)
#define HEXDUMP_V(label, address, length) HEX_DUMP_T(v, LOG_LEVEL_VERBOSE, label, address, length)
#else
#define LOG_V(format, ...)
#define LOGRAW_V(format, ...)
#endif

#endif
