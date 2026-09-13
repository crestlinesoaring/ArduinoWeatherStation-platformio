#pragma once

//   -D WS85_SERIAL_LOG  — WS85 anemometer frame dumps on Serial (requires ANEMO_WS85)

#include <Arduino.h>

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
void enableEthernet(bool quiet = false);
void disableEthernet(bool quiet = false);
void enableWifi(bool quiet = false);
void disableWifi(bool quiet = false);
void waitForWifi();

#ifdef NET_CONNECTIVITY_CHECKS
#ifndef NET_LINK_WAIT_MS
#define NET_LINK_WAIT_MS 3000
#endif
#ifndef NET_GATEWAY_PING_MS
#define NET_GATEWAY_PING_MS 2000
#endif
// Pre-upload checks (wxNetCheck.ino): link wait + ICMP gateway ping.
bool waitForEthernetLink(uint16_t timeoutMs = NET_LINK_WAIT_MS);
bool pingIp(const IPAddress& addr, uint16_t timeoutMs = NET_GATEWAY_PING_MS);
bool pingGateway(uint16_t timeoutMs = NET_GATEWAY_PING_MS);
#endif
