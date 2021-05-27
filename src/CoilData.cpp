// =================================================================================================
// eModbus: Copyright 2020, 2021 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================

#include "CoilData.h"
#undef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
#include "Logging.h"


// Constructor: mandatory size in bits, optional initial value for all bits
// Maximum size is 2000 coils (=250 bytes)
CoilData::CoilData(uint16_t size, bool initValue) :
  CDsize(0),
  CDbyteSize(0), 
  CDbuffer(nullptr) {
  // Limit the size to 2000 (Modbus rules)
  if (size > 2000) size = 2000;
  // At least the size shall be 1
  if (size == 0) size = 1;
  // Calculate number of bytes needed
  CDbyteSize = byteIndex(size) + 1;
  // Allocate and init buffer
  CDbuffer = new uint8_t[CDbyteSize];
  memset(CDbuffer, initValue ? 0xFF : 0, CDbyteSize);
  if (initValue && (size % 8)) {
    CDbuffer[CDbyteSize - 1] &= ~CDfilter[8 - (size % 8)];
  }
  CDsize = size;
}

// Alternate constructor, taking a "1101..." bit image char array to init
CoilData::CoilData(const char *initVector) :
  CDsize(0),
  CDbyteSize(0), 
  CDbuffer(nullptr) {
  set(initVector);
}

// Destructor: take care of cleaning up
CoilData::~CoilData() {
  if (CDbuffer) {
    delete CDbuffer;
  }
}

// Assignment operator
CoilData& CoilData::operator=(const CoilData& m) {
  // Remove old data
  if (CDbuffer) {
    delete CDbuffer;
  }
  // Allocate new buffer and copy data
  CDbuffer = new uint8_t[m.CDbyteSize];
  memcpy(CDbuffer, m.CDbuffer, m.CDbyteSize);
  CDsize = m.CDsize;
  CDbyteSize = m.CDbyteSize;
  return *this;
}

// Copy constructor
CoilData::CoilData(const CoilData& m) {
  CDbuffer = new uint8_t[m.CDbyteSize];
  memcpy(CDbuffer, m.CDbuffer, m.CDbyteSize);
  CDsize = m.CDsize;
  CDbyteSize = m.CDbyteSize;
}

#ifndef NO_MOVE
// Move constructor
CoilData::CoilData(CoilData&& m) {
  // Copy all data
  CDbuffer = m.CDbuffer;
  CDsize = m.CDsize;
  CDbyteSize = m.CDbyteSize;
  // Then clear source
  m.CDbuffer = nullptr;
  m.CDsize = 0;
  m.CDbyteSize = 0;
}

// Move assignment
CoilData& CoilData::operator=(CoilData&& m) {
  // Remove buffer, if already allocated
  if (CDbuffer) {
    delete CDbuffer;
  }
  // Copy over all data
  CDbuffer = m.CDbuffer;
  CDsize = m.CDsize;
  CDbyteSize = m.CDbyteSize;
  // Then clear source
  m.CDbuffer = nullptr;
  m.CDsize = 0;
  m.CDbyteSize = 0;
  return *this;
}
#endif

// Comparison operators
bool CoilData::operator==(const CoilData& m) {
  // Self-compare is always true
  if (this == &m) return true;
  // Different sizes are never equal
  if (CDsize != m.CDsize) return false;
  // Compare the data
  if (memcmp(CDbuffer, m.CDbuffer, CDbyteSize)) return false;
  return true;
}

bool CoilData::operator!=(const CoilData& m) {
  return !(*this == m);
}

// Assignment of a bit image char array to re-init
bool CoilData::operator=(const char *initVector) {
  return set(initVector);
}

// If used as vector<uint8_t>, return a complete slice
CoilData::operator vector<uint8_t> const () {
  return slice();
}

// slice: return a byte vector with coils shifted leftmost
// will return empty vector if illegal parameters are detected
vector<uint8_t> CoilData::slice(uint16_t start, uint16_t length) {
  vector<uint8_t> retval;

  // length default is all up to the end
  if (length == 0) length = CDsize - start;

  // Does the requested slice fit in the buffer?
  if ((start + length) <= CDsize) {
    // Yes, it does. Prepare return vector.
    uint8_t retbytes = byteIndex(length - 1) + 1;
    retval.reserve(retbytes);

    uint8_t work = 0;           // Byte to collect bits for the result
    uint8_t bitPtr = 0;         // bit pointer within "work"

  // Loop over all requested bits
    for (uint16_t i = start; i < start + length; ++i) {
      // Is bit set?
      if (CDbuffer[byteIndex(i)] & (1 << bitIndex(i))) {
        // Yes, add a 1 bit to the result
        work |= (1 << bitPtr);
      }
      // Proceed to next bit
      bitPtr++;
      // work filled completely?
      if (bitPtr >= 8) {
        // Yes. Move it to the result
        retval.push_back(work);
        // Re-init work  and pointer
        bitPtr = 0;
        work = 0;
      }
    }
    // Are there leftovers?
    if (bitPtr) {
      // Yes. Move these to the result.
      retval.push_back(work);
    }
  }
  return retval;
}

// Single coil get/set functions are using the unreversed bytes. 
// operator[]: return value of a single coil
bool CoilData::operator[](uint16_t index) const {
  if (index && index < CDsize) {
    return (CDbuffer[byteIndex(index)] & (1 << bitIndex(index))) ? true : false;
  }
  // Wrong parameter -> always return false
  return false;
}

