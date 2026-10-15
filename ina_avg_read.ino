/*
 * INA219  --  0.0083333 ohm (8.3333 mOhm) shunt
 * Cal = 33665  ->  Current_LSB = 0.000146 A (146 uA/bit), max ~4.784 A
 * 128-sample internal averaging, 12-bit ADC, both bus voltage & current
 * 10 measurement loops -> 10 individually named result variables
 *
 * Board: Arduino MEGA 2560
 * Wiring:
 *   VCC -> 3.3V or 5V,  GND -> GND
 *   SDA -> pin 20,  SCL -> pin 21   (Mega dedicated I2C pins)
 *   Shunt resistor between VIN+ and VIN-
 *   A0 -> GND/A1 -> 5V  (I2C address 0x41)// jjj
 */

#include <Wire.h>

#define INA219_ADDR   0x41  //jjj

// Registers
#define REG_CONFIG      0x00
#define REG_SHUNT_V     0x01
#define REG_BUS_V       0x02
#define REG_POWER       0x03
#define REG_CURRENT     0x04
#define REG_CALIBRATION 0x05

// ---------- hardware constants ----------
const float    SHUNT_OHMS   = 0.0083333f;  // 8.3333 mOhm shunt
const uint16_t CALIBRATION  = 33665;       // given
const float    CURRENT_LSB  = 0.000146f;   // A/bit  (= 0.04096 / (33665 * 0.0083333))

const float CURRENT_FULL_SCALE_A = CURRENT_LSB * 32768.0f;   // ~4.784 A

// ---------- 10 individually named result variables ----------
float voltage_1,  voltage_2,  voltage_3,  voltage_4,  voltage_5;
float voltage_6,  voltage_7,  voltage_8,  voltage_9,  voltage_10;
float voltage_11,  voltage_12,  voltage_13,  voltage_14,  voltage_15;
float voltage_16,  voltage_17,  voltage_18,  voltage_19,  voltage_20;
float voltage_21,  voltage_22,  voltage_23,  voltage_24,  voltage_25;
float voltage_26,  voltage_27,  voltage_28,  voltage_29,  voltage_30;
float voltage_31,  voltage_32,  voltage_33,  voltage_34,  voltage_35;
float voltage_36,  voltage_37,  voltage_38,  voltage_39,  voltage_40;
float voltage_41,  voltage_42,  voltage_43,  voltage_44,  voltage_45;
float voltage_46,  voltage_47,  voltage_48,  voltage_49,  voltage_50;

float current_1,  current_2,  current_3,  current_4,  current_5;
float current_6,  current_7,  current_8,  current_9,  current_10;
float current_11,  current_12,  current_13,  current_14,  current_15;
float current_16,  current_17,  current_18,  current_19,  current_20;
float current_21,  current_22,  current_23,  current_24,  current_25;
float current_26,  current_27,  current_28,  current_29,  current_30;
float current_31,  current_32,  current_33,  current_34,  current_35;
float current_36,  current_37,  current_38,  current_39,  current_40;
//float current_41,  current_42,  current_43,  current_44,  current_45;
//float current_46,  current_47,  current_48,  current_49,  current_50;

// Pointers used only to fill/print the named variables in a loop
float* const voltPtr[50] = {
  &voltage_1, &voltage_2, &voltage_3, &voltage_4, &voltage_5,
  &voltage_6, &voltage_7, &voltage_8, &voltage_9, &voltage_10,
  &voltage_11, &voltage_12, &voltage_13, &voltage_14, &voltage_15,
  &voltage_16, &voltage_17, &voltage_18, &voltage_19, &voltage_20,
  &voltage_21, &voltage_22, &voltage_23, &voltage_24, &voltage_25,
  &voltage_26, &voltage_27, &voltage_28, &voltage_29, &voltage_10,
  &voltage_31, &voltage_32, &voltage_33, &voltage_34, &voltage_35,
  &voltage_36, &voltage_37, &voltage_38, &voltage_39, &voltage_40,
//  &voltage_41, &voltage_42, &voltage_43, &voltage_44, &voltage_45,
//  &voltage_46, &voltage_47, &voltage_48, &voltage_49, &voltage_50,
 // &voltage_51, &voltage_52, &voltage_53, &voltage_54, &voltage_55,
 // &voltage_56, &voltage_57, &voltage_58, &voltage_59, &voltage_60,
};
float* const currPtr[50] = {
  &current_1, &current_2, &current_3, &current_4, &current_5,
  &current_6, &current_7, &current_8, &current_9, &current_10,
  &current_11, &current_12, &current_13, &current_14, &current_15,
  &current_16, &current_17, &current_18, &current_19, &current_20,
  &current_21, &current_22, &current_23, &current_24, &current_25,
  &current_26, &current_27, &current_28, &current_29, &current_30,
  &current_31, &current_32, &current_33, &current_34, &current_35,
  &current_36, &current_37, &current_38, &current_39, &current_40,
//  &current_41, &current_42, &current_43, &current_44, &current_45,
//  &current_46, &current_47, &current_48, &current_49, &current_50,
 // &current_51, &current_52, &current_53, &current_54, &current_55,
 // &current_56, &current_57, &current_58, &current_59, &current_60,
};

