// =================================================================================================
// eModbus: Copyright 2020, 2021 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================

#ifndef _COILDATA_H
#define _COILDATA_H

#include <vector>
#include <cstdint>
#include "options.h"

using std::vector;

// CoilData: representing Modbus coil (=bit) values
class CoilData {
public:
  // Constructor: mandatory size in bits, optional initial value for all bits
  // Maximum size is 2000 coils (=250 bytes)
  explicit CoilData(uint16_t size, bool initValue = false);

  // Alternate constructor, taking a "1101..." bit image char array to init
  explicit CoilData(const char *initVector);

  // Destructor: take car eof cleaning up
  ~CoilData();

  // Assignment operator
  CoilData& operator=(const CoilData& m);
  
  // Copy constructor
  CoilData(const CoilData& m);

#ifndef NO_MOVE
  // Move constructor
	CoilData(CoilData&& m);
  
	// Move assignment
	CoilData& operator=(CoilData&& m);
#endif

  // Comparison operators
  bool operator==(const CoilData& m);
  bool operator!=(const CoilData& m);

  // Assignment of a bit image char array to re-init
  CoilData& operator=(const char *initVector);

  // If used as vector<uint8_t>, return a complete slice
  operator vector<uint8_t> const ();

  // slice: return a byte vector with coils shifted leftmost
  // will return empty vector if illegal parameters are detected
  // Default start is first coil, default length all to the end
  vector<uint8_t> slice(uint16_t start = 0, uint16_t length = 0);

  // operator[]: return value of a single coil
  bool operator[](uint16_t index) const;

  // set functions to change coil value(s)
  // Will return true if done, false if impossible (wrong address or data)
  // set #1: alter one single coil
  bool set(uint16_t index, bool value);
  // set #2: alter a group of coils, overwriting it by the bits from newValue
  bool set(uint16_t index, uint16_t length, vector<uint8_t> newValue);
  // set #3: alter a group of coils, overwriting it by the bits from unit8_t buffer newValue
  bool set(uint16_t index, uint16_t length, uint8_t *newValue);

  // (Re-)init complete coil set
  void init(bool value = false);

  // get size in coils
  inline const uint16_t coils() const { return CDsize; }

  // get size in bytes
  inline const uint8_t size() const { return CDbyteSize; }

  // get data as a vector<uint8_t>
  vector<uint8_t> const data();

  // Raw access to coil data buffer
  inline const uint8_t *bufferData() const { return CDbuffer; };
  inline const uint8_t bufferSize() const { return CDbyteSize; };

#if !ISLINUX
  // Helper function to dump out coils in logical order
  void print(const char *label, Print& s);
#endif

protected:
  // bit masks for bits left of a bit index in a byte
  const uint8_t CDfilter[8] = { 0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
  // Calculate byte index and bit index within that byte
  inline const uint8_t byteIndex(uint16_t index) const { return index >> 3; }
  inline const uint8_t bitIndex(uint16_t index) const { return index & 0x07; }
  // Calculate reversed bit sequence for a byte (taken from http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits)
  inline uint8_t reverseBits(uint8_t b) { return ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16; }
  // (Re-)init with bit image vector
  bool setVector(const char *initVector);

  uint16_t CDsize;         // Size of the CoilData store in bits
  uint8_t  CDbyteSize;     // Size in bytes
  uint8_t *CDbuffer;       // Pointer to bit storage
};

#endif
