// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusTypeDefs.h"

using Modbus::FCType;
using Modbus::FCT;

// Initialize function code type table
FCType FCT::table[] = {
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FC01,      FC01,      FC01,      FC01,      FC01,      FC01,      FC07,       // 0x0.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCGENERIC, FCILLEGAL, FCILLEGAL, FC07,      FC07,      FCILLEGAL, FCILLEGAL, FC0F,       // 0x0.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FC10,      FC07,      FCILLEGAL, FCILLEGAL, FCGENERIC, FCGENERIC, FC16,      FCGENERIC,  // 0x1.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FC18,      FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x1.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x2.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCGENERIC, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x2.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x3.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x3.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,     // 0x4.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCUSER,    FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x4.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x5.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x5.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCUSER,    FCUSER,    FCUSER,    FCUSER,     // 0x6.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCUSER,    FCILLEGAL,  // 0x6.
// 0x.0      0x.1       0x.2       0x.3       0x.4       0x.5       0x.6       0x.7
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x7.
// 0x.8      0x.9       0x.A       0x.B       0x.C       0x.D       0x.E       0x.F
  FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL, FCILLEGAL,  // 0x7.
};

// FCT::getType: get the function code type for a given function code
const FCType FCT::getType(uint8_t functionCode) {
  return table[functionCode & 0x7F];
}

// setType: change the type of a function code.
// This is possible only for the codes undefined yet and will return
// the effective type
const FCType FCT::redefineType(uint8_t functionCode, const FCType type) {
  uint8_t fc = functionCode & 0x7F;
  
  // Allow modifications for yet undefined codes only
  if (table[fc] == FCILLEGAL) {
    table[fc] = type;
  }
  return table[fc];
}



