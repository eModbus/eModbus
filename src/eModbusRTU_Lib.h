#include <Arduino.h>
#include "HardwareSerial.h"

#ifndef eModbusRTULib_h
#define eModbusRTULib_h

// Include the header for the ModbusClient RTU style
//#include "ModbusError.h"
#include "ModbusClientRTU.h"
//#include "Input_Output.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "Logging.h"



#define FP32bit_BA 1
#define FP32bit_AB 2
#define UINT16bit 3
#define INT32bit_AB 4
#define UINT32bit_AB 5

#define INT32bit_BA 6
#define UINT32bit_BA 7


typedef struct
{
  uint8_t pack_no;
  uint8_t id;
  uint8_t func;
  uint16_t str_addr;
  uint16_t reg_len;
  unsigned success_req;
  unsigned fail_req;
}packet;

class eModbusRTU : public ModbusClientRTU
{
  private:

  bool data_ready;
  //float values[NUM_VALUES];
  uint32_t request_time;
  //uint16_t Reg[REG_LEN];
  unsigned Poll_interval;

  public:

  uint8_t Tot_Packet;
  unsigned Tot_Regstr;

  //void eModbusRTU_init(HardwareSerial &serialport, uint8_t TE_RE_Pin, uint16_t time_out);
  eModbusRTU(HardwareSerial &serial, int8_t rtsPin) : ModbusClientRTU(serial, rtsPin)
  {
    data_ready = false;
  }
  void eModbusRTU_packetinit(packet *_packet, uint8_t Tot_Pkt, unsigned Tot_Reg); // Total No.of Packet, Total No. of Registers
  void eModbusRTU_configure(HardwareSerial &serialport, uint8_t TE_RE_Pin, uint8_t RX_Pin, uint8_t TX_Pin, long Baudrate, unsigned long Byteformat, long Timeout, unsigned Poll_intr, packet *_packet); // Serial port, TE-RE_Pin, RX_Pin, TX_Pin, Baud rate, Byte format, Timeout, packet, Total Packt
  void eModbusRTU_construct(uint8_t packet_num, packet *_packet, uint8_t id, uint8_t function, uint16_t st_address, uint16_t Reg_len); // Packet no, Packet, ID, Function, start address, Register len
  void handleData(ModbusMessage response, uint32_t token); // Data handle
  void handleError(Error error, uint32_t token); // Handle Error
  void eModbusRTU_Loop(packet *_packet, uint8_t Tot_pkt);
  float round_2dec(float var);

  unsigned Reg_calc(uint8_t pckt); // Register calculation
  void modbus_packet(unsigned pack);
  float modreg_read(byte pack, unsigned addr, byte leng, byte frmat); // Packet no, Modbus address, Length of registers, Format
  float Modreg_Aread(byte pck, unsigned adr, byte ln, byte fmt, float mult); // Packet no, Modbus address, Length of registers, Format, Multiplier
};














#endif