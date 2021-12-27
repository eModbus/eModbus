#include <Arduino.h>
#include "eModbusRTU_Lib.h"

extern uint16_t Reg[];
extern packet Pkt[];

// For Packet initialisation
void eModbusRTU::eModbusRTU_packetinit(packet *_packet, uint8_t Tot_Pkt, unsigned Tot_Reg)
{
  Tot_Packet = Tot_Pkt;
  Tot_Regstr = Tot_Reg;

  for(int i=0; i<Tot_Pkt; i++)
  {
    _packet[i].success_req = 0; // Initiating the Success & Fail req to 0
    _packet[i].fail_req = 0;
  }
}

// For Modbus configuration
void eModbusRTU::eModbusRTU_configure(HardwareSerial &serialport, uint8_t TE_RE_Pin, uint8_t RX_Pin, uint8_t TX_Pin, long Baudrate, unsigned long Byteformat, long Timeout, unsigned Poll_intr, packet *_packet) // Serial port, TE-RE_Pin, RX_Pin, TX_Pin, Baud rate, Byte format, Timeout, polling interval
{
  //eModbusRTU((*serialport), TE_RE_Pin);

  Poll_interval = Poll_intr;

  serialport.begin(Baudrate, Byteformat, RX_Pin, TX_Pin);

  // Set up ModbusRTU client.
  // - provide onData handler function

  //using namespace std::placeholders;
  
  onDataHandler(std::bind(&eModbusRTU::handleData, this, std::placeholders::_1, std::placeholders::_2));
  
  // - provide onError handler function
  onErrorHandler(std::bind(&eModbusRTU::handleError, this, std::placeholders::_1, std::placeholders::_2));
  
  // Set message timeout
  setTimeout(Timeout);
  
  // Start ModbusRTU background task
  begin();  
}

// For Packetization
void eModbusRTU::eModbusRTU_construct(uint8_t packet_num, packet *_packet, uint8_t id, uint8_t function, uint16_t st_address, uint16_t Reg_len) // Packet no, Packet, ID, Function, start address, Register len
{
  _packet->pack_no = packet_num;
  _packet->id = id;
  _packet->func = function;
  _packet->str_addr = st_address;
  _packet->reg_len = Reg_len;  
}

void eModbusRTU::handleData(ModbusMessage response, uint32_t token)
{
  // First value is on pos 3, after server ID, function code and length byte
  uint16_t offs = 3;
  // The device has values all as IEEE754 float32 in two consecutive registers
  // Read the requested in a loop

/*
  for (uint8_t i = 0; i < REG_LEN; ++i) 
  {
   // offs = response.get(offs, values[i]);
    //offs = response.get(offs, Reg[i]);

    HEXDUMP_N("Data", response.data(), response.size());
  }

  Serial.printf("\n Hand :");
  for (auto& byte : response) 
  {
    Serial.printf("%02X ", byte);
  }
 */
  float val[2];

  
 /* 
  if(token == 1)
  {
    HEXDUMP_N("P1 Data", response.data(), response.size());
    //offs = response.get(offs, val[0], SWAP_REGISTERS);
    offs = response.get(offs, Reg[0]);
    offs = response.get(offs, Reg[1]);

    Serial.printf("\n Reg : %X  %X", Reg[0], Reg[1]);
  }

  else if(token == 2)
  {
    HEXDUMP_N("P2 Data", response.data(), response.size());
    offs = response.get(offs, val[1], SWAP_REGISTERS);
    Serial.printf("Data : %f", val[1]);
  }

  
  else
  {
    
  }
  // Signal "data is complete"
 */

  for(int i=1; i<=Tot_Packet; i++)
  {
    if(token == i)
    {
      Serial.printf("\n P%d", i);
      HEXDUMP_N(" Data",  response.data(), response.size());

      Pkt[i-1].success_req++; // Incrementing success request
      
      for(int j=0; j<Pkt[i-1].reg_len; j++)
      {        
        offs = response.get(offs, Reg[Reg_calc(i)+j]);
      }  
    }
  }
  
  request_time = token;
  data_ready = true;
}

unsigned eModbusRTU::Reg_calc(uint8_t pckt) // Register Calculation
{
   unsigned kt = 0;
   
   for(int i=0; i<pckt-1; i++)
   {
      kt += Pkt[i].reg_len; 
   }

   return(kt);
}

void eModbusRTU::handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);

/* 
  if(token == 1)
  {
    LOG_E("P1 - Error response: %02X - %s\n", (int)me, (const char *)me);
    
  }

  else if(token == 2)
  {
    LOG_E("P2 - Error response: %02X - %s\n", (int)me, (const char *)me);
  }

  else
  {
    Serial.print("\n Error Packet response ");
  }
*/
    if(token <= Tot_Packet)
    {   

      Pkt[token-1].fail_req++;

      LOG_E("P%d - Fail Req :%d, Error response: %02X - %s\n",1, (unsigned)token, (int)me, (const char *)me);  
      
    }

    else
    {
      Serial.println("\n Packet overflow");
    }
    
}


