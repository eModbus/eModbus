
// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

// Example code to show the usage of the eModbus library. 
// Please refer to root/Readme.md for a full description.

// Note: this is an example for the "EASTRON SDM630 Modbus-V2" power meter!

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>
#include "eModbusRTULib.h"


// Definitions for this special case
#define RXPIN GPIO_NUM_16
#define TXPIN GPIO_NUM_17
#define REDEPIN GPIO_NUM_15
#define BAUDRATE 9600
#define TIME_OUT 2000
#define POLL_INTERVAL 1000

/*
#define FIRST_REGISTER 223
#define REG_LEN 4
#define NUM_VALUES 1
*/

//#define READ_INTERVAL 1000

//bool data_ready = false;
float values[NUM_VALUES];

//uint32_t request_time;

uint16_t Reg[REG_LEN];


// Create a ModbusRTU client instance
// The RS485 module has no halfduplex, so the second parameter with the DE/RE pin is required!
eModbusRTU MB(Serial2, REDEPIN);


/////----- User Input Starts----//////////

#define Total_Packets 2
#define Total_Registers 4

packet Pkt[Total_Packets];

/////----- User Input Ends----//////////

// Setup() - initialization happens here
void setup() 
{
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("__ OK __");

  
//||||----- User Input Starts----|||||

  MB.eModbusRTU_packetinit(Pkt, Total_Packets, Total_Registers);
  MB.eModbusRTU_construct(1, &Pkt[0], 1, READ_HOLD_REGISTER, 223, 2);  // Initializing packet 1
  MB.eModbusRTU_construct(2, &Pkt[1], 1, READ_HOLD_REGISTER, 99, 2);  // Initializing packet 2
  
  
  MB.eModbusRTU_configure(Serial2, REDEPIN, RXPIN, TXPIN, BAUDRATE, SERIAL_8N1, TIME_OUT, POLL_INTERVAL, Pkt); 

//||||----- User Input Ends----|||||
}


unsigned long pre_mills = 0, pre_mills1 = 0;

void loop()
{
  MB.eModbusRTU_Loop(Pkt, Total_Packets);

  unsigned long start_mills = millis();

  if(start_mills - pre_mills1 >= 1000)
  {
    MB.modbus_packet(1);
    MB.modbus_packet(2);
    
    pre_mills1 = start_mills;  
  }
  
  if(start_mills - pre_mills >= 3000)
  {
    
    Serial.printf("\n Dat seq : ");
    
    for(int i=0; i<Total_Registers; i++)
    {
       Serial.printf("%X ", Reg[i]);
    }

    
    float M1 = MB.modreg_read(1, 223, 2, FP32bit_BA); // packet, Input address, Length, Format
    
    Serial.printf("\n Value Read : %f", M1);
    
    
    pre_mills = start_mills;  
  }
}