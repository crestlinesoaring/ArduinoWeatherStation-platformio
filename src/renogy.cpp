#include "renogy.h"
#include <ModbusMaster.h>
#include <avr/wdt.h>

// Renogy RS232/Modbus on Mega Serial2 (9600 baud, slave address 255).
// Holding registers 0x100..0x108:
//   0x100 SOC, 0x101 battery V, 0x102 charge A, 0x103 temperature,
//   0x104 load V, 0x105 load A, 0x106 load W,
//   0x107 solar V, 0x108 solar A.
static ModbusMaster renogyNode;
static const uint8_t RENOGY_MODBUS_ADDRESS = 255;
static const uint32_t RENOGY_DATA_REGISTER_COUNT = 9;
static const unsigned long RENOGY_POLL_INTERVAL_MS = 1000;
static const uint8_t RENOGY_BOOT_PROBE_ATTEMPTS = 5;
static const unsigned long RENOGY_BOOT_PROBE_GAP_MS = 500;
static const unsigned long RENOGY_BOOT_INITIAL_SETTLE_MS = 200;

float renogy_solar_volts = 0;
float renogy_solar_amps = 0;
float renogy_load_volts = 0;
float renogy_load_amps = 0;
bool renogy_connected = false;

static float minuteVoltSum = 0;
static uint16_t minuteVoltCount = 0;
static float minuteAmpSum = 0;
static uint16_t minuteAmpCount = 0;
static float minuteLoadVoltSum = 0;
static uint16_t minuteLoadVoltCount = 0;
static float minuteLoadAmpSum = 0;
static uint16_t minuteLoadAmpCount = 0;
static unsigned long lastPollMillis = 0;
static bool renogyUsable = false;

struct RenogySample {
  float solarVolts;
  float solarAmps;
  float loadVolts;
  float loadAmps;
};

static bool renogyReadOnce(RenogySample &sample) {
  uint8_t result = renogyNode.readHoldingRegisters(0x100, RENOGY_DATA_REGISTER_COUNT);
  if (result == renogyNode.ku8MBSuccess) {
    sample.solarVolts = renogyNode.getResponseBuffer(7) * 0.1f;
    sample.solarAmps = renogyNode.getResponseBuffer(8) * 0.01f;
    sample.loadVolts = renogyNode.getResponseBuffer(4) * 0.1f;
    sample.loadAmps = renogyNode.getResponseBuffer(5) * 0.01f;
    return true;
  }
  return false;
}

static void renogyDelayBoot(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    wdt_reset();
    delay(50);
  }
}

static bool renogyProbeAtBoot() {
  for (uint8_t attempt = 0; attempt < RENOGY_BOOT_PROBE_ATTEMPTS; attempt++) {
    if (attempt > 0) {
      renogyDelayBoot(RENOGY_BOOT_PROBE_GAP_MS);
    }
    RenogySample sample = {};
    if (renogyReadOnce(sample)) {
      return true;
    }
    wdt_reset();
  }
  return false;
}

bool renogyInit() {
#if !RENOGY_POLLING_ENABLED
  renogyUsable = false;
  return false;
#endif
  Serial2.begin(9600);
  renogyNode.begin(RENOGY_MODBUS_ADDRESS, Serial2);
  renogyDelayBoot(RENOGY_BOOT_INITIAL_SETTLE_MS);

  renogyUsable = renogyProbeAtBoot();
  return renogyUsable;
}

void renogyPoll() {
#if !RENOGY_POLLING_ENABLED
  return;
#endif
  if (!renogyUsable) return;
  if (lastPollMillis + RENOGY_POLL_INTERVAL_MS > millis()) return;
  lastPollMillis = millis();

  RenogySample sample = {};
  if (!renogyReadOnce(sample)) return;

  minuteVoltSum += sample.solarVolts;
  minuteVoltCount++;
  minuteAmpSum += sample.solarAmps;
  minuteAmpCount++;
  minuteLoadVoltSum += sample.loadVolts;
  minuteLoadVoltCount++;
  minuteLoadAmpSum += sample.loadAmps;
  minuteLoadAmpCount++;
}

void renogyFinalizeMinute() {
#if !RENOGY_POLLING_ENABLED
  renogy_solar_volts = 0;
  renogy_solar_amps = 0;
  renogy_load_volts = 0;
  renogy_load_amps = 0;
  renogy_connected = false;
  return;
#endif
  if (!renogyUsable) {
    renogy_solar_volts = 0;
    renogy_solar_amps = 0;
    renogy_load_volts = 0;
    renogy_load_amps = 0;
    renogy_connected = false;
    return;
  }
  if (minuteVoltCount > 0) {
    renogy_solar_volts = minuteVoltSum / minuteVoltCount;
    renogy_connected = true;
  } else {
    renogy_solar_volts = 0;
    renogy_connected = false;
  }

  if (minuteAmpCount > 0) {
    renogy_solar_amps = minuteAmpSum / minuteAmpCount;
  } else {
    renogy_solar_amps = 0;
  }

  if (minuteLoadVoltCount > 0) {
    renogy_load_volts = minuteLoadVoltSum / minuteLoadVoltCount;
  } else {
    renogy_load_volts = 0;
  }

  if (minuteLoadAmpCount > 0) {
    renogy_load_amps = minuteLoadAmpSum / minuteLoadAmpCount;
  } else {
    renogy_load_amps = 0;
  }

  minuteVoltSum = 0;
  minuteVoltCount = 0;
  minuteAmpSum = 0;
  minuteAmpCount = 0;
  minuteLoadVoltSum = 0;
  minuteLoadVoltCount = 0;
  minuteLoadAmpSum = 0;
  minuteLoadAmpCount = 0;
}
