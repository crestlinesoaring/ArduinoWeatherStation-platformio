#include "renogy.h"
#include <ModbusMaster.h>

// Renogy Wanderer RS232/Modbus on Mega Serial2 (9600 baud, slave address 255).
static ModbusMaster renogyNode;
static const uint8_t RENOGY_MODBUS_ADDRESS = 255;
static const uint32_t RENOGY_DATA_REGISTER_COUNT = 9; // through solar amps at index 8
static const unsigned long RENOGY_POLL_INTERVAL_MS = 1000;

float renogy_solar_volts = 0;
float renogy_solar_amps = 0;
bool renogy_connected = false;

static float minuteVoltSum = 0;
static float minuteAmpSum = 0;
static uint16_t minuteSampleCount = 0;
static unsigned long lastPollMillis = 0;

static bool renogyReadOnce(float &volts, float &amps) {
  uint8_t result = renogyNode.readHoldingRegisters(0x100, RENOGY_DATA_REGISTER_COUNT);
  if (result == renogyNode.ku8MBSuccess) {
    volts = renogyNode.getResponseBuffer(7) * 0.1f;
    amps = renogyNode.getResponseBuffer(8) * 0.01f;
    return true;
  }
  return false;
}

void renogyInit() {
  Serial2.begin(9600);
  renogyNode.begin(RENOGY_MODBUS_ADDRESS, Serial2);
}

void renogyPoll() {
  if (lastPollMillis + RENOGY_POLL_INTERVAL_MS > millis()) return;
  lastPollMillis = millis();

  float volts = 0;
  float amps = 0;
  if (renogyReadOnce(volts, amps)) {
    minuteVoltSum += volts;
    minuteAmpSum += amps;
    minuteSampleCount++;
  }
}

void renogyFinalizeMinute() {
  if (minuteSampleCount > 0) {
    renogy_solar_volts = minuteVoltSum / minuteSampleCount;
    renogy_solar_amps = minuteAmpSum / minuteSampleCount;
    renogy_connected = true;
  } else {
    renogy_solar_volts = 0;
    renogy_solar_amps = 0;
    renogy_connected = false;
  }

  minuteVoltSum = 0;
  minuteAmpSum = 0;
  minuteSampleCount = 0;
}
