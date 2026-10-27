/*
 * Standalone Renogy Wanderer Modbus check on Mega Serial2.
 * Build/upload: pio run -e renogy_test -t upload
 * Monitor:      pio device monitor -e renogy_test
 */

#include <Arduino.h>
#include <ModbusMaster.h>

static const uint8_t RENOGY_MODBUS_ADDRESS = 255;
static const uint16_t RENOGY_FIRST_REG = 0x0100;
static const uint8_t RENOGY_REG_COUNT = 9;
static const unsigned long POLL_INTERVAL_MS = 1000;
static const uint8_t BOOT_PROBE_ATTEMPTS = 5;
static const unsigned long BOOT_PROBE_GAP_MS = 500;
static const unsigned long BOOT_SETTLE_MS = 200;

static ModbusMaster renogyNode;

static const __FlashStringHelper *modbusResultName(uint8_t code) {
  switch (code) {
    case 0x00: return F("success");
    case 0x01: return F("illegal function");
    case 0x02: return F("illegal data address");
    case 0x03: return F("illegal data value");
    case 0x04: return F("slave device failure");
    case 0xE0: return F("invalid slave ID");
    case 0xE1: return F("invalid function");
    case 0xE2: return F("response timed out");
    case 0xE3: return F("invalid CRC");
    default: return F("unknown");
  }
}

static bool renogyReadOnce(float &volts, float &amps, uint8_t &resultCode) {
  resultCode = renogyNode.readHoldingRegisters(RENOGY_FIRST_REG, RENOGY_REG_COUNT);
  if (resultCode == renogyNode.ku8MBSuccess) {
    volts = renogyNode.getResponseBuffer(7) * 0.1f;
    amps = renogyNode.getResponseBuffer(8) * 0.01f;
    return true;
  }
  return false;
}

static bool probeAtBoot() {
  Serial.println(F("[renogy-test] boot probe..."));
  for (uint8_t attempt = 1; attempt <= BOOT_PROBE_ATTEMPTS; attempt++) {
    if (attempt > 1) {
      delay(BOOT_PROBE_GAP_MS);
    }
    float volts = 0;
    float amps = 0;
    uint8_t code = 0;
    if (renogyReadOnce(volts, amps, code)) {
      Serial.print(F("[renogy-test] probe OK on attempt "));
      Serial.print(attempt);
      Serial.print(F("  solar="));
      Serial.print(volts, 1);
      Serial.print(F(" V  "));
      Serial.print(amps, 2);
      Serial.println(F(" A"));
      return true;
    }
    Serial.print(F("[renogy-test] attempt "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.print(BOOT_PROBE_ATTEMPTS);
    Serial.print(F(" failed: "));
    Serial.print(modbusResultName(code));
    Serial.print(F(" (0x"));
    if (code < 16) Serial.print(F("0"));
    Serial.println(code, HEX);
  }
  Serial.println(F("[renogy-test] probe FAILED — check power, wiring, addr 255, 9600 baud"));
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

  Serial.println();
  Serial.println(F("[renogy-test] Renogy Modbus on Serial2 @ 9600, slave 255"));
  Serial2.begin(9600);
  renogyNode.begin(RENOGY_MODBUS_ADDRESS, Serial2);
  delay(BOOT_SETTLE_MS);

  probeAtBoot();
  Serial.println(F("[renogy-test] polling every 1s (Ctrl+C monitor to stop)"));
}

void loop() {
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < POLL_INTERVAL_MS) {
    return;
  }
  lastPoll = millis();

  float volts = 0;
  float amps = 0;
  uint8_t code = 0;
  if (renogyReadOnce(volts, amps, code)) {
    Serial.print(F("[renogy-test] OK  solar="));
    Serial.print(volts, 1);
    Serial.print(F(" V  "));
    Serial.print(amps, 2);
    Serial.println(F(" A"));
  } else {
    Serial.print(F("[renogy-test] ERR "));
    Serial.print(modbusResultName(code));
    Serial.print(F(" (0x"));
    if (code < 16) Serial.print(F("0"));
    Serial.println(code, HEX);
  }
}