void writeReg(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));
  Wire.write((uint8_t)(value & 0xFF));
  Wire.endTransmission();
}

uint16_t readReg(uint8_t reg) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(INA219_ADDR, (uint8_t)2);
  return ((uint16_t)Wire.read() << 8) | Wire.read();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();                 // Mega: SDA=20, SCL=21 (defaults)

  // Reset the INA219
  writeReg(REG_CONFIG, 0x8000);
  delay(10);

  /*
   * Configuration register (0x00)
   *   bit 15    : 0      no reset
   *   bits 14-13: 01     bus voltage range = 32 V   (LSB = 4 mV)
   *   bits 12-11: 00     PGA = +/-40 mV  (matches 4.8 A full scale here)
   *   bits 10-7 : 1111   bus   ADC = 12-bit, 128 samples averaged
   *   bits  6-3 : 1111   shunt ADC = 12-bit, 128 samples averaged
   *   bits  2-0 : 111    continuous shunt + bus
   */
  uint16_t config = 0;
  config |= (0x01 << 13);   // 32 V bus range
  config |= (0x00 << 11);   // +/-40 mV PGA
  config |= (0x0F << 7);    // bus  ADC : 12-bit, 128x averaging
  config |= (0x0F << 3);    // shunt ADC: 12-bit, 128x averaging
  config |= (0x07);         // continuous mode
  writeReg(REG_CONFIG, config);

  // Calibration register
  writeReg(REG_CALIBRATION, CALIBRATION);

  Serial.print(F("Calibration reg = ")); Serial.println(CALIBRATION);
  Serial.print(F("Current LSB     = ")); Serial.print(CURRENT_LSB * 1000.0f, 3);
  Serial.println(F(" mA/bit"));
  Serial.print(F("Full scale      = ")); Serial.print(CURRENT_FULL_SCALE_A, 3);
  Serial.println(F(" A"));

  delay(180);   // let one full 128x conversion complete
}
float last_01_amp = 0; // last run sample
void loop() {
  float first_01_amp = 0; // current sun sample
  // ---- 40 loops, each read is one 128-sample averaged 12-bit conversion ----
  for (uint8_t i = 0; i < 40; i++) {
    
    // Bus voltage: bits 15-3, LSB = 4 mV
    uint16_t busRaw = readReg(REG_BUS_V);
    *voltPtr[i] = (busRaw >> 3) * 0.004f;

    // Current: signed 16-bit, LSB = CURRENT_LSB
    int16_t curRaw = (int16_t)readReg(REG_CURRENT);
    *currPtr[i] = curRaw * (CURRENT_LSB * 1000.0f);   // -> mA
  }
  
  first_01_amp = *currPtr[0];   // capture AFTER writing

  // ---- delta vs previous run's sample 01 ----
  float result_delta = first_01_amp - last_01_amp;   // this run - previous run
  last_01_amp = first_01_amp;                        // now this run becomes "previous"
  // ---- report the 40 named results ----
  Serial.println(F("--- 40 averaged results (128 samples each, 12-bit) ---"));
  Serial.print(F("Result 01 delta: "));
  Serial.print(result_delta);
  Serial.println(F(" mA"));
  for (uint8_t i = 0; i < 40; i++) {
    Serial.print(F("Result "));   
    if (i < 9) {
      Serial.print("0");
    }
    Serial.print(i + 1);
    Serial.print(F(":  V-Bat = "));   Serial.print(*voltPtr[i], 4);
    Serial.print(F(" V   I-Bat = ")); Serial.print(*currPtr[i], 3);
    Serial.println(F(" mA"));
    // Serial.println(*currPtr[i], 3);
    
  }
    delay (5000);  //jjj
  // while (true) { }   //jjj run once; remove to repeat every loop()
}