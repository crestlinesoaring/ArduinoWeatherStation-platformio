#include "ina219.h"

extern Adafruit_INA219_5A ina219a_solar;
extern Adafruit_INA219_5A ina219b_battery;
extern float battery_ma_offset;

extern float ina219a_solar_volts;
extern float ina219a_solar_ma;
extern float ina219b_battery_volts;
extern float ina219b_battery_ma;
extern float voltsLowestSeen;

unsigned int ina219_sampleCount = 0;
unsigned int ina219_samplesLastMinute = 0;
float ina219_avgReadHz = 0;

static unsigned long ina219_lastReadMs = 0;
static float ina219_solarVoltSum = 0;
static float ina219_solarMaSum = 0;
static float ina219_battVoltSum = 0;
static float ina219_battMaSum = 0;

static const unsigned long INA_READ_INTERVAL_MS =
    (unsigned long)INA_READ_INTERVAL_SEC * 1000UL;

static void applyInaGlobals(float solarV, float solarMa, float battV, float battMa) {
#ifdef SIMULATE_INA219A_SOLAR_VOLTS
  ina219a_solar_volts = SIMULATE_INA219A_SOLAR_VOLTS;
#else
  ina219a_solar_volts = solarV;
#endif
#ifdef SIMULATE_INA219A_SOLAR_MA
  ina219a_solar_ma = SIMULATE_INA219A_SOLAR_MA;
#else
  ina219a_solar_ma = solarMa;
#endif
#ifdef SIMULATE_INA219B_BATTERY_VOLTS
  ina219b_battery_volts = SIMULATE_INA219B_BATTERY_VOLTS;
#else
  ina219b_battery_volts = battV;
#endif
#ifdef SIMULATE_INA219B_BATTERY_MA
  ina219b_battery_ma = SIMULATE_INA219B_BATTERY_MA;
#else
  ina219b_battery_ma = battMa;
#endif
}

static void ina219TakeSample() {
  float ina219a_solar_polarity = 1.0f;

  float shuntMv = ina219a_solar.getShuntVoltage_mV();
  float busV = ina219a_solar.getBusVoltage_V();
  float solarV = busV + (shuntMv / 1000.0f);
  if (solarV < 0.0f) {
    solarV = -solarV;
    ina219a_solar_polarity = -1.0f;
  }
  float solarMa = ina219a_solar.getCurrent_mA() * ina219a_solar_polarity;

  shuntMv = ina219b_battery.getShuntVoltage_mV();
  busV = ina219b_battery.getBusVoltage_V();
  float battV = busV + (shuntMv / 1000.0f);
  float battMa = ina219b_battery.getCurrent_mA() + battery_ma_offset;

  ina219_solarVoltSum += solarV;
  ina219_solarMaSum += solarMa;
  ina219_battVoltSum += battV;
  ina219_battMaSum += battMa;
  ina219_sampleCount++;

  if ((battV < voltsLowestSeen) && (battV > 2.0f)) {
    voltsLowestSeen = battV;
  }

  float n = (float)ina219_sampleCount;
  applyInaGlobals(
      ina219_solarVoltSum / n,
      ina219_solarMaSum / n,
      ina219_battVoltSum / n,
      ina219_battMaSum / n);
}

void ina219Poll() {
  unsigned long now = millis();
  if (now - ina219_lastReadMs < INA_READ_INTERVAL_MS) {
    return;
  }
  ina219_lastReadMs = now;
  ina219TakeSample();
}

void finalizeInaMinute() {
  if (ina219_sampleCount > 0) {
    float n = (float)ina219_sampleCount;
    applyInaGlobals(
        ina219_solarVoltSum / n,
        ina219_solarMaSum / n,
        ina219_battVoltSum / n,
        ina219_battMaSum / n);
  }

  ina219_samplesLastMinute = ina219_sampleCount;
  ina219_avgReadHz = ina219_samplesLastMinute / 60.0f;

  Serial.print(F("[INA] interval "));
  Serial.print(INA_READ_INTERVAL_SEC);
  Serial.print(F("s, samples/min: "));
  Serial.print(ina219_samplesLastMinute);
  Serial.print(F(", rate: "));
  Serial.print(ina219_avgReadHz, 2);
  Serial.println(F(" Hz"));

  ina219_solarVoltSum = 0;
  ina219_solarMaSum = 0;
  ina219_battVoltSum = 0;
  ina219_battMaSum = 0;
  ina219_sampleCount = 0;
}
