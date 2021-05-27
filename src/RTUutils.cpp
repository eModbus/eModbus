// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "options.h"
#include "ModbusMessage.h"
#include "RTUutils.h"
#undef LOCAL_LOG_LEVEL
// #define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
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

// calculateInterval: determine the minimal gap time between messages
uint32_t RTUutils::calculateInterval(HardwareSerial& s, uint32_t overwrite) {
  uint32_t interval = 0;

  // silent interval is at least 3.5x character time
  interval = 35000000UL / s.baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud
  if (interval < 1750) interval = 1750;       // lower limit according to Modbus RTU standard
  // User overwrite?
  if (overwrite > interval) {
    interval = overwrite;
  }
  return interval;
}

// UARTinit: modify the UART FIFO copy trigger threshold 
// This is normally set to 112 by default, resulting in short messages not being 
// recognized fast enough for higher Modbus bus speeds
// Our default is 1 - every single byte arriving will have the UART FIFO
// copied to the serial buffer.
int RTUutils::UARTinit(HardwareSerial& serial, int thresholdBytes) {
  int rc = 0;
#if NEED_UART_PATCH
  // Is the threshold value valid? The UART FIFO is 128 bytes only
  if (thresholdBytes > 0 && thresholdBytes < 128) {
    // Yes, it is. Try to identify the Serial/Serial1/Serial2 the user has provided.
    // Is it Serial (UART0)?
    if (&serial == &Serial) {
      // Yes. get the current value and set ours instead
      rc = UART0.conf1.rxfifo_full_thrhd;
      UART0.conf1.rxfifo_full_thrhd = thresholdBytes;
      LOG_D("Serial FIFO threshold set to %d (was %d)\n", thresholdBytes, rc);
    // No, but perhaps is Serial1?
    } else if (&serial == &Serial1) {
      // It is. Get the current value and set ours.
      rc = UART1.conf1.rxfifo_full_thrhd;
      UART1.conf1.rxfifo_full_thrhd = thresholdBytes;
      LOG_D("Serial1 FIFO threshold set to %d (was %d)\n", thresholdBytes, rc);
    // No, but it may be Serial2
    } else if (&serial == &Serial2) {
      // Found it. Get the current value and set ours.
      rc = UART2.conf1.rxfifo_full_thrhd;
      UART2.conf1.rxfifo_full_thrhd = thresholdBytes;
      LOG_D("Serial2 FIFO threshold set to %d (was %d)\n", thresholdBytes, rc);
    // None of the three, so we are at an end here
    } else {
      LOG_W("Unable to identify serial\n");
    }
  } else {
    Serial.printf("Threshold must be between 1 and 127! (was %d)", thresholdBytes);
  }
#endif
  // Return the previous value in case someone likes to see it.
  return rc;
}

// send: send a message via Serial, watching interval times - including CRC!
void RTUutils::send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, RTScallback rts, const uint8_t *data, uint16_t len) {
  uint16_t crc16 = calcCRC(data, len);

  // Clear serial buffers
  while (serial.available()) serial.read();

  // Respect interval
  if (micros() - lastMicros < interval) delayMicroseconds(interval - (micros() - lastMicros));

  // Toggle rtsPin, if necessary
  rts(HIGH);
  // Write message
  serial.write(data, len);
  // Write CRC in LSB order
  serial.write(crc16 & 0xff);
  serial.write((crc16 >> 8) & 0xFF);
  serial.flush();
  // Toggle rtsPin, if necessary
  rts(LOW);

  HEXDUMP_D("Sent packet", data, len);

  // Mark end-of-message time for next interval
  lastMicros = micros();
}

// send: send a message via Serial, watching interval times - including CRC!
void RTUutils::send(HardwareSerial& serial, uint32_t& lastMicros, uint32_t interval, RTScallback rts, ModbusMessage raw) {
  send(serial, lastMicros, interval, rts, raw.data(), raw.size());
}

// receive: get (any) message from Serial, taking care of timeout and interval
ModbusMessage RTUutils::receive(HardwareSerial& serial, uint32_t timeout, uint32_t& lastMicros, uint32_t interval) {
  // Allocate initial receive buffer size: 1 block of BUFBLOCKSIZE bytes
  const uint16_t BUFBLOCKSIZE(512);
  uint8_t *buffer = new uint8_t[BUFBLOCKSIZE];
  ModbusMessage rv;

  // Index into buffer
  register uint16_t bufferPtr = 0;
  // Byte read
  register int b; 
  // Flag for successful read cycle
  bool hadBytes = false;
  // Next buffer limit

  // State machine states
  enum STATES : uint8_t { WAIT_DATA = 0, IN_PACKET, DATA_READ, FINISHED };
  register STATES state = WAIT_DATA;

  // Timeout tracker
  uint32_t TimeOut = millis();

  // interval tracker 
  uint32_t intervalEnd = micros();

  while (state != FINISHED) {
    switch (state) {
    // WAIT_DATA: await first data byte, but watch timeout
    case WAIT_DATA:
      if (serial.available()) {
        state = IN_PACKET;
        intervalEnd = micros();
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
      hadBytes = false;
      b = serial.read();
      while (b >= 0) {
        buffer[bufferPtr++] = b;
        // Buffer full?
        if (bufferPtr >= BUFBLOCKSIZE) {
          // Yes. Something fishy here - bail out!
          // Most probably we will run into an error with this data, but anyway...
          break;
        }
        hadBytes = true;
        b = serial.read();
      }
      // Did we read some?
      if (hadBytes) {
        // Yes, take another turn
        intervalEnd = micros();
        taskYIELD();
        // delay(1);
      } else {
        // No. Has a complete interval passed without data?
        lastMicros = micros();
        if (lastMicros - intervalEnd >= interval) {
          // Yes, go processing data
          state = DATA_READ;
        }
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
    // FINISHED: we are done, clean up.
    case FINISHED:
      // CLear serial buffer in case something is left trailing
      // May happen with servers too slow!
      while (serial.available()) serial.read();
      break;
    }
  }
  // Deallocate buffer
  delete[] buffer;

  HEXDUMP_D("Received packet", rv.data(), rv.size());

  return rv;
}
