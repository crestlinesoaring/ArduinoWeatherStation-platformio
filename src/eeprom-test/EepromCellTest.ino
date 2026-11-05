/*
 * One-shot internal EEPROM cell test (destructive — run only on lab boards).
 * pio run -e <lab-env> -t eeprom-test --upload-port COMx
 *
 * For each byte: write 0xFF, 0x00, 0x55, 0xAA, restore 0xFF. Reports mismatches on Serial.
 */

#include <avr/eeprom.h>

#ifndef EEPROM_TEST_LAST_ADDR
#define EEPROM_TEST_LAST_ADDR 4095
#endif

static uint8_t readCell(uint16_t addr) {
  return eeprom_read_byte((const uint8_t*)(uintptr_t)addr);
}

static bool writeRead(uint16_t addr, uint8_t value, uint8_t* got) {
  eeprom_write_byte((uint8_t*)(uintptr_t)addr, value);
  *got = readCell(addr);
  return *got == value;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.print(F("[eeprom-test] scanning 0.."));
  Serial.println(EEPROM_TEST_LAST_ADDR);

  uint16_t failCount = 0;
  for (uint16_t addr = 0; addr <= EEPROM_TEST_LAST_ADDR; addr++) {
    uint8_t got = 0;
    const uint8_t patterns[] = {0xFF, 0x00, 0x55, 0xAA, 0xFF};
    for (uint8_t i = 0; i < sizeof(patterns); i++) {
      if (!writeRead(addr, patterns[i], &got)) {
        failCount++;
        Serial.print(F("[eeprom-test] FAIL addr="));
        Serial.print(addr);
        Serial.print(F(" wrote=0x"));
        if (patterns[i] < 16) {
          Serial.print('0');
        }
        Serial.print(patterns[i], HEX);
        Serial.print(F(" read=0x"));
        if (got < 16) {
          Serial.print('0');
        }
        Serial.println(got, HEX);
        break;
      }
    }
    if ((addr & 0xFF) == 0 && addr != 0) {
      Serial.print(F("[eeprom-test] progress addr="));
      Serial.println(addr);
    }
  }

  if (failCount == 0) {
    Serial.println(F("[eeprom-test] PASS all cells"));
  } else {
    Serial.print(F("[eeprom-test] FAIL "));
    Serial.print(failCount);
    Serial.println(F(" cell(s) — suspect worn or faulty EEPROM"));
  }
}

void loop() {
  delay(1000);
}
