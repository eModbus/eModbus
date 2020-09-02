#ifndef _MODBUS_TYPEDEFS_H
#define _MODBUS_TYPEDEFS_H
#include <stdint.h>
#include <stddef.h>

namespace ModbusClient {

enum FunctionCode : uint8_t {
  READ_COIL               = 0x01,
  READ_DISCR_INPUT        = 0x02,
  READ_HOLD_REGISTER      = 0x03,
  READ_INPUT_REGISTER     = 0x04,
  WRITE_COIL              = 0x05,
  WRITE_HOLD_REGISTER     = 0x06,
  READ_EXCEPTION_SERIAL   = 0x07,
  DIAGNOSTICS_SERIAL      = 0x08,
  READ_COMM_CNT_SERIAL    = 0x0B,
  READ_COMM_LOG_SERIAL    = 0x0C,
  WRITE_MULT_COILS        = 0x0F,
  WRITE_MULT_REGISTERS    = 0x10,
  REPORT_SERVER_ID_SERIAL = 0x11,
  READ_FILE_RECORD        = 0x14,
  WRITE_FILE_RECORD       = 0x15,
  MASK_WRITE_REGISTER     = 0x16,
  R_W_MULT_REGISTERS      = 0x17,
  READ_FIFO_QUEUE         = 0x18,
  ENCAPSULATED_INTERFACE  = 0x2B,
  USER_DEFINED_41         = 0x41,
  USER_DEFINED_42         = 0x42,
  USER_DEFINED_43         = 0x43,
  USER_DEFINED_44         = 0x44,
  USER_DEFINED_45         = 0x45,
  USER_DEFINED_46         = 0x46,
  USER_DEFINED_47         = 0x47,
  USER_DEFINED_48         = 0x48,
  USER_DEFINED_64         = 0x64,
  USER_DEFINED_65         = 0x65,
  USER_DEFINED_66         = 0x66,
  USER_DEFINED_67         = 0x67,
  USER_DEFINED_68         = 0x68,
  USER_DEFINED_69         = 0x69,
  USER_DEFINED_6A         = 0x6A,
  USER_DEFINED_6B         = 0x6B,
  USER_DEFINED_6C         = 0x6C,
  USER_DEFINED_6D         = 0x6D,
  USER_DEFINED_6E         = 0x6E,
};

enum Error : uint8_t {
  SUCCESS                = 0x00,
  ILLEGAL_FUNCTION       = 0x01,
  ILLEGAL_DATA_ADDRESS   = 0x02,
  ILLEGAL_DATA_VALUE     = 0x03,
  SERVER_DEVICE_FAILURE  = 0x04,
  ACKNOWLEDGE            = 0x05,
  SERVER_DEVICE_BUSY     = 0x06,
  NEGATIVE_ACKNOWLEDGE   = 0x07,
  MEMORY_PARITY_ERROR    = 0x08,
  GATEWAY_PATH_UNAVAIL   = 0x0A,
  GATEWAY_TARGET_NO_RESP = 0x0B,
  TIMEOUT                = 0xE0,
  INVALID_SERVER         = 0xE1,
  INVALID_FUNCTION       = 0xE2,
  CRC_ERROR              = 0xE3, // only for Modbus-RTU
  FC_MISMATCH            = 0xE4,
  SERVER_ID_MISMATCH     = 0xE5,
  PACKET_LENGTH_ERROR    = 0xE6,
  UNDEFINED_ERROR        = 0xFF  // otherwise uncovered communication error
};

typedef void (MBOnData) (uint8_t serverID, uint8_t functionCode, uint8_t *data, uint16_t data_length);
typedef void (MBOnError) (ModbusClient::Error);
typedef void (MBOnDataToken) (uint8_t serverID, uint8_t functionCode, uint8_t *data, uint16_t data_length, uint32_t token);
typedef void (MBOnErrorToken) (ModbusClient::Error, uint32_t token);

}  // namespace ModbusClient


#endif