// set functions to change coil value(s)
// Will return true if done, false if impossible (wrong address or data)

// set #1: alter one single coil
bool CoilData::set(uint16_t index, bool value) {
  // Within coils?
  if (index && index < CDsize) {
    // Yes. Determine affected byte and bit therein
    uint16_t by = byteIndex(index);
    uint8_t mask = 1 << bitIndex(index);

    // Stamp out bit
    CDbuffer[by] &= ~mask;
    // If required, set it to 1 now
    if (value) {
      CDbuffer[by] |= mask;
    }
    return true;
  }
  // Wrong parameter -> always return false
  return false;
}

// set #2: alter a group of coils, overwriting it by the bits from vector newValue
bool CoilData::set(uint16_t start, uint16_t length, vector<uint8_t> newValue) {
  return set(start, length, newValue.data());
}

// set #3: alter a group of coils, overwriting it by the bits from uint8_t buffer newValue
bool CoilData::set(uint16_t start, uint16_t length, uint8_t *newValue) {
  // Does the requested slice fit in the buffer?
  if (length && (start + length) <= CDsize) {
    // Yes, it does. 
    // Prepare pointers to the source byte and the bit within
    uint8_t *cp = newValue;
    uint8_t bitPtr = 0;

    // Loop over all bits to be set
    for (uint16_t i = start; i < start + length; i++) {
      // Get affected byte
      uint8_t by = byteIndex(i);
      // Calculate single-bit mask in target byte
      uint8_t mask = 1 << bitIndex(i);
      // Stamp out bit
      CDbuffer[by] &= ~mask;
      // is source bit set?
      if (*cp & (1 << bitPtr)) {
        // Yes. Set it in target as well
        CDbuffer[by] |= mask;
      }
      // Advance source bit ptr
      bitPtr++;
      // Overflow?
      if (bitPtr >= 8) {
        // Yes. move pointers to first bit in next source byte
        bitPtr = 0;
        cp++;
      }
    }
    return true;
  }
  return false;
}

// Init all coils by a readable bit image array
bool CoilData::set(const char *initVector) {
  uint16_t length = 0;           // resulting bit pattern length
  const char *cp = initVector;   // pointer to source array
  bool skipFlag = false;         // Signal next character irrelevant

  // Do a first pass to count all valid bits in array
  while (*cp) {
    switch (*cp) {
    case '1':  // A valid 1 bit
    case '0':  // A valid 0 bit
      // Shall we ignore it?
      if (skipFlag) {
        // Yes. just reset the ignore flag
        skipFlag = false;
      } else {
        // No, we can count it
        length++;
      }
      break;
    case '_':  // Skip next
      skipFlag = true;
      break;
    default:  // anything else
      skipFlag = false;
      break;
    }
    cp++;
  }

  // Did we count a manageable number?
  if (length && length <= 2000) {
    // Yes. Init the coils
    CDsize = length;
    CDbyteSize = byteIndex(length - 1) + 1;
    // If there are coils already, trash them.
    if (CDbuffer) {
      delete CDbuffer;
    }
    // Allocate new coil storage
    CDbuffer = new uint8_t[CDbyteSize];
    memset(CDbuffer, 0, CDbyteSize);

    // Prepare second loop
    uint16_t ptr = 0;    // bit pointer in coil storage
    skipFlag = false;
    cp = initVector;

    // Do a second pass, converting 1 and 0 into coils (bits)
    // loop as above, only difference is setting the bits
    while (*cp) {
      switch (*cp) {
      case '1':
      case '0':
        if (skipFlag) {
          skipFlag = false;
        } else {
          // Do we have a 1 bit here?
          if (*cp == '1') {
            // Yes. set it in coil storage
            CDbuffer[byteIndex(ptr)] |= (1 << bitIndex(ptr));
          }
          // advance bit pointer in any case 0 or 1
          ptr++;
        }
        break;
      case '_':
        skipFlag = true;
        break;
      default:
        skipFlag = false;
        break;
      }
      cp++;
    }
  } else { 
    return false;
  }
  return true;
}

// init: set all coils to 1 or 0 (default)
void CoilData::init(bool value) {
  memset(CDbuffer, value ? 0xFF : 0, CDbyteSize);
  // Stamp out overhang bits
  CDbuffer[CDbyteSize - 1] &= ~CDfilter[8 - (CDsize % 8)];
}

#if !IS_LINUX
// Not for Linux for the Print reference!

// Print out a coil storage in readable form to ease debugging
void CoilData::print(const char *label, Print& s) {
  uint8_t bitptr = 0;
  uint8_t labellen = strlen(label);
  uint8_t pos = labellen;

  // Put out the label
  s.print(label);

  // Print out all coils as "1" or "0"
  for (uint16_t i = 0; i < CDsize; ++i) {
    s.print((CDbuffer[byteIndex(i)] & (1 << bitptr)) ? "1" : "0");
    pos++;
    // Have a blank after every group of 4
    if (i % 4 == 3) {
      // Have a line break if > 80 characters, including the last group of 4
      if (pos >= 80) {
        s.println("");
        pos = 0;
        // Leave a nice empty space below the label
        while (pos++ < labellen) {
          s.print(" ");
        }
      } else {
        s.print(" ");
        pos++;
      }
    }
    bitptr++;
    bitptr &= 0x07;
  }
  s.println("");
}
#endif