void eModbusRTU::eModbusRTU_Loop(packet *_packet, uint8_t Tot_pkt)
{
  static uint32_t next_request = millis();

  // Shall we do another request?
  if (millis() - next_request > Poll_interval) 
  {
    // Yes.
    data_ready = false;
    // Issue the request
/*    
    Error err = addRequest(1, 1, READ_HOLD_REGISTER, FIRST_REGISTER, REG_LEN);
    
    if (err!=SUCCESS) 
    {
      ModbusError e(err);
      LOG_E("P1 - Error creating request: %02X - %s\n", (int)e, (const char *)e);
    }

    err = addRequest(2, 1, READ_HOLD_REGISTER, 99, 2);

    //Error err = addRequest(1, 1, READ_HOLD_REGISTER, FIRST_REGISTER, REG_LEN);
    
    if (err!=SUCCESS) 
    {
      ModbusError e(err);
      LOG_E("P2 - Error creating request: %02X - %s\n", (int)e, (const char *)e);
    }
*/
  Error err;
  
  for(int i=0; i<Tot_pkt; i++)
  {
    err = addRequest(_packet[i].pack_no, _packet[i].id, _packet[i].func, _packet[i].str_addr, _packet[i].reg_len);
    
    if (err!=SUCCESS) 
    {
      ModbusError e(err);
      LOG_E("P%d - Error creating request: %02X - %s\n", (unsigned)_packet[i].pack_no, (int)e, (const char *)e);
    }
  }

    // Save current time to check for next cycle
    next_request = millis();
  } 
  
  else 
  {
    // No, but we may have another response
    if (data_ready) 
    {
   /*   
      // We do. Print out the data
      Serial.printf("Requested at %8.3fs:\n", request_time / 1000.0);
      for (uint8_t i = 0; i < REG_LEN; ++i) 
      {
        Serial.printf("   %04X: %04X\n", i * 2 + FIRST_REGISTER, Reg[i]);
      }
      Serial.printf("----------\n\n");
   */   
      data_ready = false;
    }
  }
}

void eModbusRTU::modbus_packet(unsigned pack)
{
  Serial.print(F("\n P"));
  Serial.print(pack);

  Serial.print(F(" Fail req : "));
  Serial.print(Pkt[pack-1].fail_req);

  Serial.print(F(", Succ req : "));
  Serial.print(Pkt[pack-1].success_req);
}



float eModbusRTU::modreg_read(byte pack, unsigned addr, byte leng, byte frmat) // Packet no, Modbus address, Length of registers, Format
{
  int P_ad, P_rg, regtr;
  float Power;

  P_ad = Pkt[pack - 1].str_addr; //
  
  P_rg = Reg_calc(pack);

  regtr = addr - P_ad + P_rg;

/*
  Serial.print(F("\n reg :"));
  Serial.print(regtr);
*/

  if((leng==2)&&(frmat==1)) // Format is 32bit Floating Point and the Register size of 2
  {
    unsigned long temp;
    
    temp = (unsigned long)Reg[regtr+1] << 16 | Reg[regtr];
    Power = *(float*)&temp;
  }

  else if((leng==2)&&(frmat==2)) // Format is 32bit Floating Point and the Register size of 2
  {
    unsigned long temp;
    temp = (unsigned long)Reg[regtr] << 16 | Reg[regtr+1];
    Power = *(float*)&temp;
  }

  else if((leng == 1)&&(frmat == UINT16bit))
  {
    
    //Serial.print("LOW:");
    //Serial.print(regs[regtr]);

    Power = Reg[regtr];
    //Power = packet[pack - 1].reg_len;
  }

  else if((leng == 2)&&(frmat == INT32bit_AB))
  {
    unsigned long temp;
    int k_temp;
    temp = (unsigned long)Reg[regtr] << 16 | Reg[regtr+1];
    k_temp = *(int*)&temp;
    Power = k_temp; 
  }

  else if((leng == 2)&&(frmat == INT32bit_BA))
  {
    unsigned long temp;
    int k_temp;
    temp = (unsigned long)Reg[regtr+1] << 16 | Reg[regtr];
    k_temp = *(int*)&temp;
    Power = k_temp; 
  }


  else if((leng == 2)&&(frmat == UINT32bit_AB))
  {
    unsigned long temp;
    temp = (unsigned long)Reg[regtr] << 16 | Reg[regtr+1];
/*  
    Serial.print("LOW:");
    Serial.print(Reg[regtr]);
    Serial.print("HIGH:");
    Serial.print(Reg[regtr+1]);
*/    
    //Serial.println(temp);
    Power = temp; 
  }

  else if((leng == 2)&&(frmat == UINT32bit_BA))
  {
    unsigned long temp;
    //temp = (unsigned long)Reg[regtr+1] << 16 | Reg[regtr];

    temp = (unsigned long)Reg[regtr+1] << 16 | Reg[regtr];
/*  
    Serial.print("LOW:");
    Serial.print(regs[regtr]);
    Serial.print("HIGH:");
    Serial.print(regs[regtr+1]);
*/    
    //Serial.println(temp);
    Power = temp; 
  }


  else
  {
    Serial.println(F("Modbus format not supported"));
    Power=0;
  }

  return(Power);
}

float eModbusRTU::Modreg_Aread(byte pck, unsigned adr, byte ln, byte fmt, float mult) // Packet no, Modbus address, Length of registers, Format, Multiplier
{
  return(round_2dec(modreg_read(pck, adr, ln, fmt)*mult));
} 

float Inp_Out::round_2dec(float var) // rounding the values from 2 decimal point to 1 decimal point.
{ 
  float value = (long long)(var * 100 + .5); 
  return (float)value / 100; 
}