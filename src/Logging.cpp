// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "Logging.h"

int MBUlogLvl = LOG_LEVEL;
Print *LOGDEVICE = &Serial;

void logHexDump(Print& output, const char *letter, const char *label, const uint8_t *data, const size_t length) {
  size_t cnt = 0;
  size_t step = 0;
  char limiter = '|';
  char ascbuf[17];
       ascbuf[16] = 0;

  // Print out header
  output.printf("[%s] %s: @%08X/%d:\n", letter, label, (uint32_t)data, length);

  // loop over data in steps of 16
  for (cnt = 0; cnt < length; ++cnt) {
    step = cnt % 16;
    // New line?
    if (step == 0) {
      // Yes. Print header and clear ascii 
      output.printf("  %c %04X: ", limiter, cnt);
      memset(ascbuf, ' ', 16);
      // No, but first block of 8 done?
    } else if (step == 8) {
      // Yes, put out additional space
      output.print(' ');
    }
    // Print data byte
    uint8_t c = data[cnt];
    output.printf("%02X ", c);
    if (c >= 32 && c <= 127) ascbuf[cnt % 16] = c;
    else                     ascbuf[cnt % 16] = '.';
    // Line end?
    if (step == 15) {
      // Yes, print ascii
      output.printf(" %c%s%c\n", limiter, ascbuf, limiter);
    }
  }
  // Unfinished line?
  if (length && step != 15) {
    for (uint8_t i = step; i < 15; ++i) {
      output.print("   ");
    }
    if (step < 8) output.print(' ');
    output.printf(" %c%s%c\n", limiter, ascbuf, limiter);
  }
}
