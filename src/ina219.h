#pragma once

// Seconds between INA219 reads (each read is 128-sample hardware averaged).
// Override in platformio.ini: -D INA_READ_INTERVAL_SEC=2
#ifndef INA_READ_INTERVAL_SEC
#define INA_READ_INTERVAL_SEC 1
#endif

void ina219Poll();
void ina219ReadNow();  // One immediate sample (e.g. after setup) for lowest-voltage tracking.
void finalizeInaMinute();
