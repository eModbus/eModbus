// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusMessage.h"
#include "RTUutils.h"
// #undef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL_WARNING
#include "Logging.h"

// calcCRC: calculate Modbus CRC16 on a given array of bytes
uint16_t RTUutils::calcCRC(const uint8_t *data, uint16_t len) {
  // CRC16 pre-calculated tables
  const uint8_t crcHiTable[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40
  };

  const uint8_t crcLoTable[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
    0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
    0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
    0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
    0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
    0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
    0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
    0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
    0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
    0x40
  };

  register uint8_t crcHi = 0xFF;
  register uint8_t crcLo = 0xFF;
  register uint8_t index;

  while (len--) {
    index = crcLo ^ *data++;
    crcLo = crcHi ^ crcHiTable[index];
    crcHi = crcLoTable[index];
  }
  return (crcHi << 8 | crcLo);
}

// calcCRC: calculate Modbus CRC16 on a given message
uint16_t RTUutils::calcCRC(ModbusMessage msg) {
  return calcCRC(msg.data(), msg.size());
}


// validCRC #1: check the given CRC in a block of data for correctness
bool RTUutils::validCRC(const uint8_t *data, uint16_t len) {
  return validCRC(data, len - 2, data[len - 2] | (data[len - 1] << 8));
}

// validCRC #2: check the CRC of a block of data against a given one for equality
bool RTUutils::validCRC(const uint8_t *data, uint16_t len, uint16_t CRC) {
  uint16_t crc16 = calcCRC(data, len);
  if (CRC == crc16) return true;
  return false;
}

// validCRC #3: check the given CRC in a message for correctness
bool RTUutils::validCRC(ModbusMessage msg) {
  return validCRC(msg.data(), msg.size() - 2, msg[msg.size() - 2] | (msg[msg.size() - 1] << 8));
}

// validCRC #4: check the CRC of a message against a given one for equality
bool RTUutils::validCRC(ModbusMessage msg, uint16_t CRC) {
  return validCRC(msg.data(), msg.size(), CRC);
}

// addCRC: calculate the CRC for a given RTUMessage and add it to the end
void RTUutils::addCRC(ModbusMessage& raw) {
  uint16_t crc16 = calcCRC(raw.data(), raw.size());
  raw.push_back(crc16 & 0xff);
  raw.push_back((crc16 >> 8) & 0xFF);
}

// send: send a message via Serial, watching interval times - including CRC!
void RTUutils::send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, const uint8_t *data, uint16_t len) {
  uint16_t crc16 = calcCRC(data, len);
  while (micros() - lastMicros < interval) delayMicroseconds(1);  // respect _interval
  // Toggle rtsPin, if necessary
  if (rtsPin >= 0) digitalWrite(rtsPin, HIGH);
  // Write message
  serial.write(data, len);
  // Write CRC in LSB order
  serial.write(crc16 & 0xff);
  serial.write((crc16 >> 8) & 0xFF);
  serial.flush();
  // Toggle rtsPin, if necessary
  if (rtsPin >= 0) digitalWrite(rtsPin, LOW);

  HEXDUMP_D("Sent packet", data, len);

  // Mark end-of-message time for next interval
  lastMicros = micros();
}

// send: send a message via Serial, watching interval times - including CRC!
void RTUutils::send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, int rtsPin, ModbusMessage raw) {
  send(serial, lastMicros, interval, rtsPin, raw.data(), raw.size());
}

// receive: get (any) message from Serial, taking care of timeout and interval
ModbusMessage RTUutils::receive(HardwareSerial& serial, uint32_t timeout, uint32_t& lastMicros, uint32_t interval) {
  // Allocate initial receive buffer size: 1 block of BUFBLOCKSIZE bytes
  const uint16_t BUFBLOCKSIZE(128);
  uint8_t *buffer = new uint8_t[BUFBLOCKSIZE];
  uint8_t bufferBlocks = 1;
  ModbusMessage rv;

  // Index into buffer
  register uint16_t bufferPtr = 0;

  // State machine states
  enum STATES : uint8_t { WAIT_INTERVAL = 0, WAIT_DATA, IN_PACKET, DATA_READ, FINISHED };
  register STATES state = WAIT_INTERVAL;

  // Timeout tracker
  uint32_t TimeOut = millis();

  while (state != FINISHED) {
    switch (state) {
    // WAIT_INTERVAL: spend the remainder of the bus quiet time waiting
    case WAIT_INTERVAL:
      // Time passed?
      if (micros() - lastMicros >= interval) {
        // Yes, proceed to reading data
        state = WAIT_DATA;
      } else {
        // No, wait a little longer
        delayMicroseconds(1);
      }
      break;
    // WAIT_DATA: await first data byte, but watch timeout
    case WAIT_DATA:
      if (serial.available()) {
        state = IN_PACKET;
        lastMicros = micros();
      } else {
        if (millis() - TimeOut >= timeout) {
          rv.push_back(TIMEOUT);
          state = FINISHED;
        }
      }
      delay(1);
      break;
    // IN_PACKET: read data until a gap of at least _interval time passed without another byte arriving
    case IN_PACKET:
      // Data waiting and space left in buffer?
      while (serial.available()) {
        // Yes. Catch the byte
        buffer[bufferPtr++] = serial.read();
        // Buffer full?
        if (bufferPtr >= bufferBlocks * BUFBLOCKSIZE) {
          // Yes. Extend it by another block
          bufferBlocks++;
          uint8_t *temp = new uint8_t[bufferBlocks * BUFBLOCKSIZE];
          memcpy(temp, buffer, (bufferBlocks - 1) * BUFBLOCKSIZE);
          delete[] buffer;
          buffer = temp;
        }
        // Rewind timer
        lastMicros = micros();
      }
      // Gap of at least _interval micro seconds passed without data?
      // ***********************************************
      // Important notice!
      // Due to an implementation decision done in the ESP32 Arduino core code,
      // the correct time to detect a gap of _interval µs is not effective, as
      // the core FIFO handling takes much longer than that.
      //
      // Workaround: uncomment the following line to wait for 16ms(!) for the handling to finish:
      // if (micros() - lastMicros >= 16000) {
      //
      // Alternate solution: is to modify the uartEnableInterrupt() function in
      // the core implementation file 'esp32-hal-uart.c', to have the line
      //    'uart->dev->conf1.rxfifo_full_thrhd = 1; // 112;'
      // This will change the number of bytes received to trigger the copy interrupt
      // from 112 (as is implemented in the core) to 1, effectively firing the interrupt
      // for any single byte.
      // Then you may uncomment the line below instead:
      if (micros() - lastMicros >= interval) {
        state = DATA_READ;
      }
      break;
    // DATA_READ: successfully gathered some data. Prepare return object.
    case DATA_READ:
      // Did we get a sensible buffer length?
      if (bufferPtr >= 4)
      {
        // Yes. Allocate response object
        for (uint16_t i = 0; i < bufferPtr; ++i) {
          rv.push_back(buffer[i]);
        }
        state = FINISHED;
      } else {
        // No, packet was too short for anything usable. Return error
        rv.push_back(PACKET_LENGTH_ERROR);
        state = FINISHED;
      }
      break;
    // FINISHED: we are done, keep the compiler happy by pseudo-treating it.
    case FINISHED:
      break;
    }
  }
  // Deallocate buffer
  delete[] buffer;

  HEXDUMP_D("Received packet", rv.data(), rv.size());

  return rv;
}
