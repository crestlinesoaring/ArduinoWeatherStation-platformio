#pragma once

//   -D WS85_SERIAL_LOG   - WS85 anemometer frame dumps on Serial (requires ANEMO_WS85)
//   -D SERIAL_TIMESTAMPS - prefix every Serial output line with [HH:MM:SS] or [12345ms]

#include <Arduino.h>

#ifdef SERIAL_TIMESTAMPS
#include "wxTimestampSerial.h"
#undef Serial
#define Serial WxSerial
#endif

// Structured serial helpers (wxLog.ino)
void wxLogBlank();
void wxLogRule();
void wxLogSection(const __FlashStringHelper* title);
void wxLogTag(const __FlashStringHelper* tag, const __FlashStringHelper* msg);
void wxLogTag(const __FlashStringHelper* tag, const String& msg);
void wxLogTag(const __FlashStringHelper* tag, const char* msg);
void wxLogTagNum(const __FlashStringHelper* tag, const __FlashStringHelper* label, long value);
void wxLogTagFloat(const __FlashStringHelper* tag, const __FlashStringHelper* label, float value, uint8_t decimals = 2);

// Network (wxTimeNet.ino). quiet=true skips log (early boot before RTC is set).
bool resolveCssServerIp();
IPAddress getCssServerIp();
void enableEthernet(bool quiet = false);
void disableEthernet(bool quiet = false);
void enableWifi(bool quiet = false);
void disableWifi(bool quiet = false);
void waitForWifi();
