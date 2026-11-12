#pragma once

// Seconds between INA219 reads (each read is 128-sample hardware averaged).
// Override in platformio.ini: -D INA_READ_INTERVAL_SEC=2
#ifndef INA_READ_INTERVAL_SEC
#define INA_READ_INTERVAL_SEC 1
#endif

// Do not refresh minute-mean globals used in [WX] until we have at least this many polls
// (avoids 0000 mA from a single bad read after upload). Override in platformio.ini if needed.
#ifndef INA_MIN_SAMPLES_PER_MINUTE
#define INA_MIN_SAMPLES_PER_MINUTE 3
#endif

void ina219Poll();
void ina219ReadNow();  // One immediate sample (e.g. after setup) for lowest-voltage tracking.
void finalizeInaMinute();
